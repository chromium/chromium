# Custom Handlers

The custom handlers component provides a way to register custom
protocol handlers for specific URL schemes. This is part of the
implementation of the [System state and capabilities](https://html.spec.whatwg.org/multipage/system-state.html#system-state-and-capabilities)
defined in the HTML spec. Specifically, it implements the interface
[NavigatorContentUtils](https://html.spec.whatwg.org/multipage/system-state.html#navigatorcontentutils)
from which the Navigator object derives.

These handlers may be used to translate the http request's URL so that
it is redirected to the appropriated service (e.g. email, apps) or a
different http(s) site.

All the component's code is intended to be run by the Browser process,
in the UI thread.

## Security and privacy

The component addresses the [security and privacy](https://html.spec.whatwg.org/multipage/system-state.html#security-and-privacy)
considerations described in the HTML spec.

A protocol handler is only valid if it passes all [protocol handler
parameters normalization](https://html.spec.whatwg.org/multipage/system-state.html#normalize-protocol-handler-parameters)
steps. These security and privacy checks are:

* The handler's url must be HTTP or HTTPS; 'chrome-extension' schemes
  may be allowed depending on the [`security level`].
* Ensure the handler's url fulfills the [potentially trustworthy criteria](https://w3c.github.io/webappsec-secure-contexts/#potentially-trustworthy-url)
  defined in the Secure Context spec. This logic is implemented by the
  [`IsUrlPotentiallyTrustworthy`] in the `//services/network` module.

* The protocol being handled must be on the [safelisted scheme](https://html.spec.whatwg.org/multipage/system-state.html#safelisted-scheme)
  described in the spec. The [`IsValidCustomHandlerScheme`] function
  in `blink` implements this check.

Chromium defines a hierarchy of security levels to **relax the
restrictions** imposed by the spec and allow the implementation of
certain features. Being `strict` the default behavior and the one
defined in the spec, there are levels for allowing untrusted-origins
and schemes not listed in the mentioned safelist defined in the spec.

For instance, on order to make possible for extensions to register
their own pages as handlers, the `chrome-extension` scheme is also
allowed when security level is
`blink::ProtocolHandlerSecurityLevel::kExtensionFeatures`.

It's also worth mentioned that Chromium defines its own
[`kProtocolSafelist`] that includes some additional [decentralized schemes](https://github.com/whatwg/html/pull/5482)
that are not being explicitly defined in the mentioned.

## High-level architecture

```
Browser

  +--------------------------------------------------------------------+
  | //components/custom_handlers                                       |
  |                                                                    |
  |                     +------------------------------------------+   |
  |                     | RegisterProtocolHandlerPermissionRequest |   |
  |                     +------------------------------------------+   |
  |                                                / \                 |
  |      +-----------------+                        |                  |
  |      | ProtocolHandler | <-------------+        |                  |
  |      +-----------------+               |        |                  |
  |              / \                       |        |                  |
  |               |                        |        |                  |
  |               |                        |        |                  |
  |               |                        |        |                  |
  |   +-------------------------+          |        |                  |
  |   | ProtocolHandlerRegistry | <--------+        |                  |
  |   +-------------------------+          |        |                  |
  |              / \                       |        |                  |
  |               |                        |        |                  |
  |               |                        |        |                  |
  +--------------------------------------------------------------------+
                  |                        |        |
                  |                        |        |
  +--------------------------------------------------------------------+
  | //chrome      |                        |        |                  |
  |               |                        |        |                  |
  | +--------------------------------+     |  +---------+              |    +--------------------------+
  | | ProtocolHandlerRegistryFactory | <----- | Browser | ----------------> | PermissionRequestManager |
  | +--------------------------------+        +---------+              |    +--------------------------+
  |                                                / \                 |
  |                                                 |                  |
  +--------------------------------------------------------------------+
                                                    |
  +--------------------------------------------------------------------+
  | //content                                       |                  |
  |                                                 |                 |
  |   +-----------------+            +---------------------+           |
  |   | WebContentsImpl | ---------> | WebContentsDelegate |           |
  |   +-----------------+            +---------------------+           |
  |          / \                                                       |
  |           |                                                        |
  |           |                                                        |
  |           |                                                        |
  |  +-------------------------+            +---------------------+    |
  |  | RenderFrameHostDelegate | <--------- | RenderFrameHostImpl |    |
  |  +-------------------------+            +---------------------+    |
  |                                                 |                  |
  |                                                 |                  |
  +--------------------------------------------------------------------+
                                                    |
                                                    |
+--------------------------------------------------------------------------------------------------------+
Renderer                                            |
                                                    |
  +--------------------------------------------------------------------+
  | //blink                                         |                  |
  |                                                 V                  |
  |  +-----------------------+       +------------------------------+  |
  |  | NavigatorContentUtils | ----> | mojom::blink::LocalFrameHost |  |
  |  +-----------------------+       +------------------------------+  |
  |                                                                    |
  +--------------------------------------------------------------------+
 ```


Here is a summary of the core responsibilities of the classes and interfaces:

* [`ProtocolHandler`]

  It's the class responsible of the security and privacy validation
  mentioned before, and eventually of the http request's URL
  translation, using the protocol handler's url spec.

* [`ProtocolHandlerRegistry`]

  This class is implemented as a [`KeyedService`] which means it is
  attached to a [`BrowserContext`].

  The registry holds different kind of protocol handlers lists,
  depending on their source during the registration: user or internal
  policies. The registry also provides an API to selectively ignore
  protocol handlers, which are managed in an independent list.

  There are also some **predefined-handlers**, which are automatically
  registered by the registry factory during the service's
  initialization.

  Finally there is a list of the default handlers for each protocol.

  All the protocol handlers managed by the registry are stored in the
  user preference storage, based on the user profile (the Browser
  Context) used to initialize the keyed service. This makes possible
  to guarantee the persistence of the protocol handlers state.

* [`ProtocolHandlerThrottle`]

  It implements the blink's [`blink::URLLoaderThrottle`] interface to
  manage the http request. It holds a pointer to a
  ProtocolHandlerRegistry instance to performs the URL translation if
  there is a custom handler for the protocol used for the request.

* [`RegisterProtocolHandlerPermissionRequest`]

  It implements the [`PermissionRequest`] interface to manage user
  authorization for the requests issued by the Navigator object's
  `registerProtocolHandler()` method. An instance of this class holds
  a pointer to a _ProtocolHandlerRegistry_ instance and a
  _ProtocolHandler_ reference to be registered.

  It performs the handler registration of granted, or adds it to the
  ignored list if denied.


[`security level`]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/public/common/security/protocol_handler_security_level.h;bpv=1;bpt=1;l=12?q=ProtocolHandlerSecurityLevel&ss=chromium%2Fchromium%2Fsrc
[`IsUrlPotentiallyTrustworthy`]: https://source.chromium.org/chromium/chromium/src/+/main:services/network/public/cpp/is_potentially_trustworthy.cc;l=334?q=IsUrlPotentiallyTrustworthy&ss=chromium%2Fchromium%2Fsrc
[`IsValidCustomHandlerScheme`]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/common/custom_handlers/protocol_handler_utils.cc;l=13?q=IsValidCustomHandlerScheme&ss=chromium%2Fchromium%2Fsrc
[`kProtocolSafelist`]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/common/custom_handlers/protocol_handler_utils.cc;l=40?q=IsValidCustomHandlerScheme&ss=chromium%2Fchromium%2Fsrc
[`KeyedService`]: https://cs.chromium.org/search?q=file:/keyed_service.h$
[`BrowserContext`]: https://cs.chromium.org/search?q=file:/browser_context.h$
[`ProtocolHandler`]: https://cs.chromium.org/search?q=file:/protocol_handler.h$
[`ProtocolHandlerRegistry`]: https://cs.chromium.org/search?q=file:/protocol_handler_registry.h$
[`ProtocolHandlerThrottle`]: https://cs.chromium.org/search?q=file:/protocol_handler_throttle.h$
[`blink::URLLoaderThrottle`]: https://cs.chromium.org/search?q=file:/loader/url_loader_throttle.h$
[`RegisterProtocolHandlerPermissionRequest`]: https://cs.chromium.org/search?q=file:/register_protocol_handler_request.h$
[`PermissionRequest`]: https://cs.chromium.org/search?q=file:/permission_request.h$