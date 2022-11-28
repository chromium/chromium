# Autofill

Autofill is a [layered
component](https://sites.google.com/a/chromium.org/dev/developers/design-documents/layered-components-design).
It has the following structure:

- [`core/`](https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/core): Code shared by `content/` and `ios/`.
  - [`browser/`](https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/core/browser): Browser process code.
  - [`common/`](https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/core/common): Code shared by the browser and the renderer.
- [`content/`](https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/content): Driver using the `//content` layer (all platforms except iOS).
  - [`browser/`](https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/content/browser): Browser process code.
  - [`renderer/`](https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/content/renderer): Renderer process code.
  - [`common/`](https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/content/common): Code shared by the browser and the renderer.
- [`ios/`](https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/ios): Driver using `//ios` (as opposed to `//content`).
- [`android/`](https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/android): Java code for Android.

## High-level architecture

The following architecture diagram shows instances of Autofill's core objects.
For simplicity, we only consider the case of Chrome Autofill on non-iOS
platforms. The diagram is best read bottom-up because every Autofill flow starts
with `AutofillAgent` extracting a form from the DOM.

```
┌────────────────────┐
│PersonalDataManager ├────────┬──────────────────┐
│1 per BrowserContext│  owns N│            owns N│
└─▲──────────────────┘      ┌─▼─────────────┐  ┌─▼────────┐
  │                         │AutofillProfile│  │CreditCard│
  │weak ref                 └───────────────┘  └──────────┘
┌─┴───────────────┐
│FormDataImporter ◄─────────────────────┐
│1 per WebContents│               events│
└─▲───────────────┘                     │
  │                                     │                                  ┌───────────────┐
  │ ┌────────────────────────┐        ┌─┴────────────────────┐             │Autofill server│
  │ │AutofillExternalDelegate◄────────┤BrowserAutofillManager├───────┐     └─────────────▲─┘
  │ │1 per RenderFrameHost   │  owns 1│1 per RenderFrameHost │  votes│               HTTP│
  │ └──────────────────────┬─┘        └─▲──────────────────┬─┘     ┌─▼───────────────────▼─┐
  │                  events│            │            events│       │AutofillDownloadManager│
  │                        │            │                  │       │1 per RenderFrameHost  │
  │                        │            │                  │       └─────────────────────▲─┘
  │                        │            │                  │                             │
  │                        │            │                  │                             │
  │                        │            │                  └──┐ ┌──────────────┐         │
  │                        │            │                     │ │FormStructure │         │
  │                        │            │                     │ │1 per FormData│         │
  │                        │            │                     │ └─▲────────────┘         │
  │                        │            │                     │   │                      │
  │owns 1                  │            │events               │   │sets types     queries│
┌─┴──────────────────┐     │          ┌─┴───────────────────┐ │   │owns N          owns 1│
│ChromeAutofillClient◄─────┼──────────┤AutofillManager      ├─┼───┴──────────────────────┘
│1 per WebContents   │     │  weak ref│1 per RenderFrameHost│ │
└────────────────────┘     │          └─▲─────────────────┬─┘ │
                           │            │           events│   │
                           └────────────┼────────────────►│◄──┘
                                        │                 │
                           ┌────────────┼─────────────────┼────────────┐
                           │owns 1      │events           │            │
                           │            │owns 1           │            │
┌──────────────────────────┴─┐        ┌─┴─────────────────▼─┐        ┌─▼───────────────────┐
│ContentAutofillDriverFactory├────────►ContentAutofillDriver◄────────►ContentAutofillRouter│
│1 per WebContents           │owns N  │1 per RenderFrameHost│ events │1 per WebContents    │
└────────────────────────────┘        └─▲─────────┬─────────┘        └─────────────────────┘
                                        │         │
Browser                                 │         │fill form and
1 process                               │         │other events
────────────────────────────────────────┼─────────┼─────────────────────────────────────────
Renderer              events, often with│         │
N processes           FormData objects  │         │
                                        │         │
                                      ┌─┴─────────▼─────┐       ┌─────────────────────┐
                                      │AutofillAgent    ├───────►form_autofill_util.cc│
                                      │1 per RenderFrame│calls  └─────────────────────┘
                                      └─────────────────┘
```
To edit the diagram, copy-paste it to asciiflow.com.

A [`WebContents`](https://source.chromium.org/chromium/chromium/src/+/main:content/public/browser/web_contents.h)
corresponds to a tab. A [`RenderFrameHost`](https://source.chromium.org/chromium/chromium/src/+/main:content/public/browser/render_frame_host.h)
roughly corresponds to a frame or a document (but to neither exactly; they
differ in whether or not they survive navigations; details are [here](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/render_document.md)
and [details](https://docs.google.com/document/d/1C2VKkFRSc0kdmqjKan1G4NlNlxWZqE4Wam41FNMgnmA/edit#)).
A [`BrowserContext`](https://source.chromium.org/chromium/chromium/src/+/main:content/public/browser/browser_context.h)
corresponds to a [`Profile`](https://www.chromium.org/developers/design-documents/profile-architecture/).

### Differences among platforms and embedders

* Desktop vs Android: Android has different UI, which mostly lives in
  `//chrome`.
* non-iOS vs iOS: iOS also uses `AutofillManager` and everything north of it,
  but `AutofillDriverIOS*` instead of `ContentAutofill*`, and a different but
  identically named `AutofillAgent`.
* Chrome vs WebView: WebView also uses `AutofillManager` and everything south
  of it, but `AwAutofillClient` instead of `ChromeAutofillClient`, and
  `AndroidAutofillManager` instead of `BrowserAutofillManager`.

### Links to files

- [`core/`](https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/core)
  - [`common/`](https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/core/common)
    - [`form_data.h`](https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/core/common/form_data.h)
  - [`browser/`](https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/core/browser)
    - [`autofill_client.h`](https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/core/browser/autofill_client.h)
      - [`//android_webview/browser/aw_autofill_client.h`](https://source.chromium.org/chromium/chromium/src/+/main:android_webview/browser/aw_autofill_client.h) (WebView implementation)
      - [`//chrome/browser/ui/autofill/chrome_autofill_client.h`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/autofill/chrome_autofill_client.h) (Chrome implementation)
    - [`autofill_download_manager.h`](https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/core/browser/autofill_download_manager.h)
    - [`autofill_driver.h`](https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/core/browser/autofill_driver.h)
      - [`../../content/browser/content_autofill_driver.h`](https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/core/browser/autofill_driver.h) (non-iOS implementation)
      - [`../../ios/browser/autofill_driver_ios.h`](https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/ios/browser/autofill_driver_ios.h) (iOS implementation)
    - [`autofill_external_delegate.h`](https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/core/browser/autofill_external_delegate.h)
    - [`autofill_manager.h`](https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/core/browser/autofill_manager.h)
      - [`browser_autofill_manager.h`](https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/core/browser/browser_autofill_manager.h) (Chrome specialization)
      - [`//components/android_autofill/browser/android_autofill_manager.h`](https://source.chromium.org/chromium/chromium/src/+/main:components/android_autofill/browser/android_autofill_manager.h) (WebView specialization)
    - [`data_model/`](https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/core/browser/data_model)
      - [`autofill_profile.h`](https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/core/browser/data_model/autofill_profile.h)
      - [`credit_card.h`](https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/core/browser/data_model/credit_card.h)
    - [`form_data_importer.h`](https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/core/browser/form_data_importer.h)
    - [`form_structure.h`](https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/core/browser/form_structure.h)
    - [`personal_data_manager.h`](https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/core/browser/personal_data_manager.h)
    - [`proto/`](https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/core/browser/proto/) (Autofill server)
- [`content/`](https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/content)
  - [`browser/`](https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/content/browser)
      - [`content_autofill_driver.h`](https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/content/browser/content_autofill_driver.h)
      - [`content_autofill_driver_factory.h`](https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/content/browser/content_autofill_driver_factory.h)
      - [`content_autofill_router.h`](https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/content/browser/content_autofill_router.h)
  - [`renderer/`](https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/content/renderer)
      - [`autofill_agent.h`](https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/content/renderer/autofill_agent.h)
      - [`form_autofill_util.cc`](https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/content/renderer/autofill_agent.h)
- [`ios/`](https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/ios)
  - [`browser/`](https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/ios/browser)
    - [`autofill_agent.h`](https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/ios/browser/autofill_agent.h)
    - [`autofill_driver_ios.h`](https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/ios/browser/autofill_driver_ios.h)
  - [`form_util/`](https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/ios/form_util)
    - [`form.js`](https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/ios/form_util/resources/form.js)
    - [`fill.js`](https://source.chromium.org/chromium/chromium/src/+/main:components/autofill/ios/form_util/resources/fill.js)

# Related directories

There are some closely related directories in `//chrome`:

- [`//chrome/browser/autofill`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/autofill)
- [`//chrome/browser/ui/android/autofill`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/android/autofill)
  - [`//chrome/android/java/src/org/chromium/chrome/browser/autofill`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/android/java/src/org/chromium/chrome/browser/autofill/)
- [`//chrome/browser/ui/autofill`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/autofill)
- [`//chrome/browser/ui/views/autofill`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/views/autofill)
