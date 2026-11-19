#include <Arduino.h>
#include <BLEGamepadClient.h>
#include <Stepper.h>
#include <ESP32Servo.h>

#include "esp_attr.h"

// macros for stepper motor pins
#define IN1 15
#define IN2 16
#define IN3 17
#define IN4 18

#define LASER_PIN 26

// LED pin and blink times
#define LED_PIN 25
#define BLINK_SLOW 800
#define BLINK_FAST 300

#define SERVO_PWM 27

#define STEPPER_SPEED 10
#define STEPS_PER_LOOP 3

#define SERVO_INCREMENT 1

#define ANALOG_STICK_THRESHOLD 0.5

TaskHandle_t bluetoothHandle;
TaskHandle_t ledHandle;

// 0 -> LED is off, 1 -> LED is on 
volatile int ledState = 0;
// LED task gets suspended if not needed
volatile bool ledSuspended = false;

volatile int laserState = 0;

XboxController controller;

// state machine for bluetooth controller conectivity
enum class ConnectionState {
  NoController,
  Syncing,
  Connected
};

// initialize to no connection
volatile ConnectionState connectionState = ConnectionState::NoController;

// holds all of the controller inputs
XboxControlsState xState;

// servo motor position
int servoPos = 0;

// steps per rotation on the Stepper Motor
const int stepsPerRevolution = 2038;

// Creates an instance of stepper class
Stepper myStepper = Stepper(stepsPerRevolution, IN1, IN3, IN2, IN4);
Servo myServo;

void updateMotors(void *parameter) {
  while (true) {
    float x = xState.rightStickX;
    float y = xState.rightStickY;

    // Servo
    if (y < -ANALOG_STICK_THRESHOLD) {
      servoPos = max(0, servoPos - SERVO_INCREMENT);
      myServo.write(servoPos);
    }
    else if (y > ANALOG_STICK_THRESHOLD) {
      servoPos = min(180, servoPos + SERVO_INCREMENT);
      myServo.write(servoPos);
    }

    // Stepper
    if (x < -ANALOG_STICK_THRESHOLD) {
      myStepper.setSpeed(STEPPER_SPEED);
      myStepper.step(-1);
    }
    else if (x > ANALOG_STICK_THRESHOLD) {
      myStepper.setSpeed(STEPPER_SPEED);
      myStepper.step(1);
    }

    vTaskDelay(pdMS_TO_TICKS(2));
  }
}
void updateLED(void *parameter) {
  while (1) {
    switch (connectionState) {
      case ConnectionState::NoController:
        ledState = 0;
        digitalWrite(LED_PIN, HIGH);
        vTaskDelay(BLINK_SLOW / portTICK_PERIOD_MS);
        digitalWrite(LED_PIN, LOW);
        vTaskDelay(BLINK_SLOW / portTICK_PERIOD_MS);
        break;

      case ConnectionState::Syncing:
        ledState = 0;
        digitalWrite(LED_PIN, HIGH);
        vTaskDelay(BLINK_FAST / portTICK_PERIOD_MS);
        digitalWrite(LED_PIN, LOW);
        vTaskDelay(BLINK_FAST / portTICK_PERIOD_MS);
        break;

      case ConnectionState::Connected:
        if (ledState == 0) {
          digitalWrite(LED_PIN, HIGH);
          ledState = 1;
        }
        break;
    }

  }
}

void readBtController(void *parameter) {
  while (1) {
    //Serial.println("bt loop");
    if (controller.isConnected()) {
      connectionState = ConnectionState::Connected;
      if (digitalRead(LED_PIN) == 1) {
        vTaskSuspend(ledHandle);
        ledSuspended = true;
      }
      controller.read(&xState);

			// Right trigger ranges from 0.0 to 1.0
      bool triggerHeld = xState.rightTrigger > 0.15f;

      digitalWrite(LASER_PIN, triggerHeld ? HIGH : LOW);
      laserState = triggerHeld;


    } else if (controller.isConnecting()) {
      connectionState = ConnectionState::Syncing;
			digitalWrite(LASER_PIN, LOW);
      laserState = 0;
    } else {
			digitalWrite(LASER_PIN, LOW);
      laserState = 0;
      if (ledSuspended) {
        vTaskResume(ledHandle);
        ledSuspended = false;
      }
      connectionState = ConnectionState::NoController;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void setup() {
	Serial.begin(115200);
	controller.begin();

	pinMode(LED_PIN, OUTPUT);
	pinMode(LASER_PIN, OUTPUT);
	digitalWrite(LASER_PIN, 0);

	myServo.attach(SERVO_PWM);

  // create a task for updating motor positions
  xTaskCreatePinnedToCore(
    updateMotors,
    "Update motors",
    4096,
    NULL,
    2,
    NULL,
    0
  );

  // create task for reading bluetooth controller
  BaseType_t resultx = xTaskCreatePinnedToCore(
    readBtController,
    "Read Bluetooth Controller",
    4096,
    NULL,
    3,
    &bluetoothHandle,
    1 // bluetooth on a separate core from motors
  );

	xTaskCreatePinnedToCore(
    updateLED,
    "Write to LED indicator",
    2048,
    NULL,
    1,
    &ledHandle,
    0
  );

}

void loop() {


}
