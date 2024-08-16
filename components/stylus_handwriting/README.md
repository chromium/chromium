# Stylus Handwriting to HTML text input

This directory contains files and classes needed to support the Stylus Handwriting recognition
feature provided by the Windows and Android Platforms, used to commit text to html inputs.

## Android Platform
Since Android Platform cannot recognize the location of HTML input fields in Chrome Tab or Web view,
Stylus writing service expects Chrome to detect stylus writable input field and initiate the stylus
handwriting recognition. System settings would be used to determine the status of stylus writing
service on the device. There are 2 different ways in which Chromium would interact with the
platform's writing services:

### 1. Android Stylus writing
Android Platform provides APIs to interact with the stylus writing system so that recognition can be
started in HTML input fields. Then the platform uses the InputConnection created for the HTML input
field to commit the recognized text. Stylus input gestures like deleting text or adding spaces is
also supported with feedback from Chromium about the Input field location and the text around the
gesture coordinates. This feature is currently available in Android version T under Developer
options only and will be available fully from Android U.

### 2. Direct Writing service available only in Samsung Platform
This feature is exposed to webview and Chrome from Samsung Platform via service aidls defined in the
//content/public directory. Direct writing service expects Chrome to implement the service callback
Interface and forward touch events to the service when writing is detected over and input field.
Here, the resposibility of committing the recognized text lies with Chromium and is handled via the
ImeAdapter class. The information about Input field bounds, position on screen, caret position are
also expected to be provided in the service callback implementation. The Direct Writing service is
available on select devices starting Android version R.

Stylus Gestures are detected by the service and gesture data like gesture coordinates and gesture
type, whether it is to remove or add text near current input text, is sent to Chromium to handle the
gesture and take corresponding input action.

## Windows Platforms

### Text Services Framework (TSF) Shell Handwriting in Windows Platforms
Windows Platforms provide TSF APIs for integrating handwriting to text recognition. This feature is
exposed to webview and Chrome for Windows Platforms.
