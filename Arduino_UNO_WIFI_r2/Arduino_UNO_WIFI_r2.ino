#include <SPI.h>
#include <WiFiNINA.h>
#include <HCSR04.h>
#include "secrets.h"

// Define variables
const int TRIGGER_PIN = 2; // Pin D2, Ultrasonic sensor
const int ECHO_PIN = 5;    // Pin D5, Ultrasonic sensor

const int MEASURE_CORRECTION = 9; // Added to measure value since te tabletop is higher that the sensor

// Init web server with port 80
int status = WL_IDLE_STATUS;
WiFiServer server(80);

// measurement buffer
const uint8_t BUFFER_SIZE = 10;
static uint8_t buffer_index = 0;
static uint8_t loop_index = 0;
static uint8_t loop_mod = 10;
static double buffer[BUFFER_SIZE];

void setup(void)
{
  // Enable Serial for DEBUG purposes
  // Serial.begin(115200);

  // Setup ultrasonic sensor
  HCSR04.begin(TRIGGER_PIN, ECHO_PIN);

  // check for the WiFi module:
  if (WiFi.status() == WL_NO_MODULE)
  {
    // don't continue
    while (true)
      ;
  }

  // setup Hostname
  WiFi.setHostname(HOSTNAME);

  // attempt to connect to WiFi network
  // Serial.print("Attempting to connect to Network named: ");
  // Serial.println(WIFI_SSID); // print the network name (SSID);
  while (status != WL_CONNECTED)
  {
    // Connect to WPA/WPA2 network.
    status = WiFi.begin(WIFI_SSID, WIFI_PSK);
    Serial.print(".");
    // wait 1 seconds for connection:
    delay(5000);
  }

  // Print debug info via serial
  // Serial.println();
  // Serial.print("Connected - IP: ");
  // Serial.println(WiFi.localIP());

  // start the server
  server.begin();

  // init buffer
  for (uint8_t i = 0; i < BUFFER_SIZE; i++)
  {
    buffer[i] = 0;
  }
}

void loop()
{

  if (WiFi.RSSI() == 0) // Checks if Wifi signal lost.
  {
    // reset arduino by caling reset function at address 0x00
    asm volatile("jmp 0x00"); // reset
  }

  // new measurement in buffer
  if (loop_index >= loop_mod)
  {
    // measure distance and store in buffer
    buffer[buffer_index] = *HCSR04.measureDistanceCm();

    // check if measurement is valid (GREATERTHAN 0)
    if (buffer[buffer_index] > 0)
    {
      buffer_index = (buffer_index + 1) % BUFFER_SIZE;
    }

    loop_index = 0;
  }

  WiFiClient client = server.available(); // listen for incoming clients

  if (client)
  { // if you get a client,
    // Serial.println("new client"); // print a message out the serial port
    String currentLine = ""; // make a String to hold incoming data from the client
    while (client.connected())
    { // loop while the client's connected
      if (client.available())
      { // if there's bytes to read from the client,

        char c = client.read(); // read a byte
        if (c == '\n')
        { // if the byte is a newline character

          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0)
          {
            // measure the height
            double height = 0;
            for (uint8_t i = 0; i < BUFFER_SIZE; i++)
            {
              height += buffer[i];
            }
            height = round(height / BUFFER_SIZE);

            char response[50];
            sprintf(response, "{ \"table_height\": %d }", (int)height + MEASURE_CORRECTION);

            // send height to client
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/json");
            client.println();
            client.println(response);
            client.println();

            // break out of the while loop:
            break;
          }
          else
          { // if you got a newline, then clear currentLine:
            currentLine = "";
          }
        }
        else if (c != '\r')
        {                   // if you got anything else but a carriage return character,
          currentLine += c; // add it to the end of the currentLine
        }
      }
    }
    // close the connection:
    client.stop();
    // Serial.println("client disconnected");
  }
  else
  {
    // sleep one second
    delay(1000);
  }

  loop_index++;
}