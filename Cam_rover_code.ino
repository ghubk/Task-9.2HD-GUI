

#include <PubSubClient.h>
#include "esp_camera.h" //esp camera library
#include <WiFi.h> //esp8266 wifi library

// WARNING!!! Make sure that you have either selected ESP32 Wrover Module,
//            or another board which has PSRAM enabled
//
// Adafruit ESP32 Feather
// Select camera model
//#define CAMERA_MODEL_WROVER_KIT
//#define CAMERA_MODEL_M5STACK_PSRAM
#define CAMERA_MODEL_AI_THINKER // The camera model im using in espressif same as AI thinker model

//Add your wifi information. If the wifi speed is not good there maybe delays in movement and camera feed.

const char* ssid = "Kushan";   //Enter SSID WIFI Name
const char* password = "hotspotinfo";   //Enter WIFI Password

const char* mqtt_broker = "broker.emqx.io"; //declaring the settings for mqtt broker
const char* topic = "GreenErover"; //declaring the subscription and publishing topic. 
const char* mqtt_username = "eqmx"; // the username for the mqtt broker
const char* mqtt_password = "public"; //password for the broker server //a public free server is used for this. There are options to use a private server as well.
const int mqtt_port = 1883;  // tcp port details.



#if defined(CAMERA_MODEL_WROVER_KIT) //these codes will only work if the WRover esp32 cam is used.
#define PWDN_GPIO_NUM    -1
#define RESET_GPIO_NUM   -1
#define XCLK_GPIO_NUM    21
#define SIOD_GPIO_NUM    26
#define SIOC_GPIO_NUM    27s

#define Y9_GPIO_NUM      35
#define Y8_GPIO_NUM      34
#define Y7_GPIO_NUM      39
#define Y6_GPIO_NUM      36
#define Y5_GPIO_NUM      19
#define Y4_GPIO_NUM      18
#define Y3_GPIO_NUM       5
#define Y2_GPIO_NUM       4
#define VSYNC_GPIO_NUM   25
#define HREF_GPIO_NUM    23
#define PCLK_GPIO_NUM    22


#elif defined(CAMERA_MODEL_AI_THINKER)  //setting the pins on the esp32 cam board
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

#else
#error "Camera model not selected"
#endif

// GPIO Setting
extern int gpLb =  2; // Left 1  
extern int gpLf = 14; // Left 2
extern int gpRb = 15; // Right 1
extern int gpRf = 13; // Right 2
extern int gpLed =  4; // Light
extern String WiFiAddr ="";

extern int buzz = 16; // buzzer


void startCameraServer(); //this function will start the wifi camera server


WiFiClient espClient;  //wifi client variable declaration
PubSubClient client(espClient); // declare the client variable to pubsubclient 




void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();


  pinMode(gpLb, OUTPUT); //Left Backward
  pinMode(gpLf, OUTPUT); //Left Forward
  pinMode(gpRb, OUTPUT); //Right Forward
  pinMode(gpRf, OUTPUT); //Right Backward
  pinMode(gpLed, OUTPUT); //Light

  pinMode(buzz, OUTPUT);
  digitalWrite(buzz, LOW);

  //initialize the wheels to be at LOW so the rover does not move
  digitalWrite(gpLb, LOW);
  digitalWrite(gpLf, LOW);
  digitalWrite(gpRb, LOW);
  digitalWrite(gpRf, LOW);
  digitalWrite(gpLed, LOW);

  camera_config_t config;  //Camera configuration 
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  //init with high specs to pre-allocate larger buffers
  if(psramFound()){
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  // Intialize the camera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  //drop down frame size for higher initial frame rate
  sensor_t * s = esp_camera_sensor_get();
  s->set_framesize(s, FRAMESIZE_CIF);

  WiFi.begin(ssid, password); //begin connecting to the wifi network

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");

  startCameraServer(); //Live feed 

  Serial.print("Camera Ready! Use 'http://"); //prints the information of the esp32 cam ip address. 
  Serial.print(WiFi.localIP());
  WiFiAddr = WiFi.localIP().toString();
  Serial.println("' to connect");




  client.setServer(mqtt_broker, mqtt_port); //client server intialization
  client.setCallback(callback); // setcallback funtion used to check if the the function works

  while(!client.connected())
  {
    String client_id = "esp8266-client-";
    client_id += String(WiFi.macAddress()); 
    Serial.printf("The client %s connects to the public mqtt broker\n", client_id.c_str());
    
    if(client.connect(client_id.c_str(), mqtt_username, mqtt_password))
    {
      Serial.println("Public emqx mqtt broker connected");

    }else
    {
      Serial.print("Failed with state");
      Serial.print(client.state());
      delay(2000);

    }


  }


  client.publish(topic, "This is a msg published by the surveilence rover."); // publishing from the arduino 
  client.subscribe(topic); //subscribing to a topic in the mqtt tool mqttx app


}

void loop() 
{
  
  client.loop();

}

void callback(char *topic, byte *payload, unsigned int length)
{
  //serial print msgs to see if the fucntions work
  Serial.print("subscribed topic received: ");
  Serial.println(topic);
  Serial.print("Message: ");

//this loop with convert the payload data to characters
  for (int i=0; i<length; i++)
  {
    Serial.print((char) payload[i]);

  }

  //the function to make the wheel move once the topic is received. 
  Serial.println(" ");
  Serial.println(" subscribe message has been received and wheels will move");
  Wheelmove(HIGH, LOW, HIGH, LOW);
  digitalWrite(gpLed, HIGH);
  delay(2000);
  digitalWrite(gpLed, LOW);
  Wheelmove(LOW,LOW,LOW,LOW);


  Serial.println();
  Serial.println("--------------------");


}


//wheelmove function
void Wheelmove(int nLf, int nLb, int nRf, int nRb)
{
 digitalWrite(gpLf, nLf);
 digitalWrite(gpLb, nLb);
 digitalWrite(gpRf, nRf);
 digitalWrite(gpRb, nRb);
}
