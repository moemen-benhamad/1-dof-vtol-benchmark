#include <Arduino.h>
#include <WiFi.h>
#include "Wire.h"
#include <ArduinoJson.h>
#include <MPU6050_light.h>
#include <esp_wifi.h>

//#define STATIC_IP

WiFiServer server(8000);

const char* ssid = "";
const char* password = "";
const char* hostname = "esp32";


int motor1Pin = 14; // PNP transistor base pin for motor 1
int motor2Pin = 27; // PNP transistor base pin for motor 2
int motor1Speed = 255; // initial speed for motor 1
int motor2Speed = 255; // initial speed for motor 2

int pwmFrequency = 5000; // PWM frequency in Hz
int pwmResolution = 8; // PWM resolution in bits

int motor1Channel = 0; // PWM channel for motor 1
int motor2Channel = 1; // PWM channel for motor 2

#ifdef STATIC_IP
// Set your Static IP address
IPAddress local_IP(192, 168, 137, 100);
// Set your Gateway IP address
IPAddress gateway(192, 168, 137, 1);
IPAddress subnet(255, 255, 0, 0);
#endif

MPU6050 mpu(Wire);

void initialise_MPU6050();
void connect2Wifi();
void setup_motors();
void labview_server_read_task(void*);
void labview_server_write_task(void*);
void signal_ready();

void setup() {
  Serial.begin(115200);
  WiFi.setHostname(hostname);
  Wire.begin();
  esp_wifi_set_ps(WIFI_PS_NONE);

  connect2Wifi();
  setup_motors();
  delay(50);
  initialise_MPU6050();
  server.begin();

  signal_ready();

  // * tache 1
  xTaskCreate(
    labview_server_read_task,
    "labview_read",
    20000,
    NULL,
    0,
    NULL
  );

  // ** tache 2
  xTaskCreate(
    labview_server_write_task,
    "labview_write",
    20000,
    NULL,
    0,
    NULL
  );
}

void loop() {
  vTaskSuspend(NULL);
}

void labview_server_read_task(void * pvParameters){
  while(1){
    // Wait for a new client to connect
    WiFiClient client = server.available();
    if (!client) {
      vTaskDelay(10 / portTICK_PERIOD_MS);
      continue;
    }

    // Read the incoming message from the client
    String jsonString="";
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        jsonString += c;
        if (c == '}') {
          break;
        }
      }
    }

    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, jsonString);
    if (error) {
      Serial.println("Error deserializing JSON");
    }
    else {
      int m1Speed = doc["M1"];
      int m2Speed = doc["M2"];
      motor1Speed = m1Speed;
      motor2Speed = m2Speed;
      Serial.println("Motor 1 speed: " + String(motor1Speed));
      Serial.println("Motor 2 speed: " + String(motor2Speed));
      ledcWrite(motor1Channel, motor1Speed);
      ledcWrite(motor2Channel, motor2Speed);
    }
  }
}

void labview_server_write_task(void * pvParameters){
  float roll = 0.0;
  WiFiClient client; 
  while(1){
    client = server.available();
    if (client){
      if (client.connected())
        Serial.println("New client connected");
      while(client.connected()){
      //Get new sensor events with the readings
      mpu.update();
      roll = mpu.getAngleX();
      // Create a JSON object to send to the client
      StaticJsonDocument<256> jsonDoc;
      jsonDoc["roll"] = roll;
      String jsonString;
      serializeJson(jsonDoc, jsonString);
      jsonString += "\n";
      //if (client.connected())
        client.println(jsonString);
      vTaskDelay(40 / portTICK_PERIOD_MS);
      } 
    }
  }
}

void initialise_MPU6050(){
  byte status = mpu.begin();
  if(status!=0) {
    Serial.println("MPU6050 not found");
    while(status!=0);
  }
  Serial.println("MPU6050 found");

  Serial.println(F("Calculating offsets, do not move MPU6050"));
  delay(1000);
  mpu.calcOffsets(); // gyro and accelero
  Serial.println("Done!\n");
}

void connect2Wifi(){
  #ifdef STATIC_IP
  // Configures static IP address
  if (!WiFi.config(local_IP, gateway, subnet)) {
    Serial.println("STA Failed to configure");
  }
  #endif

  // Connect to Wi-Fi network with SSID and password
  Serial.print("Connecting to ");
  Serial.print(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("Connected to WiFi");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void setup_motors(){
  pinMode(motor1Pin, OUTPUT);
  pinMode(motor2Pin, OUTPUT);

  ledcSetup(motor1Channel, pwmFrequency, pwmResolution);
  ledcSetup(motor2Channel, pwmFrequency, pwmResolution);
  ledcAttachPin(motor1Pin, motor1Channel);
  ledcAttachPin(motor2Pin, motor2Channel);

  ledcWrite(motor1Channel, motor1Speed);
  ledcWrite(motor2Channel, motor2Speed);
}

void signal_ready(){
  ledcWrite(motor1Channel, 200);
  ledcWrite(motor2Channel, 255);
  vTaskDelay(pdMS_TO_TICKS(500));
  ledcWrite(motor1Channel, 255);
  ledcWrite(motor2Channel, 200);
  vTaskDelay(pdMS_TO_TICKS(500));
  ledcWrite(motor1Channel, 255);
  ledcWrite(motor2Channel, 255);

}
