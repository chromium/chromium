# Password Manager

The password manager is [a layered component](https://sites.google.com/a/chromium.org/dev/developers/design-documents/layered-components-design).

This means that the code is spread out through the following directories:
- ./core/: Essentials, not depending on any other layers. All other layers may
  depend on core.
- ./content/: Content-specific embedding.
- ./ios/: iOS-specific embedding.

*** note
NOTE: Some embedder-specific code must not be part of content (e.g. UI specific
code) and located in
- /chrome/browser/password_manager/
- /chrome/android/java/src/org/chromium/chrome/browser/password_manager/
- /ios/chrome/browser/passwords/
***

## High-level architecture

The following architecture diagram shows instances of core classes. For
simplicity, it shows the concrete instances in the case of Chrome on desktop. In
reality there exist further abstractions, e.g. the
[`ChromePasswordManagerClient`] is the Chrome-specific implementation of the
[`PasswordManagerClient`] interface. The [`ContentPasswordManagerDriver`] is the
[content](https://www.chromium.org/developers/content-module) specific
implementation of the `*::PasswordManagerDriver` interfaces.


```
Browser                                        + UI specific code in browser
                                               | and further browser specific
+-------------------+                          | code
|PasswordFormManager<-------+                  |
+-------------------+       |                  |
                            |  1 per tab       |  1 per tab
+-------------------+       +---------------+  |  +---------------------------+
|PasswordFormManager<-------+PasswordManager<----->ChromePasswordManagerClient|
+-------------------+       +-------+-------+  |  +-------------+-------------+
                            |       |          |                |
+-------------------+       |       |          |                |
|PasswordFormManager<-------+       |          |                |
+-------------------+               |          |                |
1 per form                          |          |  +-------------v-------------+
                                    |          |  |PasswordManagerUIController|
                        1 per frame |          |  +---------------------------+
            +-----------------------+----+     |
            |ContentPasswordManagerDriver|     |
            +-----------------------+----+     |
                                    |          |
+----------------------------------------------+------------------------------+
                                    |
Renderer                            |
             +----------------------------------------------+
             |                      |                       |
             |                      |                       |
             |                      |                       |
     +-------+-----+   +------------+--------+   +----------v------------+
     |AutofillAgent|   |PasswordAutofillAgent|   |PasswordGenerationAgent|
     +-------------+   +---------------------+   +-----------------------+
             1 of each per frame
```

Here is a summary of the core responsibilities of the classes and interfaces:

* [`PasswordManagerClient`] interface (1 per tab)

  Abstracts operations that depend on the embedder environment (e.g. Chromium).
  Manages settings (which features are enabled?), UI (popup bubbles, etc.),
  provides access to the password store, etc.

  * [`ChromePasswordManagerClient`]

    Chrome's implementation of the [`PasswordManagerClient`] on non-iOS
    platforms. This bootstraps the browser side classes at tab creation time.

  * [`IOSChromePasswordManagerClient`] iOS implementation

  * [`WebViewPasswordManagerClient`] //ios/web_view implementation

  * [`StubPasswordManagerClient`] stub for mocking out calls to the embedder
    in tests.

* [`PasswordManager`] (1 per tab)

  Embedder-agnostic password manager, manages the life-cycle of password forms
  (represented as [`PasswordFormManager`] instances). It is informed about newly
  observed forms from the renderers and initiates the filling on page load and
  (together with [`PasswordFormManager`]) save/update prompts on form
  submission.

* [`PasswordFormManager`] (1 per form)

  This manages the life-cycle of an individual password form. It knows about
  credentials that are stored on disk or a credential typed by the user. It
  makes the decision which credentials should be filled into a form and whether
  to offer password saving or updating existing credentials after a successful
  form submission.

* `*::PasswordManagerDriver` (1 per frame)

  The `*::PasswordManagerDriver` is the browser-side communication end-point for
  password related communication between the browser and renderers.

  This is actually a collection of two interfaces named
  `*::PasswordManagerDriver`. The first one ([`mojom::PasswordManagerDriver`])
  is a [mojo](https://chromium.googlesource.com/chromium/src/+/main/mojo/)
  interface that allows renderers to talk to the browser. The second one
  ([`password_manager::PasswordManagerDriver`]) allows the browser to talk to
  renderers.

  * [`ContentPasswordManagerDriver`]

    This is the [content](https://www.chromium.org/developers/content-module)
    specific implementation of the `PasswordManagerDriver` interfaces.

  * [`IOSPasswordManagerDriver`]

    This is the iOS specific implementation.

  * [`StubPasswordManagerDriver`]

    A stub for mocking out communication to the renderer in tests.

* [`AutofillAgent`] (1 per frame)

  The renderer side implementation of Autofill, listens to DOM events and
  autofill UI events, and dispatches them to the password manager specific
  agents.

  This implements the [`mojom::AutofillAgent`] interface.

* [`PasswordAutofillAgent`] (1 per frame)

  The renderer side implementation of filling credentials into the DOM and
  capturing/extracting all relevant information and events from DOM.

  This implements the [`mojom::PasswordAutofillAgent`] interface.

* [`PasswordGenerationAgent`] (1 per frame)

  The renderer side implementation of password generation.

  This implements the [`mojom::PasswordGenerationAgent`] interface.

[`AutofillAgent`]: https://cs.chromium.org/search?q=file:/autofill_agent.h$
[`ChromePasswordManagerClient`]: https://cs.chromium.org/search?q=file:/chrome_password_manager_client.h$
[`ContentPasswordManagerDriver`]: https://cs.chromium.org/search?q=file:/content_password_manager_driver.h$
[`IOSChromePasswordManagerClient`]: https://cs.chromium.org/search?q=file:/ios_chrome_password_manager_client.h$
[`IOSPasswordManagerDriver`]: https://cs.chromium.org/search?q=file:/ios_password_manager_driver.h$
[`mojom::AutofillAgent`]: https://cs.chromium.org/search?q=file:autofill_agent.mojom+"interface+AutofillAgent"
[`mojom::PasswordAutofillAgent`]: https://cs.chromium.org/search?q=file:autofill_agent.mojom+"interface+PasswordAutofillAgent"
[`mojom::PasswordGenerationAgent`]: https://cs.chromium.org/search?q=file:autofill_agent.mojom+"interface+PasswordGenerationAgent"
[`mojom::PasswordManagerDriver`]: https://cs.chromium.org/search?q=file:autofill_driver.mojom+"interface+PasswordManagerDriver"
[`PasswordFormManager`]: https://cs.chromium.org/search?q=file:/password_form_manager.h$
[`password_manager::PasswordManagerDriver`]: https://cs.chromium.org/search?q=file:/password_manager_driver.h$
[`password_manager::PasswordManagerDriver`]: https://cs.chromium.org/search?q=file:/password_manager_driver.h$
[`PasswordAutofillAgent`]: https://cs.chromium.org/search?q=file:/password_autofill_agent.h$
[`PasswordFormManager`]: https://cs.chromium.org/search?q=file:/password_form_manager.h$
[`PasswordGenerationAgent`]: https://cs.chromium.org/search?q=file:/password_generation_agent.h$
[`PasswordManager`]: https://cs.chromium.org/search?q=file:/password_manager.h$
[`PasswordManagerClient`]: https://cs.chromium.org/search?q=file:/password_manager_client.h$
[`StubPasswordManagerClient`]: https://cs.chromium.org/search?q=file:/stub_password_manager_client.h$
[`StubPasswordManagerDriver`]: https://cs.chromium.org/search?q=file:/stub_password_manager_driver.h$
[`WebViewPasswordManagerClient`]: https://cs.chromium.org/search?q=file:/web_view_password_manager_client.h$
