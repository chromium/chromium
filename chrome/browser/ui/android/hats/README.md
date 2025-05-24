# Clank HaTS Survey Integration Guide

This one-page explains the integration steps needed to use HaTS on Clank. For generic integration flow beyond coding, please see [go/clank-hats](go/clank-hats).

[TOC]

---

## 1 - Add SurveyConfig

Define your SurveyConfig in `survey_config.h`, and fill in the details in `survey_config.cc`.

This is also the time to decide the requirements for the survey (e.g. is the survey user-prompt? What PSD will be attached to the survey?). The survey_configs are owned by privacy reviewers - when adding a new SurveyConfig, please be sure to get reviews from one of the OWNERS in `//chrome/browser/ui/hats/OWNERS`.


## 2 - Create SurveyUiDelegate

Create a [SurveyUiDelegate](java/src/org/chromium/chrome/browser/ui/hats/SurveyUiDelegate.java) instance that is used to display the survey. This is needed in order to create a SurveyClient (see below).

### Java client
There is a ready-to-use [MessageSurveyUiDelegate](java/src/org/chromium/chrome/browser/ui/hats/MessageSurveyUiDelegate.java) that shows the invitation using a Clank message; features can implement the interface `SurveyUiDelegate` to define their own behaviors.

### C++ client
The survey UI interface is defined in [hats::SurveyUiDelegateAndroid](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/android/hats/survey_ui_delegate_android.h;l=32?q=hats::SurveyUiDelegateAndroid&ss=chromium%2Fchromium%2Fsrc). The interface allows users to create a message UI client, or implement their own behaviors.

## 3 - Create SurveyClient
Create a `SurveyClient` instance. Once the client instance is ready, use `showSurvey()` to start the survey presentation flow.

`showSurvey()` is also the chance to pass in the PSD when showing the survey. This means features don’t have to always have the PSD ready when creating the `SurveyClient`. In order for the PSD to link correctly, the number of keys and values for the PSD has to match exactly what’s defined in the `survey_config.cc`.

### Java client
Use [SurveyClientFactory.getInstance().createClient()](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/android/hats/internal/java/src/org/chromium/chrome/browser/ui/hats/SurveyClientFactory.java;l=79?q=SurveyClientFactory.createClient&sq=&ss=chromium%2Fchromium%2Fsrc) to create an instance.


### C++ client
Use [hats::HatsServiceAndroid](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/android/hats/hats_service_android.h;l=36?q=HatsServiceAndroid&ss=chromium%2Fchromium%2Fsrc) to launch the new survey.

## Reference


### Example usage
* Java client: [ChromeSurveyControlle](https://source.chromium.org/chromium/chromium/src/+/main:chrome/android/java/src/org/chromium/chrome/browser/survey/ChromeSurveyController.java)
* C++ client: [ChromePermissionsClient::TriggerPromptHatsSurveyIfEnabled](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/permissions/chrome_permissions_client.cc;l=281?q=ChromePermissionsClient::TriggerPromptHatsSurveyIfEnabled&ss=chromium%2Fchromium%2Fsrc)

### Survey presentation flow

App or feature (`ChromeSurveyController` in this example) code calls `SurveyFactory.createSurveyClient()` to create a SurveyClient, possibly with a `SurveyUiDelegate` (`MessagesSurveyUiDelegate` in this example)

1. App or feature call `SurveyClient.showSurvey()` to start downloading in the background;
2. A throttling check `canShowSurvey()` will be performed;
`downloadSurvey()` will start if the throttler check is passed; SurveyClient will communicate with `SurveyController` that eventually calls HaTS client API;
3. Once survey download is completed, `SurveyUiDelegate.showSurveyInvitation()` is called to present the survey.
    * Internally, SurveyClient calls showSurveyInvitation()
    *  If the download is not completed yet or failed, showSurveyInvitation() will not be called.
4. Once Survey is shown, `SurveyClient.destroy()` to release objects.

(More design / implementation details see design doc: [go/clank-hats-2023](go/clank-hats-2023))
