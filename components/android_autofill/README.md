# Android Autofill

Android Autofill is a component that provides functionality to use alternative
Autofill providers, such as the
[Android Autofill framework](https://developer.android.com/guide/topics/text/autofill).

It is always used by the [`//android_webview`](https://source.chromium.org/chromium/chromium/src/+/main:android_webview/)
embedder. The [`//chrome`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/) embedder
uses it only if users set the `"autofill.using_virtual_view_structure"` pref in settings — see below
for chrome-specific usage.


# High-level architecture

The below diagram shows the main classes involved in `//android_autofill`.

```
                                   ┌───────────────────────┐             ┌──────────────────────────┐
                                   │                       │             │                          │
                                   │AwContents             │             │AwContents.java           │
                                   │                       │             │                          │
                                   └───┬─────────▲─────────┘             └───┬──────────────────────┘
                                       │         │                           │
                                       │owns 1   │raw ref                    │
┌──────────────────────┐           ┌───▼─────────┴─────────┐                 │
│ContentAutofillDriver │           │                       │                 │
│(1 per RFH,           │           │WebContents            │                 │
│ see c/autofill)      │           │                       │                 │
└───┬──────────────────┘           └───┬───────────────────┘                 │
    │                                  │                                     │
    │owns                              │owns                                 │owns
┌───▼──────────────────┐           ┌───▼───────────────────┐              ┌──▼──────────────────────┐     ┌────────────────────────────┐
│                      │           │AndroidAutofillProvider├─events──────►│                         │     │AutofillManagerWrapper.java │
│AndroidAutofillManager├─events───►│implements             │              │AutofillProvider.java    │owns─►(wraps Android's            │
│                      │           │AutofillProvider       │◄─ask-to-fill─┤                         │     │ AutofillManager)           │
└──────────────────────┘           └───┬───────────────────┘              └──┬──────────────────────┘     └────────────────────────────┘
                                       │                                     │
                                       │                                     │owns at most 1
                                       │                                  ┌──▼──────────────────────┐
                                       │                                  │                         │
                                       │                                  │AutofillRequest.java     │
                                       │                                  │                         │
                                       │                                  └──┬──────────────────────┘
                                       │                                     │
                                       │owns at most 1                       │owns 1
                                   ┌───▼───────────────────┐              ┌──▼──────────────────────┐
                                   │FormDataAndroid        │              │                         │
                                   │(wraps a FormData)     ◄──updates─────►FormDataAndroid.java     │
                                   │                       │  & references│                         │
                                   └───┬───────────────────┘              └──┬──────────────────────┘
                                       │                                     │
                                       │owns 0 to N                          │owns 0 to N
                                   ┌───▼───────────────────┐              ┌──▼──────────────────────┐
                                   │                       │              │                         │
                                   │FormFieldDataAndroid   ◄──updates─────►FormFieldDataAndroid.java│
                                   │                       │  & references│                         │
                                   └───────────────────────┘              └─────────────────────────┘
```
To edit the diagram, copy-paste it to asciiflow.com.

# Responsibilities of the main classes

## C++ classes
* [`AndroidAutofillManager`](https://source.chromium.org/chromium/chromium/src/+/main:components/android_autofill/browser/android_autofill_manager.h;bpv=0;bpt=0):
  * One instance per `RenderFrameHost`, i.e. potentially multiple instances per `WebContents`.
  * Implements `AutofillManager` to receive information for various `OnX()` events.
  * Responsibilities:
    * Forward all information from various `OnX()` implementations to the
      `AutofillProvider` of the `WebContents` (if one exists).
    * Conversely, receive fill requests from `AutofillProvider` and forward them to `ContentAutofillDriver`.
* [`AndroidAutofillProvider`](https://source.chromium.org/chromium/chromium/src/+/main:components/android_autofill/browser/android_autofill_provider.h;bpv=0;bpt=0):
  * One instance per `WebContents`.
  * Implements `AutofillProvider` (and is currently the only class to do).
  * Owns at most one `FormDataAndroid`, the one related to the current autofill session
  * Responsibilities:
    * Start an Autofill session. An Autofill session is initiated e.g. if a user interacts with a form field
      and `OnAskForValuesToFill` is called. The purpose of an Autofill session is to keep Android's AutofillManager
      informed about the state of the currently focused form such as its field structure, its field positions, the currently
      focused field, etc. Autofill sessions are tied to a form in a frame and are represented by `AutofillRequests.java`
      on the Java side.
    * If there is an ongoing autofill session, update the `FormDataAndroid` object
      it owns to keep it in sync with changes on the page.
    * Forward information from `OnX()` methods to its `AutofillProvider.java` sibling.
* [`FormDataAndroid`](https://source.chromium.org/chromium/chromium/src/+/main:components/android_autofill/browser/form_data_android.h;bpv=0;bpt=0):
  * Is created based from a `FormData` object and creates a copy of it, which continues to be
    updated by its parent `AndroidAutofillProvider`.
  * Owns 0 to N `FormFieldDataAndroid` objects that represent the form field elements in the form.
  * Responsibilities:
    * Form a wrapper around `FormData` and propagate updates to and from its sibling class
      `FormDataAndroid.java`.
* [`FormFieldDataAndroid`](https://source.chromium.org/chromium/chromium/src/+/main:components/android_autofill/browser/form_field_data_android.h;bpv=0;bpt=0):
  * Responsibilities:
    * Forms a wrapper around `FormFieldData` and propagate updates to and from its
      sibling class `FormFieldDataAndroid.java`.

## Java classes
* [`AutofillProvider.java`](https://source.chromium.org/chromium/chromium/src/+/main:components/android_autofill/browser/java/src/org/chromium/components/autofill/AutofillProvider.java;bpv=0;bpt=1):
  * One instance per embedder wrapper of `WebContents` (or `AwContents`).
  * Owns up to one `AutofillRequest.java`, one `AutofillManagerWrapper.java` and multiple helper classes
    (e.g. for metrics collection).
  * Responsibilities:
    * Orchestrate Autofill logic on the Java and form the glue between its C++ siblings,
      `AutofillManagerWrapper.java`, and various metrics/helper classes.
    * Create an `AutofillRequest.java` when a new Autofill session is started and forwards updates to it.
    * Use `AutofillRequest.java` to fill virtual view structures for use in Android Autofill.
    * Propagate selection choices to its C++ sibling.
* [`AutofillRequest.java`](hhttps://source.chromium.org/search?q=class:AutofillProvider.AutofillRequest&ss=chromium):
  Responsibilities:
    * Package information about the view structure of the current session's form for use by
      Android's Autofill framework.
* [`FormDataAndroid.java`](https://source.chromium.org/chromium/chromium/src/+/main:components/android_autofill/browser/java/src/org/chromium/components/autofill/FormData.java;bpv=0;bpt=1) and [`FormFieldDataAndroid.java`](https://source.chromium.org/chromium/chromium/src/+/main:components/android_autofill/browser/java/src/org/chromium/components/autofill/FormFieldData.java;bpv=0;bpt=1): Pure data classes and siblings to the
  C++ classes of the same name.

# Chrome-specific functionality for Android Autofill
By default, Chrome uses its a platform-agnostic implementation of Autofill for passwords, addresses
and autofill. If the user pref `"autofill.using_virtual_view_structure"` is set when Chrome starts,
all autofill activity will instead be forwarded to Android Autofill.

## Changing the setting
The setting is only available if the feature `"enable-autofill-virtual-view-structure"` in
`chrome://flags` is enabled. Users can then navigate to the Chrome settings screen and select the
`Autofill services` entry to switch Autofill. The Settings screen prompts the user to restart.

Restarting after changing the pref ensures that all tabs and frames run with the same configuration
and provide a consistent autofill experience for the entire browsing session.  The
[`AutofillClientProvider`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/autofill/autofill_client_provider.h) provides the
autofill mechanism that was selected on startup and constructs the correct AutofillClient.

Users can only change the setting if a valid Android Autofill service is configured in Android
System Settings. The Chrome settings screen offers a deep-link to these settings.

### Deep-linking into Chrome settings
Autofill Services may want to direct users to Chrome settings. Chrome allows to start this activity
using an intent:
```java
Intent autofillSettingsIntent = new Intent(Intent.ACTION_APPLICATION_PREFERENCES);
autofillSettingsIntent.addCategory(Intent.CATEGORY_DEFAULT);
autofillSettingsIntent.addCategory(Intent.CATEGORY_APP_BROWSER);
autofillSettingsIntent.addCategory(Intent.CATEGORY_PREFERENCE);

// Invoking the intent with a chooser allows users to select the channel they want to configure. If
// there is only one browser reacting to the intent, the chooser is skipped.
Intent chooser = Intent.createChooser(autofillSettingsIntent, "Pick Chrome Channel");
startActivity(chooser);

// If the caller knows which Chrome channel they want to configure, they can instead add a package
// hint to the intent, e.g.
autofillSettingsIntent.setPackage("com.android.chrome");
startActivity(autofillSettingsInstent);
```

The deep-link sanitizes the launch intent using the `AutofillOptionsLauncher` and opens a new
[AutofillOptionsFragment.java](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/autofill/android/java/src/org/chromium/chrome/browser/autofill/options/AutofillOptionsFragment.java)

### Querying the Chrome setting
Autofill Services can read the Chrome setting to understand whether Chrome uses Android Autofill.
For that purpose, Chrome defines the [AutofillThirdPartyModeContentProvider.java](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/autofill/android/java/src/org/chromium/chrome/browser/autofill/AutofillThirdPartyModeContentProvider.java).
This [`ContentProvider`](https://developer.android.com/reference/android/content/ContentProvider)
allows reading whether Chrome is in "3P mode", i.e. uses Android Autofill.
To read the status of the setting, declare the permission for the Chrome channel
you are interested in in your `AndroidManifest.xml`:
```xml
  <uses-permission android:name="android.permission.READ_USER_DICTIONARY"/>
  <queries>
    <package android:name="com.chrome.canary" />
    <package android:name="com.chrome.dev" />
    <package android:name="com.chrome.beta" />
    <package android:name="com.google.android.apps.chrome" />
    <package android:name="org.chromium.chrome" />
    <package android:name="com.android.chrome" />
  </queries>
```

```java
final String CHROME_CHANNEL_PACKAGE = "com.android.chrome";  // Chrome Stable.
final String CONTENT_PROVIDER_NAME = ".AutofillThirdPartyModeContentProvider";
final String THIRD_PARTY_MODE_COLUMN = "autofill_third_party_state";
final String THIRD_PARTY_MODE_ACTIONS_URI_PATH = "autofill_third_party_mode";

final Uri uri = new Uri.Builder()
                  .scheme(ContentResolver.SCHEME_CONTENT)
                  .authority(CHROME_CHANNEL_PACKAGE + CONTENT_PROVIDER_NAME)
                  .path(THIRD_PARTY_MODE_ACTIONS_URI_PATH)
                  .build();

final Cursor cursor = getContentResolver().query(
                  uri,
                  /*projection=*/new String[] {THIRD_PARTY_MODE_COLUMN},
                  /*selection=*/ null,
                  /*selectionArgs=*/ null,
                  /*sortOrder=*/ null);

if (cursor == null) {
  // Terminate now! Older versions of Chromium don't provide this information.
}

cursor.moveToFirst(); // Retrieve the result;

int index = cursor.getColumnIndex(THIRD_PARTY_MODE_COLUMN);

if (0 == cursor.getInt(index)) {
  // 0 means that the third party mode is turned off. Chrome uses its built-in
  // password manager. This is the default for new users.
} else {
  // 1 means that the third party mode is turned on. Chrome uses forwards all
  // autofill requests to Android Autofill. Users have to opt-in for this.
}
```
