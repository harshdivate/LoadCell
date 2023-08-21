#include <SPI.h>
#include <MFRC522.h>
#include <ArduinoJson.h>
#include "WiFi.h"
#include <PubSubClient.h>
#include <Arduino.h>
#include "HX711.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

//Bridge Variable Declaration
#define motor1_pin1 02
#define motor1_pin2 04
#define motor2_pin1 26
#define motor2_pin2 27


#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

//Load cell wiring
const int LOADCELL_DOUT_PIN = 17;
const int LOADCELL_SCK_PIN = 16;

// int rbutton = ;//insert reset pin here

//MRC522 wiring
#define SS_PIN 5
#define RST_PIN 0

//Calibration factor for load cell
float calibration =120090;
long reading;

HX711 scale;
 
MFRC522 rfid(SS_PIN, RST_PIN);

MFRC522::MIFARE_Key key; 


//byte dataBuffer[18];
//byte bufferSize=sizeof(dataBuffer);

byte nuidPICC[4];
//Variable for mac address
uint8_t mac[6];
char macStr[18]; 

// WiFi credentials
const char* ssid = "narzo 50A";
const char* password = "123456789";

// MQTT broker configuration
const char* mqttBroker = "thingsboard.cloud";
const int mqttPort = 1883;
const char* mqttClientId = "Weight";
const char* deviceAccessToken="Qwert67890$";
const char* mqttTopic = "v1/devices/me/telemetry";

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

void setup() { 
  Serial.begin(9600);

  //Both Gates Closed   
  pinMode(motor1_pin1,OUTPUT);
  pinMode(motor1_pin2,OUTPUT);
  pinMode(motor2_pin1,OUTPUT);
  pinMode(motor2_pin2,OUTPUT);
  digitalWrite(motor1_pin1,LOW);
  digitalWrite(motor1_pin2,LOW);
  digitalWrite(motor2_pin1,LOW);
  digitalWrite(motor2_pin2,LOW);

  //Oled Display Connection
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3D)) { 
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  delay(2000);
  display.clearDisplay();
  
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);

    
  //Initialize the HX711 scale
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN); 
  scale.set_scale(calibration);
  scale.tare();
  SPI.begin();
  rfid.PCD_Init();
  WiFi.begin(ssid, password);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
  Serial.println(WiFi.localIP());
  WiFi.macAddress(mac);
  // Convert MAC address to a human-readable string
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  // Connect to MQTT broker
  mqttClient.setServer(mqttBroker, mqttPort);

  // Wait for MQTT connection
  while (!mqttClient.connected()) {
    if (mqttClient.connect(mqttClientId,deviceAccessToken,"")) {
      Serial.println("Connected to MQTT broker");
    } else {
      Serial.print("Failed to connect to MQTT broker, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" Retrying...");
      delay(2000);
    }
  } 

  for (byte i = 0; i < 6; i++) {
    key.keyByte[i] = 0xFF;
  }

  // Serial.println(F("This code scan the MIFARE Classsic NUID."));
  // Serial.print(F("Using the following key:"));
}

