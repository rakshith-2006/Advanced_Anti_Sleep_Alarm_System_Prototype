#include <Arduino.h>
#include <ESP_Mail_Client.h>
#include <TinyGPSPlus.h>

TinyGPSPlus gps;

#if defined(ESP32)
  #include <WiFi.h>
  // GPS Module RX pin to NodeMCU 17
  // GPS Module TX pin to NodeMCU 16
  #define RXD2 16
  #define TXD2 17
  HardwareSerial neo7m(2);
  #define eye_blink_sensor 5
  #define BUZZER_PIN 26  // Define the buzzer pin
#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
  #include <SoftwareSerial.h>
  // GPS Module RX pin to NodeMCU D3
  // GPS Module TX pin to NodeMCU D4
  const int rxPin = D4, txPin = D3;
  SoftwareSerial neo7m(rxPin, txPin);
  #define eye_blink_sensor D5
  #define BUZZER_PIN D6  // Define the buzzer pin
#endif

#define AUTHOR_EMAIL "rakshithkkr01@gmail.com"
#define AUTHOR_PASSWORD "diekvjbxtwtdkgij"
#define RECIPIENT_EMAIL "rakshith300306@gmail.com"
#define SMTP_HOST "smtp.gmail.com"
#define SMTP_PORT 465 // SMTP port (SSL)

#define WIFI_SSID "SSID"
#define WIFI_PASSWORD "Password"

SMTPSession smtp;
void smtpCallback(SMTP_Status status);

int drowsinessCount = 0;  // Counter for drowsiness detections

void setup() {
  Serial.begin(115200);
  pinMode(eye_blink_sensor, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);  // Set buzzer pin as OUTPUT
  digitalWrite(BUZZER_PIN, LOW); // Ensure buzzer is off at start

  #if defined(ESP32)
    neo7m.begin(9600, SERIAL_8N1, RXD2, TXD2);
  #elif defined(ESP8266)
    neo7m.begin(9600);
  #endif

  Serial.println();
  Serial.print("Connecting to AP");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(200);
  }
  Serial.println("\nWiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  int val = digitalRead(eye_blink_sensor);
  if (val == HIGH) {
    Serial.println("Drowsiness detected!!!");
    
    // Activate buzzer for 1 second
    digitalWrite(BUZZER_PIN, HIGH);
    delay(1000);
    digitalWrite(BUZZER_PIN, LOW);

    drowsinessCount++;  // Increase count

    // If drowsiness is detected more than 3 times, send email alert
    if (drowsinessCount > 3) {
      send_email_alert();
      drowsinessCount = 0;  // Reset counter after sending email
    }
  }
  delay(1000);
}

void send_email_alert() {
  boolean newData = false;

  // Wait for valid GPS data for up to 10 seconds
  for (unsigned long start = millis(); millis() - start < 10000;) {
    while (neo7m.available()) {
      if (gps.encode(neo7m.read())) {
        newData = true;
        break;
      }
    }
  }

  // Check if GPS data is valid
  if (!newData || !gps.location.isValid()) {
    Serial.println("No Valid GPS Data Found.");
    return;
  }

  // Reset newData flag after getting valid data
  newData = false;

  // Get latitude and longitude
  String latitude = String(gps.location.lat(), 6);
  String longitude = String(gps.location.lng(), 6);
  Serial.println("Latitude: " + latitude);
  Serial.println("Longitude: " + longitude);

  // Set up the SMTP client
  smtp.debug(1);
  smtp.callback(smtpCallback);

  ESP_Mail_Session session;
  session.server.host_name = SMTP_HOST;
  session.server.port = SMTP_PORT;
  session.login.email = AUTHOR_EMAIL;
  session.login.password = AUTHOR_PASSWORD;
  session.login.user_domain = "";  // Optional, may not be needed

  // Prepare the email message
  SMTP_Message message;
  message.sender.name = "ESP";
  message.sender.email = AUTHOR_EMAIL;
  message.subject = "ESP Motion Alert";
  message.addRecipient("rakshith sooriya", RECIPIENT_EMAIL);

  // HTML content for the email body
  String htmlMsg = "<div style=\"color:#2f4468;\">";
  htmlMsg += "<h1>Latitude: " + latitude + "</h1>";
  htmlMsg += "<h1>Longitude: " + longitude + "</h1>";
  htmlMsg += "<h1><a href=\"http://maps.google.com/maps?q=loc:" + latitude + "," + longitude + "\">Check Location in Google Maps</a></h1>";
  htmlMsg += "<p>- Sent from ESP board</p>";
  htmlMsg += "</div>";

  message.html.content = htmlMsg.c_str();  // Set HTML content
  message.text.charSet = "us-ascii";
  message.html.transfer_encoding = Content_Transfer_Encoding::enc_7bit;

  // Connect to SMTP server and send the email
  if (!smtp.connect(&session)) {
    Serial.println("Error: Failed to connect to SMTP server.");
    return;
  }

  if (!MailClient.sendMail(&smtp, &message)) {
    Serial.println("Error sending email: " + smtp.errorReason());
  } else {
    Serial.println("Email sent successfully.");
  }
}

void smtpCallback(SMTP_Status status) {
  Serial.println(status.info());
  if (status.success()) {
    Serial.println("----------------");
    ESP_MAIL_PRINTF("Message sent success: %d\n", status.completedCount());
    ESP_MAIL_PRINTF("Message sent failed: %d\n", status.failedCount());
    Serial.println("----------------\n");

    struct tm dt;
    for (size_t i = 0; i < smtp.sendingResult.size(); i++) {
      SMTP_Result result = smtp.sendingResult.getItem(i);
      time_t ts = (time_t)result.timestamp;
      localtime_r(&ts, &dt);

      ESP_MAIL_PRINTF("Message No: %d\n", i + 1);
      ESP_MAIL_PRINTF("Status: %s\n", result.completed ? "success" : "failed");
      ESP_MAIL_PRINTF("Date/Time: %d/%d/%d %d:%d:%d\n", dt.tm_year + 1900, dt.tm_mon + 1, dt.tm_mday, dt.tm_hour, dt.tm_min, dt.tm_sec);
      ESP_MAIL_PRINTF("Recipient: %s\n", result.recipients);
      ESP_MAIL_PRINTF("Subject: %s\n", result.subject);
    }
    Serial.println("----------------\n");
  }
}
