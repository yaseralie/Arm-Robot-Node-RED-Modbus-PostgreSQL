/*
  By Yaser Ali Husen
  GPIO Reference: https://randomnerdtutorials.com/esp8266-pinout-reference-gpios/
  Library: https://github.com/emelianov/modbus-esp8266

  Wiring:
  PCA       ESP8266
  VCC       3v
  GND       GND
  SCL       D1
  SDA       D2
  Include the HCPCA9685 library

  Wiring W5500:
  W5500     ESP8266
  VCC       5v
  GND       GND
  RST       RST
  INT       EN
  MISO      D6
  MOSI      D7
  SCS       D4 (Default is D2, but it used by PCA for SCL I2C)
  SCLK      D5

  Input from relay conveyor when detec object
  D3      Ground as common input

*/

#include "Arduino.h"
//#include <ModbusIP_ESP8266.h>
#include <ArduinoJson.h>

#include <SPI.h>
#include <Ethernet.h>       // Ethernet library v2 is required
#include <ModbusEthernet.h>

//For servo library=============
// You can use scan I2C to find I2C address
#include "HCPCA9685.h"
#define  I2CAdd 0x40
HCPCA9685 HCPCA9685(I2CAdd);
//For servo library=============

// Modbus Configuration
// Enter a MAC address and IP address for your controller below.
byte mac[] = {
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED
};
IPAddress ip(192, 168, 0, 213); // The IP address will be dependent on your local network:
ModbusEthernet mb;              // Declare ModbusTCP instance


//Variables
int x_pos;       // variable to store the servo position
int y_pos;       // variable to store the servo position
int z1_pos;      // variable to store the servo position
int z2_pos;      // variable to store the servo position
int rotate_pos;  // variable to store the servo position
int grip_pos;    // variable to store the servo position

int x_target;
int y_target;
int z1_target;
int z2_target;
int rotate_target;
int grip_target;
int speed_delay;

//variable for button manual move
bool x1_button = false;
bool x2_button = false;
bool y1_button = false;
bool y2_button = false;
bool z1_1_button = false;
bool z1_2_button = false;
bool z2_1_button = false;
bool z2_2_button = false;
bool rotate1_button = false;
bool rotate2_button = false;
bool grip1_button = false;
bool grip2_button = false;

int delay_time = 15;
bool status_manual = false;

const int input_pin = 0; //D3
bool status_input = false;

StaticJsonDocument<200> jsonDocument;

void setup() {
  Serial.begin(9600);
  // Modbus Setup
  Ethernet.init(2);        // SS pin, default is 5
  Ethernet.begin(mac, ip);  // start the Ethernet connection
  delay(1000);              // give the Ethernet shield a second to initialize

  mb.server();

  //configure holding register for Servo Position
  mb.addHreg(0);  //X-Servo
  mb.addHreg(1);  //Y-Servo
  mb.addHreg(2);  //Z1-Servo
  mb.addHreg(3);  //Z2-Servo
  mb.addHreg(4);  //Rotate-Servo
  mb.addHreg(5);  //Gripper-Servo
  mb.addHreg(6);  //Delay

  //configure coil register for manual button (joystick)
  mb.addCoil(0);  //x1_button
  mb.addCoil(1);  //x2_button
  mb.addCoil(2);  //y1_button
  mb.addCoil(3);  //y2_button
  mb.addCoil(4);  //z1_1_button
  mb.addCoil(5);  //z1_2_button
  mb.addCoil(6);  //z2_1_button
  mb.addCoil(7);  //z2_2_button
  mb.addCoil(8);  //rotate1_button
  mb.addCoil(9);  //rotate2_button
  mb.addCoil(10);  //grip1_button
  mb.addCoil(11);  //grip2_button
  mb.addCoil(12);  //input conveyor (true if conveyor stop/object detected)
  Serial.println("Modbus Configuration Done!");

  //Initial Servo
  HCPCA9685.Init(SERVO_MODE);
  /* Wake the device up */
  HCPCA9685.Sleep(false);
  //origin position
  x_pos = 238;
  y_pos = 120;
  z1_pos = 120;
  z2_pos = 400;
  rotate_pos = 422;
  grip_pos = 200;

  HCPCA9685.Servo(0, x_pos);
  HCPCA9685.Servo(1, y_pos);
  HCPCA9685.Servo(2, z1_pos);
  HCPCA9685.Servo(3, z2_pos);
  HCPCA9685.Servo(4, rotate_pos);
  HCPCA9685.Servo(5, grip_pos);

  //Initial store register
  mb.Hreg(0, x_pos);
  mb.Hreg(1, y_pos);
  mb.Hreg(2, z1_pos);
  mb.Hreg(3, z2_pos);
  mb.Hreg(4, rotate_pos);
  mb.Hreg(5, grip_pos);
  mb.Hreg(6, 10);
  Serial.print("Store to Modbus Done!");

  delay(5000);
  pinMode(input_pin, INPUT_PULLUP);
  delay(2000);
}

