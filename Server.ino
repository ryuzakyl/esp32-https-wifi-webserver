#include <WiFi.h>

#include <HTTPSServer.hpp>
#include <SSLCert.hpp>
#include <HTTPRequest.hpp>
#include <HTTPResponse.hpp>

#define WIFI_SSID "MIWIFI_2G_E6L7"
#define WIFI_PASSWORD "vfueYJsv"

// ---------------------------------------------------------------

// LM35 temperature sensor is connected to GPIO 33 (Analog ADC1_5)
const int lm35InputPin = 33;

const int esp32BitsPrecision = 10;

const float esp32MaxVolts = 1.5;

using namespace httpsserver;

SSLCert* cert;
HTTPSServer* secureServer;

// ---------------------------------------------------------------

float analog_to_volts(float value, float max_volts, int n_bits) {
    // compute analog resolution
    float resolution = pow(2, n_bits) - 1;

    // transform value in [0, 2^n_bits] interval, to [0, max_volts] interval
    float volts = (value / resolution) * max_volts;

    return volts;
}

float volts_to_celcius(float volts) {
    // for LM35: divide by 10 mV (i.e., multiply by 100)
    return 100.0 * volts;
}

float analog_to_celcius(float value) {
    // converts analog value to volts
    float volts = analog_to_volts(value, esp32MaxVolts, esp32BitsPrecision);

    // converts volts value to celcius
    float celcius = volts_to_celcius(volts);

    return celcius;
}

float celcius_to_farenheit(float celcius) {
    float fahrenheit;

    /* celsius to fahrenheit conversion formula */
    fahrenheit = (celcius * 9 / 5) + 32;

    return fahrenheit;
}

float get_lm35_temperature() {
    // read analog value from input GPIO
    int analog_value = analogRead(lm35InputPin);

    // convert analog input to temperature (in celcius)
    float t_celcius = analog_to_celcius(analog_value);

    return t_celcius;
}

// ---------------------------------------------------------------

// this sets the ground pin to LOW and the input voltage pin to high
void setup() {
    // using 9600 baud rate
    Serial.begin(9600);

    Serial.println("Creating certificate...");

    cert = new SSLCert();

    int createCertResult = createSelfSignedCert(
        *cert,
        KEYSIZE_2048,
        "CN=myesp.local,O=acme,C=US"
    );
   
    if (createCertResult != 0) {
        Serial.printf("Error generating certificate");
        return; 
    }

    Serial.println("Certificate created with success");
    secureServer = new HTTPSServer(cert);

    // connecting to specified wifi
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Connecting to WiFi..");
    }

    // showing local IP address
    Serial.println(WiFi.localIP());

    ResourceNode * nodeRoot = new ResourceNode("/", "GET", [](HTTPRequest * req, HTTPResponse * res) {
        // get current LM35 temperature
        float t_celcius = get_lm35_temperature();
        float t_farenheit = celcius_to_farenheit(t_celcius);

        // formatting the json content to be sent
        char json_content[100];
        sprintf(
            json_content,
            "{ msg: 'The current temperature is %.2f celcius degrees'}",
            t_celcius
        );

        res->println(json_content);
    });

    // starting the secure server
    secureServer->registerNode(nodeRoot);
    secureServer->start();
    if (secureServer->isRunning()) {
        Serial.println("Server running.");
    }
}

// ---------------------------------------------------------------

// main loop
void loop()
{
    secureServer->loop();
    delay(10);
}
