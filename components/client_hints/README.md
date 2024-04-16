# Client Hints

This README will serve as a reference for the Chromium implementation of [HTTP Client Hints](https://www.rfc-editor.org/rfc/rfc8942.html), the [HTML and Fetch integrations](https://wicg.github.io/client-hints-infrastructure), and the [Critical-CH response header](https://tools.ietf.org/html/draft-davidben-http-client-hint-reliability) as defined in the Client Hints Reliability draft.

The code can be found in the following directories:

*   [/components/client_hints/] (This directory)
*   [/content/browser/client_hints/]
*   [/chrome/browser/client_hints/]
*   [/third_party/blink/common/client_hints/]
*   [/services/network/public/mojom/web_client_hints_types.mojom] contains the list of all existing Client Hints. Developers adding a new client hint should start from here.
*   [/third_party/blink/web_tests/external/wpt/client-hints/]

[TOC]

## What Client Hints *are* (and how they work)

Client hints are information about the client that is supplied to origins that signal they want the extra information. Historically, this information was in headers such as `User-Agent`, `Rtt`, or `Device-Memory` and was sent with every request. Client hints contain more precise information than the `User-Agent` header, but require that the origin opt-in to receive that information.

Client hints are sent to the origin as [request headers](https://developer.mozilla.org/en-US/docs/Web/HTTP/Client_hints). UA client hints are also available (in secure contexts) via the [`navigator.userAgentData` property](https://developer.mozilla.org/en-US/docs/Web/API/User-Agent_Client_Hints_API).

### Client-Hint Types

There are several types of client hints, which are handled differently:

 * *UA* client hints contain information about the user agent which might once have been found expected in the User-Agent header.
 * *Device* client hints contain dynamic information about the configuration of the device on which the browser is running.
 * *Network* client hints contain dynamic information about the browser's network connection.
 * *User Preference Media Features* client hints contain information about the user agent's preferences as represented in CSS media features.

All UA client hints are distinguished by a `Sec-CH-UA` header prefix. UA hints are available via the JS `navigator.userAgentData` API, while other types are not. UA hints are further divided into [low- and high-entropy hints](https://wicg.github.io/client-hints-infrastructure/#low-entropy-table). Hints that do not contain enough information to fingerprint a user are considered low-entropy, while the remainder are high-entropy.

Some device client hints are specific to the type of resource being requested. For example, `Sec-CH-Resource-Width` is sent only for image fetches.

Except for `Save-Data`, network client hints are currently being deprecated.

Some device and user-preference client hints are not sent for fetches in detached frames.

### Requesting Client Hint Headers

Client hint headers are requested via a response header (`Accept-CH`). When an origin sends an `Accept-CH` header with a (comma separated) list of client hint headers it would like to receive (on a secure, top-level navigation request) those preferences are stored by the browser. Every subsequent request to an origin will contain those extra client hint request headers, as described in the HTTP Client Hints specification. This cache is cleared when session cookies are cleared, or when a user clears site data or cookies for a given origin.

The response header can also be included in the response HTML using a `meta` element. Note that this only works when the element appears in the downloaded HTML. Adding the element from a script does not work.

### Sub-resource delegation

Every document created with that origin contains those preferences as a “client hint set” and uses that set alongside other settings to decide what client hints to delegate to sub-resource requests associated with that document.

When requests are initiated from a document, the client hints are filtered through [Permission Policies](https://w3c.github.io/webappsec-permissions-policy/), which allows origins to control what features are used by what 3rd parties in a document. By default, the feature policies for client hints (except `Sec-CH-UA` and `Sec-CH-UA-Mobile`) are set to “self,” which means that hints are only delegated to the same origin as the (top-level) document. The permission can also be a list of origins or an asterisk `*` for all origins (`Sec-CH-UA` and `Sec-CH-UA-Mobile` are considered low-entropy and safe to send to all origins, thus their defaults are `*`). Permissions can also be set in HTML for iframes in the same format through the `allow` attribute.

Note: All client hints (top-level and subresource) are gated on JavaScript being enabled in Chrome. While not explicitly stated, it fits into the requirement to only reveal information visible through JavaScript interfaces.

### Client Hint Reliability

There are two situations where a request could have hints that are different from what the origin wanted:

1. The origin and browser’s client hints preferences are out of sync (either because the site has not been visited or because the site’s preferences have changed since the last time), OR
2. The browser does not wish to send the requested client hint (e.g. it goes against user preferences or because of some browser controlled privacy mechanism)

As HTTP Client Hints are defined, there’s no way to know which is the case. Two mechanisms were defined in the [Client Hints Reliability proposal](https://tools.ietf.org/html/draft-davidben-http-client-hint-reliability):

*   an HTTP-header-based retry to ensure critical Client Hints are reliably available (`Critical-CH1)
*   a connection-level optimization to avoid the performance hit of a retry in most cases (ACCEPT_CH)

An explainer for both can be found [here](https://github.com/WICG/client-hints-infrastructure/blob/main/reliability.md)

#### Critical-CH retry mechanism

The `Critical-CH` response header is a signal from the origin that the hints listed are important to have on the very first request (i.e. make a meaningful change to the request). The algorithm is fairly straightforward:

1. Determine what hints would be sent with the newly-received `Accept-CH` header (including user preferences and local policy)
2. Find the difference between those hints and the hints that were sent with the initial request
3. If any hints are present in the `Critical-CH` list that were not sent but would have been:
    1. retry the request with the new client hint set.

#### ACCEPT_CH HTTP2/3 frame and ALPS TLS extension

The ALPS ("Application-Layer Protocol Settings") TLS extension allows applications to add application-layer protocol settings to the TLS handshake, notably *before* the "first" round trip of the application protocol itself.

For the purposes of Client-Hints, this means that an "ACCEPT_CH" HTTP2/3 frame can be added to the handshake, which sets a connection-level addition to the "Accept-CH" cache, meaning the browser can see these settings and reach appropriately *before* sending the request to a server.

The full explanation is outside of the scope of this document and can be found in the reliability explainer linked above.

### Header Names

All client hint headers should be [forbidden request-headers](https://fetch.spec.whatwg.org/#forbidden-request-header), by virtue of the prefix `Sec-`. Historically, some were not forbidden and most of these have been replaced with new headers using this prefix. Within Chromium, these are distinguished with a `_DEPRECATED` suffix, e.g., `WebClientHintsType::kDeviceMemory_DEPRECATED`. The exception to this rule is the `Save-Data` header, which [will
not be replaced with `Sec-CH-Save-Data`](https://groups.google.com/a/chromium.org/g/blink-dev/c/HR7tWmewbSA/m/R0QYg-ZAAAAJ).

The "new" naming adds `CH` to distinguish client hints from other forbidden request-headers, giving the combined prefix `Sec-CH`.

## Implementation

### Client-Hint Data Sources

In terms of the implementation, the web sees client hints in three ways: in headers on navigation-related fetches, in headers for subresource fetches, and via the [`navigator.userAgentData` property](https://developer.mozilla.org/en-US/docs/Web/API/Navigator/userAgentData).

The first of these occurs in the browser process. Hint data for client-hint types other than UA are fetched from appropriate APIs, such as `NetworkQualityTracker` or `display::Screen`. Data for UA client hints are gathered in a `UserAgentMetadata` type, which is acquired from the embedder via the `ClientHintControllerDelegate`.

Subresource fetches and JS access occur in the renderer. The renderer gets UA client hint data from the browser via `RenderThreadImpl::InitializeRenderer` and stores it for the lifetime of the renderer thread. The browser gets this data from the embedder via the `ContentBrowserClient` delegate. *Note that this is entirely independent of the `ClientHintControllerDelegate`!*

Programmatic access only exposes UA client hints. All other client hints are only revealed in client-hint headers, based on locally-observed information such as that from `NetworkStateNotifier` or the frame's device pixel ratio.

### Accept-CH cache

Client Hint preferences are stored in the preferences service as a content setting (`ContentSettingsType::CLIENT_HINTS`), keyed to the origin. This storage is accessed through the [content::ClientHintsControllerDelegate] interface, with the principle implementation being [client_hints::ClientHints] in //components (to share across multiple platforms). The delegate is accessible in the browser process as a property of the [content::BrowserContext] (in //chrome land, this is implemented as the Profile and “Off The Record” Profile. An important note is that there is an “incognito profile” that gets its own client hints storage).

This storage is marked as `content_settings::mojom::SessionModel::DURABLE`. This means that the client hint settings are read in from disk on browser start up and loaded into memory. Practically, this means that the client hint settings persist until the user clears site data or cookies for the origin.

The code for reading from and writing to the client hint preferences in content is in [/content/browser/client_hints/client_hints.cc]

The preferences are read on the construction of a `ClientHintsExtendedData` object, which then will use the `FrameTreeNode` (which is where the object gets first party origin and permission policy information) and client hints preferences to calculate what hints should be sent for a given request.

The preferences are written in `ParseAndPersistAcceptCHForNavigation`, which is also where various settings (secure context, JS permissions, feature flags set) are checked before sending the information to the controller delegate.

### Client Hints Infrastructure

The client hints set is passed into the document on commit from [NavigationRequest::CommitNavigation](/content/browser/renderer_host/navigation_request.cc) to the document and is used in [FrameFetchContext::AddClientHintsIfNecessary](/third_party/blink/renderer/core/loader/frame_fetch_context.cc), where all of the relevant client hint information gets filled into the headers to be sent.

### Critical-CH response header

The Critical-CH retry mechanism is implemented as [content::CriticalClientHintsThrottle] and all of the important logic is in `WillProcessResponse`. When a retry situation is found (and the `redirected_` flag isn’t set) the header is stored, the new hints are stored as normal and the request is “restarted” (i.e. the response is changed to an internal redirect to the same location, which is also what DevTools sees).

### ACCEPT_CH restart

The ACCEPT_CH restart mechanism is implemented with the navigation stack. The mojo interface [network::mojom::AcceptCHFrameObserver](/services/network/public/mojom/accept_ch_frame_observer.mojom) is implemented by [content::NavigationURLLoaderImpl](/content/browser/loader/navigation_url_loader_impl.h) and the resulting pipe is passed to the URLLoaderFactory and so on through the [network::ResourceRequest](/services/network/public/cpp/resource_request.h) and related mojo interface.

On the network side, when a TLS socket is selected (either created or re-used from the socket pool) that contains an ACCEPT_CH ALPS frame, it's checked against the headers in the request in [ComputeAcceptCHFrame](/services/network/url_loader.cc), which removes any tokens from the ACCEPT_CH frame that are already present in the request as headers and checks if the result is empty.

If the result is not empty, it's passed through the AcceptCHFrameObserver mojo pipe (back to content::NavigationURLLoaderImpl) to the browser, and if new headers are added the and navigation is restarted.

## Testing

### Incognito mode

Any storage related to an incognito mode profile is cleared when the last incognito tab is closed, including client hint preferences.

### `--initial-client-hint-storage` Command Line Switch

A command line flag is available for testing to pre-populate the Accept-CH cache. It takes a json dictionary, where each key is an origin and each value is a string in the same format as the Accept-CH response header.

Each new profile will include these pre-populated preferences *except* for "Off The Record" profiles (e.g. guest profiles and incognito profiles).

**Note:** Don't forget to escape quotes if your shell needs it.

An example use might be:

```
out/default/chrome --initial-client-hint-storage="{\"https://a.test\":\"Sec-CH-UA-Full-Version\", \"https://b.test\":\"Sec-CH-UA-Model\"}"
```

## Adding a new hint

There are two main steps to adding a hint to Chromium: adding the token, and populating the value when appropriate. You will probably also want a feature to control use of the hint.
Find an example of adding a new UA hint in https://crrev.com/c/4628277.

### Adding a feature

Add a new Blink feature, named `ClientHints<HintName>` (without `Sec-CH-UA`) to [/third_party/blink/public/common/features.h] and [/third_party/blink/common/features.cc].

### Adding a new client hint token

The canonical enum for client hint tokens is [network::mojom::WebClientHintsType]. Any new token should be added to the end of that list. Along with that:

*   Add the header name to the map in `MakeClientHintToNameMap` in [/services/network/public/cpp/client_hints.cc].
*   Add an enum value to `WebFeature` in [/third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom] and run [/tools/metrics/histograms/update_use_counter_feature_enum.py] to update the enum in [/tools/metrics/histograms/enums.xml].
*   Map the hint to the web feature in `MakeClientHintToWebFeatureMap` in [/third_party/blink/renderer/core/loader/frame_client_hints_preferences_context.cc].
*   Update the `static_assert` for `network::mojom::WebClientHintsType::kMaxValue` in [/content/browser/client_hints/client_hints.cc], leaving a TODO for your implementation.
*   Add a permissions policy in [/third_party/blink/renderer/core/permissions_policy/permissions_policy_features.json5]. Note that the entries in this file are in lexical order by name.
*   Add an entry to the end of [/third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom] and run [/tools/metrics/histograms/update_permissions_policy_enum.py] to update [tools/metrics/histograms/enums.xml].
*   Map the hint to the permissions policy in `MakeClientHintToPolicyFeatureMap` in [/third_party/blink/common/client_hints/client_hints.cc].
*   Add the permissions policy name to:
    * `third_party/blink/public/devtools_protocol/browser_protocol.pdl`
    * `third_party/blink/web_tests/virtual/stable/webexposed/feature-policy-features-expected.txt`
    * `third_party/blink/web_tests/webexposed/feature-policy-features-expected.txt`
*   Update [/services/network/public/cpp/cors/cors.cc] to include this value in the "safe names", including a brief comment describing the header. Update [/services/network/public/cpp/cors/cors_unittest.cc] to correspond (note that it uses the Title-Cased header name).

If gating the new hint on a feature:
*   Add a conditional to `VerifyClientHintsReceived` in [/chrome/browser/client_hints/client_hints_browsertest.cc] to skip your new header when the feature is not enabled.
*   Add a conditional to `IsDisabledByFeature` in [/third_party/blink/common/client_hints/enabled_client_hints.cc], based on your new feature.

TODO(crbug.com/40168503): There should be UseCounters measuring usage, but there are not currently.

At this point, if the hint is gated on a feature, tests should pass when that feature is disabled. In particular, check that `content_unittests` passes tests matching `*Cors*`, and `browser_tests` passes tests matching `*ClientHints*`.

### Populating the Client Hint

As described in "Client-Hint Data Sources" above, client hints are populated in
several places:

 * _UA Hints:_ Included in [`blink::UserAgentMetadata`](/third_party/blink/public/common/user_agent/user_agent_metadata.h) and determined in [`ClientHintsControllerDelegate`](/content/public/browser/client_hints_controller_delegate.h) for navigation fetches and [`ContentBrowserClient`](/content/public/browser/content_browser_client.h) for programmatic access and subresource fetches.
 * _navigation fetches:_ [`content::AddNavigationRequestClientHintsHeaders`](/content/browser/client_hints/client_hints.cc) and `content::AddRequestClientHintsHeaders`.
 * _subresource fetches:_ [BaseFetchContext::AddClientHintsIfNecessary](/third_party/blink/renderer/core/loader/base_fetch_context.cc). If you need frame-based information, this should be added to [ClientHintsImageInfo](/third_party/blink/renderer/core/loader/base_fetch_context.cc), which is populated in [FrameFetchContext::AddClientHintsIfNecessary](/third_party/blink/renderer/core/loader/frame_fetch_context.cc)

After updating these sites to populate the hint, update the various tests for this new header. This includes WPTs, Webview Java tests, and browsertests.

### Devtools Backend

* Any addition to [`blink::UserAgentMetadata`](/third_party/blink/public/common/user_agent/user_agent_metadata.h) also needs to extend the related Chrome Devtools Protocol API calls, namely `setUserAgentOverride`. The backend implementation can be found in [/third_party/blink/renderer/core/inspector/inspector_emulation_agent.h], and the UserAgentMetadata type in [/third_party/blink/public/devtools_protocol/browser_protocol.pdl] will also need to be extended.
* Update overridden function `SetUserAgentOverride` in [/third_party/blink/renderer/core/inspector/inspector_emulation_agent.cc], and [/content/browser/devtools/protocol/emulation_handler.cc].
* Add the new client hint to [/third_party/blink/web_tests/http/tests/inspector-protocol/emulation/resources/set-accept-ch.php] and update tests in [/third_party/blink/web_tests/http/tests/inspector-protocol/emulation/emulation-user-agent-metadata-override.js].

### Devtools Frontend

Devtools frontend source code is in a different branch [devtools/devtools-frontend](https://chromium.googlesource.com/devtools/devtools-frontend).

* Any addition to [blink::UserAgentMetadata] also needs to extend the related type `UserAgentMetadata` in [/third_party/devtools-frontend/src/third_party/blink/public/devtools_protocol/browser_protocol.pdl], [/third_party/devtools-frontend/src/third_party/blink/public/devtools_protocol/browser_protocol.json], and [/third_party/devtools-frontend/src/front_end/generated/protocol.d.ts].
* Add the permission policy token to the `PermissionsPolicyFeature` enum in [/third_party/devtools-frontend/src/third_party/blink/public/devtools_protocol/browser_protocol.pdl], and [/third_party/devtools-frontend/src/third_party/blink/public/devtools_protocol/browser_protocol.json].

<!-- links -->
[/android_webview/javatests/src/org/chromium/android_webview/test/ClientHintsTest.java]: /android_webview/javatests/src/org/chromium/android_webview/test/ClientHintsTest.java
[/chrome/browser/client_hints/]: /chrome/browser/client_hints/
[/chrome/browser/client_hints/client_hints_browsertest.cc]: /chrome/browser/client_hints/client_hints_browsertest.cc
[/chrome/test/data/client_hints/]: /chrome/test/data/client_hints/
[client_hints::ClientHints]: /components/client_hints/browser/client_hints.h
[/components/client_hints/]: /components/client_hints/
[/content/browser/client_hints/client_hints.cc]: /content/browser/client_hints/client_hints.cc
[/content/browser/client_hints/]: /content/browser/client_hints/
[content::BrowserContext]: /content/public/browser/browser_context.h
[/content/browser/devtools/protocol/emulation_handler.cc]: /content/browser/devtools/protocol/emulation_handler.cc
[content::ClientHintsControllerDelegate]: /content/public/browser/client_hints_controller_delegate.h
[content::CriticalClientHintsThrottle]: /content/browser/client_hints/critical_client_hints_throttle.h
[network::mojom::WebClientHintsType]: /services/network/public/mojom/web_client_hints_types.mojom
[/services/network/public/cpp/client_hints.cc]: /services/network/public/cpp/client_hints.cc
[/services/network/public/cpp/cors/cors.cc]: /services/network/public/cpp/cors/cors.cc
[/services/network/public/cpp/cors/cors_unittest.cc]: /services/network/public/cpp/cors/cors_unittest.cc
[/services/network/public/mojom/web_client_hints_types.mojom]: /services/network/public/mojom/web_client_hints_types.mojom
[/third_party/blink/common/client_hints/client_hints.cc]: /third_party/blink/common/client_hints/client_hints.cc
[/third_party/blink/common/client_hints/enabled_client_hints.cc]: /third_party/blink/common/client_hints/enabled_client_hints.cc
[/third_party/blink/common/client_hints/]: /third_party/blink/common/client_hints/
[/third_party/blink/common/features.cc]: /third_party/blink/common/features.cc
[/third_party/blink/public/common/features.h]: /third_party/blink/public/common/features.h
[/third_party/blink/public/devtools_protocol/browser_protocol.pdl]: /third_party/blink/public/devtools_protocol/browser_protocol.pdl
[/third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom]: /third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom
[/third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom]: /third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom
[/third_party/blink/renderer/core/inspector/inspector_emulation_agent.cc]: /third_party/blink/renderer/core/inspector/inspector_emulation_agent.cc
[/third_party/blink/renderer/core/inspector/inspector_emulation_agent.h]: /third_party/blink/renderer/core/inspector/inspector_emulation_agent.h
[/third_party/blink/renderer/core/loader/frame_client_hints_preferences_context.cc]: /third_party/blink/renderer/core/loader/frame_client_hints_preferences_context.cc
[/third_party/blink/renderer/core/permissions_policy/permissions_policy_features.json5]: /third_party/blink/renderer/core/permissions_policy/permissions_policy_features.json5
[/third_party/blink/web_tests/external/wpt/client-hints/resources/clienthintslist.py]: /third_party/blink/web_tests/external/wpt/client-hints/resources/clienthintslist.py
[/third_party/blink/web_tests/external/wpt/client-hints/resources/export.js]: /third_party/blink/web_tests/external/wpt/client-hints/resources/export.js
[/third_party/blink/web_tests/external/wpt/client-hints/]: /third_party/blink/web_tests/external/wpt/client-hints/
[/third_party/blink/web_tests/http/tests/inspector-protocol/emulation/emulation-user-agent-metadata-override.js]: /third_party/blink/web_tests/http/tests/inspector-protocol/emulation/emulation-user-agent-metadata-override.js
[/third_party/blink/web_tests/http/tests/inspector-protocol/emulation/resources/set-accept-ch.php]: /third_party/blink/web_tests/http/tests/inspector-protocol/emulation/resources/set-accept-ch.php
[/third_party/blink/web_tests/virtual/stable/webexposed/feature-policy-features-expected.txt]: /third_party/blink/web_tests/virtual/stable/webexposed/feature-policy-features-expected.txt
[/third_party/blink/web_tests/webexposed/feature-policy-features-expected.txt]: /third_party/blink/web_tests/webexposed/feature-policy-features-expected.txt
[/third_party/devtools-frontend/src/front_end/generated/InspectorBackendCommands.js]: https://chromium.googlesource.com/devtools/devtools-frontend/+/main/front_end/generated/InspectorBackendCommands.js
[/third_party/devtools-frontend/src/front_end/generated/protocol.d.ts]: /third_party/devtools-frontend/src/front_end/generated/protocol.d.ts
[/third_party/devtools-frontend/src/third_party/blink/public/devtools_protocol/browser_protocol.json]: https://chromium.googlesource.com/devtools/devtools-frontend/+/main/third_party/blink/public/devtools_protocol/browser_protocol.json
[/third_party/devtools-frontend/src/third_party/blink/public/devtools_protocol/browser_protocol.pdl]: https://chromium.googlesource.com/devtools/devtools-frontend/+/main/third_party/blink/public/devtools_protocol/browser_protocol.pdl
[/tools/metrics/histograms/enums.xml]: /tools/metrics/histograms/enums.xml
[tools/metrics/histograms/enums.xml]: tools/metrics/histograms/enums.xml
[/tools/metrics/histograms/update_permissions_policy_enum.py]: /tools/metrics/histograms/update_permissions_policy_enum.py
[/tools/metrics/histograms/update_use_counter_feature_enum.py]: /tools/metrics/histograms/update_use_counter_feature_enum.py
