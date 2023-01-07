# Android Stylus Handwriting - Direct Writing

This folder contains files, .aidl service definitions needed for using Stylus handwriting
recognition service called the Direct Writing service. It contains definitions for Service API
implemented by the Samsung Keyboard application, and for Service Callback API which needs to be
implemented by the application using this service which in this case is Chrome.

Since Android Platform cannot recognize the location of HTML input fields in Chrome Tab or Web view,
Direct writing service expects Chrome to detect stylus writable input field and send Touch events to
the service API when stylus writing can be initiated. In response to the recognition, the Direct
Writing service calls the service callback APIs to commit the recognized text and to handle the
recognized gestures like adding or removing spaces and deleting some text under the gesture.
Before this, Chrome needs to connect to the Direct Writing service and register the object that
implements the service callback APIs.
