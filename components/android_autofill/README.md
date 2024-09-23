# Android Autofill

Android Autofill is a component that provides functionality to use alternative
Autofill providers, such as the
[Android Autofill framework](https://developer.android.com/guide/topics/text/autofill).

It is used exclusively by the [`//android_webview`](https://source.chromium.org/chromium/chromium/src/+/main:android_webview/)
embedder.

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
  * One instance per embedder wrapper of `WebContents`, i.e. `AwContents`.
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
