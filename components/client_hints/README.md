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

*   an HTTP-header-based retry to ensure critical Client Hints are reliably available
*   a connection-level optimization to avoid the performance hit of a retry in most cases

Currently only the former, the Critical-CH retry mechanism, is implemented.

#### Critical-CH retry mechanism

The `Critical-CH` response header is a signal from the origin that the hints listed are important to have on the very first request (i.e. make a meaningful change to the request). The algorithm is fairly straightforward:

1. Determine what hints would be sent with the newly-received `Accept-CH` header (including user preferences and local policy)
2. Find the difference between those hints and the hints that were sent with the initial request
3. If any hints are present in the `Critical-CH` list that were not sent but would have been:
    1. retry the request with the new client hint set.

## Implementation

### Accept-CH cache

Client Hint preferences are stored in the preferences service as a content setting (`ContentSettingsType::CLIENT_HINTS`), keyed to the origin. This storage is accessed through the [content::ClientHintsControllerDelegate] interface, with the principle implementation being [client_hints::ClientHints] in //components (to share across multiple platforms). The delegate is accessible in the browser process as a property of the [content::BrowserContext] (in //chrome land, this is implemented as the Profile and “Off The Record” Profile. An important note is that there is an “incognito profile” that gets its own client hints storage).

This storage is marked as `content_settings::SessionModel::UserSession`. This means that when settings are read in from disk (on browser start up) there’s also a check for a flag that’s set on graceful shutdown. (This is to exclude crashes and browser updates). If that flag is set, the settings are cleared. Practically, this means that the settings are cleared after closing the browser.

The code for reading from and writing to the client hint preferences in content is in [/content/browser/client_hints/client_hints.cc]

The preferences are read on the construction of a `ClientHintsExtendedData` object, which then will use the `FrameTreeNode` (which is where the object gets first party origin and permission policy information) and client hints preferences to calculate what hints should be sent for a given request.

The preferences are written in `ParseAndPersistAcceptCHForNavigation`, which is also where various settings (secure context, JS permissions, feature flags set) are checked before sending the information to the controller delegate.

### Client Hints Infrastructure

The client hints set is passed into the document on commit from [NavigationRequest::CommitNavigation](/content/browser/renderer_host/navigation_request.cc) to the document and is used in [FrameFetchContext::AddClientHintsIfNecessary](third_party/blink/renderer/core/loader/frame_fetch_context.cc), where all of the relevant client hint information filled into the headers to be sent.

### Critical-CH response header

The Critical-CH retry mechanism is implemented as [content::CriticalClientHintsThrottle] and all of the important logic is in `WillProcessResponse`. When a retry situation is found (and the `redirected_` flag isn’t set) the header is stored, the new hints are added to the request, and the request is “restarted” (i.e. the request is aborted and a new one is started).

## Adding a new hint

There’s two main steps to adding a hint to Chromium: adding the token, and populating the value when appropriate

### Adding a new client hint token

The canonical enum for client hint tokens is [network::mojom::WebClientHintsType]. Any new token should be added to the end of that list. Along with that, a string of the token/header name should be added to:

*   `kClientHintsNameMapping` in [/services/network/public/cpp/client_hints.cc]
*   `kClientHintsHeaderMapping` in [/third_party/blink/common/client_hints/client_hints.cc]

**NOTE:** It’s very important that the order of these arrays remain in sync.

There should also be a new feature policy created, which should go in [/third_party/blink/renderer/core/feature_policy/feature_policy_features.json5](/third_party/blink/renderer/core/feature_policy/feature_policy_features.json5), and the header should be added to the cors `safe_names` list in [/services/network/public/cpp/cors/cors.cc](/services/network/public/cpp/cors/cors.cc)

TODO(crbug.com/1176808): There should be UseCounters measuring usage, but there are not currently.

### Populating the client hint

Client Hints are populated in [BaseFetchContext::AddClientHintsIfNecessary](/third_party/blink/renderer/core/loader/base_fetch_context.cc). If you need frame-based information, this should be added to [ClientHintsImageInfo](/third_party/blink/renderer/core/loader/base_fetch_context.cc), which is populated in [FrameFetchContext::AddClientHintsIfNecessary](/third_party/blink/renderer/core/loader/frame_fetch_context.cc)

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