void reconnect() {
  while (!mqttClient.connected()) {
    if (mqttClient.connect(mqttClientId,deviceAccessToken,"")) {
      Serial.println("Connected to MQTT broker");
    } else {
      Serial.print("Failed to connect to MQTT broker, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" Retrying...");
      delay(2000);
    }
  }
}

void loop() {
  // Reset the scale to 0
  // if (digitalRead(rbutton) == LOW) {
  //   scale.set_scale();
  //   scale.tare(); 
  // }

  // Maintain MQTT connection
  if(!mqttClient.connected()) {
    reconnect();
  }
  mqttClient.loop();
  
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()){

       //New Card Detection
       if (rfid.uid.uidByte[0] != nuidPICC[0] || 
        rfid.uid.uidByte[1] != nuidPICC[1] || 
        rfid.uid.uidByte[2] != nuidPICC[2] || 
        rfid.uid.uidByte[3] != nuidPICC[3] ) {
          Serial.println(F("A new card has been detected."));


        //Reads the card Type 
        Serial.print("Data stored on the card: ");
       // Serial.println((char *)dataBuffer);
        Serial.print(F("PICC type: "));
        MFRC522::PICC_Type piccType = rfid.PICC_GetType(rfid.uid.sak);
        Serial.println(rfid.PICC_GetTypeName(piccType));
         
        // Check is the PICC of Classic MIFARE type
        if (piccType != MFRC522::PICC_TYPE_MIFARE_MINI &&  piccType != MFRC522::PICC_TYPE_MIFARE_1K && piccType != MFRC522::PICC_TYPE_MIFARE_4K) {
          Serial.println(F("Your tag is not of type MIFARE Classic."));
          return;
        }

          // Store NUID into nuidPICC array
          for (byte i = 0; i < 4; i++) {
            nuidPICC[i] = rfid.uid.uidByte[i];
          }

          //Front Gate Opens Now
          String message="OPENING";
          printOled(message);
          display.clearDisplay();

          //Opening Front Door
          openFrontDoor();

          //Weighing Start
          if (scale.is_ready()) {
                reading=takeReading();

                
                
                
                //Print the weight on 
                String reading_str=String(reading);
                printOled(reading_str);
                // Clear Display
                display.clearDisplay();
                


 
          // Publish weight to MQTT broker
              publishMQTTData();

            // Print to Oled    
              printOled("CLOSING");

              display.clearDisplay();

              //Closing the Back Door 
             closeBackDoor();

          // Halt PICC
          rfid.PICC_HaltA();

          // Stop encryption on PCD
          rfid.PCD_StopCrypto1();


      }
      else {
      Serial.println("Scale not found.");
    }
}
  else{
          Serial.println(F("Card read previously."));
          delay(2000);
          }
} 
else{
      //Print on Oled Waiting      
      display.print("Waiting");
      display.display();
      delay(500);
      display.clearDisplay();
      display.setCursor(0,0);
  }    
}

String readRFID(){
  String uidDecimal = ""; 

    for (byte i = 0; i < rfid.uid.size; i++) {
      if (i > 0) uidDecimal += " "; 
      uidDecimal += String(rfid.uid.uidByte[i], DEC);
    }
    
    rfid.PICC_HaltA();
    return uidDecimal;
}

void printOled(String message){
          display.print(message);
          display.setCursor(0,0);
          delay(500);
          display.display();
}


void openFrontDoor(){
          digitalWrite(motor1_pin1,LOW);
          digitalWrite(motor1_pin2,HIGH);
          delay(3000);
          digitalWrite(motor1_pin1,LOW);
          digitalWrite(motor1_pin2,LOW);
          //Now Close the opened Door
          delay(3000);
          digitalWrite(motor1_pin1,HIGH);
          digitalWrite(motor1_pin2,LOW);
          //Now Stop
          delay(3000);
          digitalWrite(motor1_pin1,LOW);
          digitalWrite(motor1_pin2,LOW);
}

long takeReading(){
        scale.set_scale(calibration);    
        delay(2000);
        Serial.print("Place a known weight on the scale...");
        delay(2000);
        reading=scale.get_units(3);
        Serial.print("Result: ");
        Serial.println(reading);
    return reading;
}

void closeBackDoor(){
    digitalWrite(motor2_pin1,LOW);
    digitalWrite(motor2_pin2,HIGH);
    delay(3000);
    digitalWrite(motor2_pin1,LOW);
    digitalWrite(motor2_pin2,LOW);
    //Now Close the opened Door
    delay(3000);
    digitalWrite(motor2_pin1,HIGH);
    digitalWrite(motor2_pin2,LOW);
    //Now Stop
    delay(3000);
    digitalWrite(motor2_pin1,LOW);
    digitalWrite(motor2_pin2,LOW);

}


void publishMQTTData(){
    char message[10];
    StaticJsonDocument<200> jsonDocument;
    jsonDocument["mac_address"]=macStr;
    jsonDocument["weight"]=reading;
    jsonDocument["a_id"]= readRFID();
    String jsonString ;
    serializeJson(jsonDocument,jsonString);
    mqttClient.publish(mqttTopic, jsonString.c_str());
    Serial.println("PUBLISHED");
}
// String printDec(byte *buffer, byte bufferSize) {
//   String result = ""; // Initialize an empty String to hold the result

//   for (byte i = 0; i < bufferSize; i++) {
//     result += (buffer[i] < 0x10 ? " 0" : " "); // Add leading space and '0' for single-digit numbers
//     result += String(buffer[i], DEC); // Convert the current element to a decimal String and append it
//   }

//   return result; // Return the accumulated String
// }