void loop() {
  //Call once inside loop()
  mb.task();
  delay(10);
  //=====For Checking Input Conveyor============
  int buttonState = digitalRead(input_pin);  // read input
  if (buttonState == LOW && status_input == false) {
    mb.Coil(12,1);
    status_input = true;
    //Serial.println("Conveyor Stop!");
  } 
  if (buttonState == HIGH && status_input == true) {
    mb.Coil(12,0);
    status_input = false;
    //Serial.println("Conveyor Run!");  
  }
  //=====For Checking Input Conveyor============
  
  //=====================================For Coil Register================================
  int registers2[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
  const char* registerNames2[] = {"x1_button", "x2_button", "y1_button", "y2_button", "z1_1_button", "z1_2_button", "z2_1_button", "z2_2_button", "rotate1_button", "rotate2_button", "grip1_button", "grip2_button"};
  int numRegisters = 12;

  //clear previous data
  jsonDocument.clear();

  //create objek JSON
  JsonObject data = jsonDocument.to<JsonObject>();

  //read data and store to JSON
  for (int i = 0; i < numRegisters; i++) {
    int value = mb.Coil(registers2[i]);
    data[registerNames2[i]] = value;
    registers2[i] = value;
  }

  //print into serial
  //serializeJson(data, Serial);
  //Serial.println(); // Tambahkan newline untuk memisahkan setiap iterasi

  //get value
  x1_button = registers2[0];
  x2_button = registers2[1];
  y1_button = registers2[2];
  y2_button = registers2[3];
  z1_1_button = registers2[4];
  z1_2_button = registers2[5];
  z2_1_button = registers2[6];
  z2_2_button = registers2[7];
  rotate1_button = registers2[8];
  rotate2_button = registers2[9];
  grip1_button = registers2[10];
  grip2_button = registers2[11];
  //=====================================For Coil Register================================

  if (x1_button == 1 || x2_button == 1 || y1_button == 1 || y2_button == 1 || z1_1_button == 1 || z1_2_button == 1 ||
      z2_1_button == 1 || z2_2_button == 1 || rotate1_button == 1 || rotate2_button == 1 || grip1_button == 1 || grip2_button == 1) {
    status_manual = true;
    move_manual();
  }

  if (x1_button == 0 && x2_button == 0 && y1_button == 0 && y2_button == 0 && z1_1_button == 0 && z1_2_button == 0 &&
      z2_1_button == 0 && z2_2_button == 0 && rotate1_button == 0 && rotate2_button == 0 && grip1_button == 0 && grip2_button == 0) {
    if (status_manual == true) {
      status_manual = false;
      record_position();
    }
    //===================================For Holding Register===============================
    //read modbus holding register as target position
    int registers[] = {0, 1, 2, 3, 4, 5, 6};
    const char* registerNames[] = {"x_target", "y_target", "z1_target", "z2_target", "rotate_target", "grip_target", "speed_delay"};
    numRegisters = 7;

    //clear previous data
    jsonDocument.clear();

    //read data and store to JSON
    for (int i = 0; i < numRegisters; i++) {
      int value = mb.Hreg(registers[i]);
      data[registerNames[i]] = value;
      registers[i] = value;
    }

    //print into serial
    //serializeJson(data, Serial);
    //Serial.println(); // Tambahkan newline untuk memisahkan setiap iterasi

    //get value
    x_target = registers[0];
    y_target = registers[1];
    z1_target = registers[2];
    z2_target = registers[3];
    rotate_target = registers[4];
    grip_target = registers[5];
    speed_delay = registers[6];
    //===================================For Holding Register===============================
    move_position();
  }

  //add delay
  delay(10);
}

void move_position() {
  //Move
  while (x_pos != x_target || y_pos != y_target || z1_pos != z1_target ||
         z2_pos != z2_target || rotate_pos != rotate_target || grip_pos != grip_target) {
    if (x_pos < x_target) {
      x_pos = min(x_pos + 2, x_target);
    } else if (x_pos > x_target) {
      x_pos = max(x_pos - 2, x_target);
    }
    HCPCA9685.Servo(0, x_pos);

    if (y_pos < y_target) {
      y_pos = min(y_pos + 2, y_target);
    } else if (y_pos > y_target) {
      y_pos = max(y_pos - 2, y_target);
    }
    HCPCA9685.Servo(1, y_pos);

    if (z1_pos < z1_target) {
      z1_pos = min(z1_pos + 2, z1_target);
    } else if (z1_pos > z1_target) {
      z1_pos = max(z1_pos - 2, z1_target);
    }
    HCPCA9685.Servo(2, z1_pos);

    if (z2_pos < z2_target) {
      z2_pos = min(z2_pos + 2, z2_target);
    } else if (z2_pos > z2_target) {
      z2_pos = max(z2_pos - 2, z2_target);
    }
    HCPCA9685.Servo(3, z2_pos);

    if (rotate_pos < rotate_target) {
      rotate_pos = min(rotate_pos + 2, rotate_target);
    } else if (rotate_pos > rotate_target) {
      rotate_pos = max(rotate_pos - 2, rotate_target);
    }
    HCPCA9685.Servo(4, rotate_pos);

    if (grip_pos < grip_target) {
      grip_pos = min(grip_pos + 2, grip_target);
    } else if (grip_pos > grip_target) {
      grip_pos = max(grip_pos - 2, grip_target);
    }
    HCPCA9685.Servo(5, grip_pos);
    delay(speed_delay);
  }
}

void move_manual() {
  delay(delay_time);
  //Button X***************************
  if (x1_button == 1)
  {
    if (x_pos < 450) {
      x_pos = x_pos + 2;
      HCPCA9685.Servo(0, x_pos);
    }
  }
  if (x2_button == 1)
  {
    if (x_pos > 0) {
      x_pos = x_pos - 2;
      HCPCA9685.Servo(0, x_pos);
    }
  }
  //Button X***************************
  //Button Y***************************
  if (y1_button == 1)
  {
    if (y_pos < 450) {
      y_pos = y_pos + 2;
      HCPCA9685.Servo(1, y_pos);
    }
  }
  if (y2_button == 1)
  {
    if (y_pos > 0) {
      y_pos = y_pos - 2;
      HCPCA9685.Servo(1, y_pos);
    }
  }
  //Button Y***************************
  //Button Z1**************************
  if (z1_1_button == 1)
  {
    if (z1_pos > 0) {
      z1_pos = z1_pos - 2;
      HCPCA9685.Servo(2, z1_pos);
    }

  }
  if (z1_2_button == 1)
  {
    if (z1_pos < 450) {
      z1_pos = z1_pos + 2;
      HCPCA9685.Servo(2, z1_pos);
    }
  }
  //Button Z1**************************
  //Button Z2**************************
  if (z2_1_button == 1)
  {
    if (z2_pos < 450) {
      z2_pos = z2_pos + 2;
      HCPCA9685.Servo(3, z2_pos);
    }
  }
  if (z2_2_button == 1)
  {
    if (z2_pos > 0) {
      z2_pos = z2_pos - 2;
      HCPCA9685.Servo(3, z2_pos);
    }
  }
  //Button Z2**************************
  //Button Rotate**********************
  if (rotate1_button == 1)
  {
    if (rotate_pos < 450) {
      rotate_pos = rotate_pos + 2;
      HCPCA9685.Servo(4, rotate_pos);
    }
  }
  if (rotate2_button == 1)
  {
    if (rotate_pos > 0) {
      rotate_pos = rotate_pos - 2;
      HCPCA9685.Servo(4, rotate_pos);
    }
  }
  //Button Rotate***********************
  //Button Grip*************************
  if (grip1_button == 1)
  {
    if (grip_pos < 220) {
      grip_pos = grip_pos + 2;
      HCPCA9685.Servo(5, grip_pos);
    }
  }
  if (grip2_button == 1)
  {
    if (grip_pos > 100) {
      grip_pos = grip_pos - 2;
      HCPCA9685.Servo(5, grip_pos);
    }
  }
  //Button Grip*************************
}

void record_position() {
  mb.Hreg(0, x_pos);
  mb.Hreg(1, y_pos);
  mb.Hreg(2, z1_pos);
  mb.Hreg(3, z2_pos);
  mb.Hreg(4, rotate_pos);
  mb.Hreg(5, grip_pos);
  Serial.println("Record Servo Position:");
}
