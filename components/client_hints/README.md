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

HTTP Client Hints are request headers that can be optionally sent to origins that signal they want extra information via a response header (`Accept-CH`). When an origin sends an `Accept-CH` header with a (comma separated) list of client hint headers it would like to receive (on a secure top-level navigation request) those preferences are stored by the browser. Every subsequent request to an origin will contain those extra client hint request headers, as described in the HTTP Client Hints specification. This cache is cleared when session cookies are cleared, or when a user clears site data or cookies for a given origin.

### Sub-resource delegation

Every document created with that origin contains those preferences as a “client hint set” and uses that set alongside other settings to decide what client hints to delegate to sub-resource requests associated with that document.

When requests are initiated from a document, the client hints are filtered through [Permission Policies](https://w3c.github.io/webappsec-permissions-policy/), which allows origins to control what features are used by what 3rd parties in a document. By default, the feature policies for client hints (except `Sec-CH-UA` and `Sec-CH-UA-Mobile`) are set to “self,” which means that hints are only delegated to the same origin as the (top-level) document. The permission can also be a list of origins or an asterisk `*` for all origins (`Sec-CH-UA` and `Sec-CH-UA-Mobile` are considered “[low-entropy](https://wicg.github.io/client-hints-infrastructure/#low-entropy-hint-table)” and safe to send to all origins, thus their defaults are `*`). Permissions can also be set in HTML for iframes in the same format through the `allow` attribute.

Note: All client hints (top-level and subresource) are gated on JavaScript being enabled in Chrome. While not explicitly stated, it fits into the requirement to only reveal information visible through JavaScript interfaces.

### Client Hint Reliability

There are two situations where a request could have hints that are different from what the origin wanted:

1. The origin and browser’s client hints preferences are out of sync (either because the site has not been visited or because the site’s preferences have changed since the last time), OR
2. The browser does not wish to send the requested client hint (e.g. it goes against user preferences or because of some browser controlled privacy mechanism)

As HTTP Client Hints are defined, there’s no way to know which is the case. Two mechanisms were defined in the [Client Hints Reliability proposal](https://tools.ietf.org/html/draft-davidben-http-client-hint-reliability):

*   an HTTP-header-based retry to ensure critical Client Hints are reliably available ("Critical-CH")
*   a connection-level optimization to avoid the performance hit of a retry in most cases ("ACCEPT_CH")

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

## Implementation

### Accept-CH cache

Client Hint preferences are stored in the preferences service as a content setting (`ContentSettingsType::CLIENT_HINTS`), keyed to the origin. This storage is accessed through the [content::ClientHintsControllerDelegate] interface, with the principle implementation being [client_hints::ClientHints] in //components (to share across multiple platforms). The delegate is accessible in the browser process as a property of the [content::BrowserContext] (in //chrome land, this is implemented as the Profile and “Off The Record” Profile. An important note is that there is an “incognito profile” that gets its own client hints storage).

This storage is marked as `content_settings::SessionModel::Durable`. This means that the client hint settings are read in from disk on browser start up and loaded into memory. Practically, this means that the client hint settings persist until the user clears site data or cookies for the origin.

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

There’s two main steps to adding a hint to Chromium: adding the token, and populating the value when appropriate

### Adding a new client hint token

The canonical enum for client hint tokens is [network::mojom::WebClientHintsType]. Any new token should be added to the end of that list. Along with that:

*   Add the header name to the map in `MakeClientHintToNameMap` in [/services/network/public/cpp/client_hints.cc].
*   Add an enum value to `WebFeature` in [/third_party/blink/public/mojom/web_feature/web_feature.mojom].
*   Add the feature enum to the map in `MakeClientHintToWebFeatureMap` in [/third_party/blink/renderer/core/loader/frame_client_hints_preferences_context.cc].
*   Add the client hint header to the `Accept-CH` header in the appropriate test files in [/chrome/test/data/client_hints/] and [/third_party/blink/web_tests/external/wpt/client-hints].
*   Update `expected_client_hints_number` to the current value + 1 in [/chrome/browser/client_hints/client_hints_browsertest.cc].

**NOTE:** It’s very important that the order of these arrays remain in sync.

There should also be a new feature policy created:

*   Define the permission policy in [/third_party/blink/renderer/core/permissions_policy/permissions_policy_features.json5].
*   Add an enum to [/third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom].
*   Add the same enum to the map in `MakeClientHintToPolicyFeatureMap` in [/third_party/blink/common/client_hints/client_hints.cc].
*   Add the permission policy token to the `PermissionsPolicyFeature` enum in [/third_party/blink/public/devtools_protocol/browser_protocol.pdl], [/third_party/devtools-frontend/src/third_party/blink/public/devtools_protocol/browser_protocol.pdl], and [/third_party/devtools-frontend/src/third_party/blink/public/devtools_protocol/browser_protocol.json].
*   Add the permission policy token to [/third_party/blink/web_tests/webexposed/feature-policy-features-expected.txt] and [/third_party/blink/web_tests/virtual/stable/webexposed/feature-policy-features-expected.txt].

The header should also be added to the cors `safe_names` list in [/services/network/public/cpp/cors/cors.cc](/services/network/public/cpp/cors/cors.cc) and update its test.

TODO(crbug.com/1176808): There should be UseCounters measuring usage, but there are not currently.

### Populating the client hint

Client Hints are populated in [BaseFetchContext::AddClientHintsIfNecessary](/third_party/blink/renderer/core/loader/base_fetch_context.cc). If you need frame-based information, this should be added to [ClientHintsImageInfo](/third_party/blink/renderer/core/loader/base_fetch_context.cc), which is populated in [FrameFetchContext::AddClientHintsIfNecessary](/third_party/blink/renderer/core/loader/frame_fetch_context.cc)

### Web platform tests
* Add the new client hint to [/third_party/blink/web_tests/external/wpt/client-hints/resources/export.js], [/third_party/blink/web_tests/external/wpt/client-hints/resources/clienthintslist.py], [/third_party/blink/web_tests/external/wpt/client-hints/accept-ch/feature-policy-navigation/\_\_dir\_\_.headers], [/third_party/blink/web_tests/external/wpt/client-hints/sandbox/\_\_dir\_\_.headers], and [/third_party/blink/web_tests/external/wpt/client-hints/accept-ch/\_\_dir\_\_.headers]

### Devtools Backend

* Any addition to [blink::UserAgentMetadata](/third_party/blink/public/common/user_agent/user_agent_metadata.h) also needs to extend the related Chrome Devtools Protocol API calls, namely `setUserAgentOverride`. The backend implementation can be found in [/third_party/blink/renderer/core/inspector/inspector_emulation_agent.h], and the UserAgentMetadata type in [/third_party/blink/public/devtools_protocol/browser_protocol.pdl] will also need to be extended.
* Update overridden function `SetUserAgentOverride` in [/third_party/blink/renderer/core/inspector/inspector_emulation_agent.cc], and [/content/browser/devtools/protocol/emulation_handler.cc].
* Add the new client hint to [/third_party/blink/web_tests/http/tests/inspector-protocol/emulation/resources/set-accept-ch.php] and update tests in [/third_party/blink/web_tests/http/tests/inspector-protocol/emulation/emulation-user-agent-metadata-override.js].

### Devtools Frontend

Devtools frontend source code is in a different branch [devtools/devtools-frontend](https://chromium.googlesource.com/devtools/devtools-frontend).

* Any addition to [blink::UserAgentMetadata] also needs to extend the related type `UserAgentMetadata` in [/third_party/devtools-frontend/src/third_party/blink/public/devtools_protocol/browser_protocol.pdl], [/third_party/devtools-frontend/src/third_party/blink/public/devtools_protocol/browser_protocol.json], and [/third_party/devtools-frontend/src/front_end/generated/protocol.d.ts].
* Add the permission policy token to the `PermissionsPolicyFeature` enum in [/third_party/devtools-frontend/src/third_party/blink/public/devtools_protocol/browser_protocol.pdl], and [/third_party/devtools-frontend/src/third_party/blink/public/devtools_protocol/browser_protocol.json].

<!-- links -->
[/components/client_hints/]: /components/client_hints/
[/content/browser/client_hints/]: /content/browser/client_hints/
[/chrome/browser/client_hints/]: /chrome/browser/client_hints/
[/third_party/blink/common/client_hints/]: /third_party/blink/common/client_hints/
[/services/network/public/mojom/web_client_hints_types.mojom]: /services/network/public/mojom/web_client_hints_types.mojom
[/third_party/blink/web_tests/external/wpt/client-hints/]: /third_party/blink/web_tests/external/wpt/client-hints/
[content::ClientHintsControllerDelegate]: /content/public/browser/client_hints_controller_delegate.h
[client_hints::ClientHints]: /components/client_hints/browser/client_hints.h
[content::BrowserContext]: /content/public/browser/browser_context.h
[/content/browser/client_hints/client_hints.cc]: /content/browser/client_hints/client_hints.cc
[content::CriticalClientHintsThrottle]: /content/browser/client_hints/critical_client_hints_throttle.h
[network::mojom::WebClientHintsType]: /services/network/public/mojom/web_client_hints_types.mojom
[/services/network/public/cpp/client_hints.cc]: /services/network/public/cpp/client_hints.cc
[/third_party/blink/common/client_hints/client_hints.cc]: /third_party/blink/common/client_hints/client_hints.cc
[/third_party/blink/public/mojom/web_feature/web_feature.mojom]: /third_party/blink/public/mojom/web_feature/web_feature.mojom
[/third_party/blink/renderer/core/loader/frame_client_hints_preferences_context.cc]: /third_party/blink/renderer/core/loader/frame_client_hints_preferences_context.cc
[/chrome/test/data/client_hints/]: /chrome/test/data/client_hints/
[/third_party/blink/web_tests/external/wpt/client-hints]: /third_party/blink/web_tests/external/wpt/client-hints
[/chrome/browser/client_hints/client_hints_browsertest.cc]: /chrome/browser/client_hints/client_hints_browsertest.cc
[/third_party/blink/renderer/core/permissions_policy/permissions_policy_features.json5]: /third_party/blink/renderer/core/permissions_policy/permissions_policy_features.json5
[/third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom]: /third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom
[/third_party/blink/public/devtools_protocol/browser_protocol.pdl]: /third_party/blink/public/devtools_protocol/browser_protocol.pdl
[/third_party/devtools-frontend/src/third_party/blink/public/devtools_protocol/browser_protocol.pdl]: https://chromium.googlesource.com/devtools/devtools-frontend/+/main/third_party/blink/public/devtools_protocol/browser_protocol.pdl
[/third_party/devtools-frontend/src/third_party/blink/public/devtools_protocol/browser_protocol.json]: https://chromium.googlesource.com/devtools/devtools-frontend/+/main/third_party/blink/public/devtools_protocol/browser_protocol.json
[/third_party/devtools-frontend/src/front_end/generated/InspectorBackendCommands.js]: https://chromium.googlesource.com/devtools/devtools-frontend/+/main/front_end/generated/InspectorBackendCommands.js
[/third_party/blink/web_tests/webexposed/feature-policy-features-expected.txt]: /third_party/blink/web_tests/webexposed/feature-policy-features-expected.txt
[/third_party/blink/web_tests/virtual/stable/webexposed/feature-policy-features-expected.txt]: /third_party/blink/web_tests/virtual/stable/webexposed/feature-policy-features-expected.txt
[/third_party/blink/renderer/core/inspector/inspector_emulation_agent.h]: /third_party/blink/renderer/core/inspector/inspector_emulation_agent.h
[/third_party/blink/renderer/core/inspector/inspector_emulation_agent.cc]: /third_party/blink/renderer/core/inspector/inspector_emulation_agent.cc
[/content/browser/devtools/protocol/emulation_handler.cc]: /content/browser/devtools/protocol/emulation_handler.cc
[/third_party/blink/web_tests/http/tests/inspector-protocol/emulation/resources/set-accept-ch.php]: /third_party/blink/web_tests/http/tests/inspector-protocol/emulation/resources/set-accept-ch.php
[/third_party/blink/web_tests/http/tests/inspector-protocol/emulation/emulation-user-agent-metadata-override.js]: /third_party/blink/web_tests/http/tests/inspector-protocol/emulation/emulation-user-agent-metadata-override.js
[/third_party/blink/web_tests/external/wpt/client-hints/resources/export.js]: /third_party/blink/web_tests/external/wpt/client-hints/resources/export.js
[/third_party/blink/web_tests/external/wpt/client-hints/resources/clienthintslist.py]: /third_party/blink/web_tests/external/wpt/client-hints/resources/clienthintslist.py
[/third_party/blink/web_tests/external/wpt/client-hints/accept-ch/feature-policy-navigation/\_\_dir\_\_.headers]: /third_party/blink/web_tests/external/wpt/client-hints/accept-ch/feature-policy-navigation/__dir__.headers
[/third_party/blink/web_tests/external/wpt/client-hints/sandbox/\_\_dir\_\_.headers]: /third_party/blink/web_tests/external/wpt/client-hints/sandbox/__dir__.headers
[/third_party/blink/web_tests/external/wpt/client-hints/accept-ch/\_\_dir\_\_.headers]: /third_party/blink/web_tests/external/wpt/client-hints/accept-ch/__dir__.headers