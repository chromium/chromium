# IP Protection

This directory contains generic IP Protection code that can be used by
different platforms.

## Overview

IP Protection prevents disclosure of the client's IP address to selected origin
servers by proxying that traffic. It uses two chained proxies, `[proxyA,
proxyB]`, in a layered fashion to avoid disclosure of both client and
origin-server identities to any single proxy server.

The *Masked Domain List* (MDL) determines whether to proxy a specific request.

The identities of `proxyA` and `proxyB` are determined by the proxy
configuration, which is obtained from a server at runtime.

Each new connection to a proxy requires an authentication token, the
implementation pre-fetches these tokens in batches to avoid adding latency to
requests. These tokens are blind-signed, meaning that the signer cannot
correlate the (user-authenticated) token signature with the (otherwise
un-authenticated) token sent to the proxy.

*Probabilistic Reveal Tokens* (PRT) are added for IP-Protected requests to
registered domains (through PRT Registry) to be used for measuring and
preventing fraud. PRTs are obtained from
`net::features::kProbabilisticRevealTokenServer` through request/response
pair defined in `get_probabilistic_reveal_token.proto`. PRTs are stored in
`IpProtectionProbabilisticRevealTokenManager::crypter_` of type
`IpProtectionProbabilisticRevealTokenCrypter`. Randomized tokens are
retrieved using `IpProtectionProbabilisticRevealTokenManager::GetToken()`.
Check `GetToken()` docstring for details of token randomization.
Unlike authentication tokens PRTs can be re-used, see `GetToken()` docstring.

Domains are registered to receive PRTs through following the registration
process. Registration list is obtained through component updater.
For more details see [PRT explainer](https://github.com/GoogleChrome/ip-protection/blob/main/prt_explainer.md#probabilistic-reveal-tokens).

See [the IP Protection
explainer](https://github.com/GoogleChrome/ip-protection/blob/main/README.md)
for more details on the design.

## Bug Tracker

Find or file bugs for IP Protection in the [Chromium Tracker "IP Protection"
component](https://issues.chromium.org/u/1/issues?q=status:open%20componentid:1456782&s=created_time:desc).

## Guide to the Implementation

### General Principles

IP Protection runs primarily in the network service, where it can provide
low-latency support for its primary task.

IP Protection is used in contexts that do not implement IPC (Cronet), and thus
all of the functionality is available in IPC-free implementations, with the IPC
support added in subclasses which are built only on compatible platforms.

To support testability, several classes such as `IpProtectionCore` have an
abstract base class with only one production implementation. This allows tests
for adjacent functionality to use simple mock subclasses.

### Core

The implementation is centered on `IpProtectionCore`, which provides methods
for checking whether to proxy a request, getting tokens, getting the list of
proxies, among other things. One instance of this class exists for each network
context where IP protection is allowed.

### Managers

The core uses "managers" to handle each of its three primary jobs -- tokens,
proxy config, and the MDL. The first two are owned by the core, while the MDL
is owned by the network service and shared among multiple core instances,
saving substantial memory usage. The core uses one token manager for each proxy
layer.

### Proxy Delegate

The core's primary user is the `IpProtectionProxyDelegate`. This object
intercepts HTTP requests at two points: `OnResolveProxy` is called for each
request, and `OnBeforeTunnelRequest` is called when establishing a tunnel to a
proxy.

The delegate's`OnResolveProxy` determines whether the request should be
proxied, and if so configures the proxy chain for the request. If the network
stack determines that a new proxy connection is required,
`OnBeforeTunnelRequest` adds an authentication token to the request headers.

The delegate's `OnResolveProxy` adds a PRT to the request headers for the
proxied requests to the registered domains.

#### QUIC Fallback

While we would prefer to use QUIC (HTTP/3) for connections to the proxy, this
is not always possible. In order to detect this situation, the proxy delegate
inserts both QUIC and HTTP proxies into the proxy list. If the QUIC connection
to a pair of proxies fails, but the fallback to an HTTP connection to the same
proxies succeeds, it "locks" into a mode where it uses only HTTP proxies until
the network changes.

### Fetchers

The core's managers use abstract "fetchers" to fetch the information they need.
The concrete types actually used depend on the platform in use. In particular,
the direct fetchers fetch the data directly from the external source. The mojo
fetchers call a remote core host which delegates to these direct fetchers.

### Mojo

IPC is used both for calls from the network service to the browser (fetching)
and the reverse (such as when browser settings change).

In the browser process, the `IpProtectionCoreHost` class manages the necessary
functionality. It implements the `ip_protection::mojo::CoreHost` type and
implements methods to fetch data using privileged information such as user
credentials. In the network service, purpose-specific fetchers (with `Mojo` in
the type name) translate fetch calls into IPC calls. These fetchers share a
reference-counted remote in an `IpProtectionCoreHostRemote` instance.

In the reverse direction, `IpProtectionCoreImplMojo` derives from
`IpProtectionCoreImpl`and implements the `ip_protection::mojo::Core` type,
translating IPCs into appropriate local calls. This unusual subclassing design
allows `IpProtectionCoreImpl` to be built without any reference to Mojo
declarations, such as for Cronet.

### Telemetery

Cronet and other Chrome platforms have different telemetry implementations.
These are abstracted by the `IpProtectionTelemetry` class, allowing measurement
points to be independent of the platform.

### Object Ownership

Within the network service, each `NetworkContext` owns its proxy delegate and
core.

### Profiles

Blind-signed tokens are fetched using authentication associated with the
signed-in Chrome profile. Core host profile keyed service profile selection
includes `ProfileSelection::kRedirectedToOriginal`, and it uses authentication
from the original profile's signed-in user.

PRTs are fetched for incognito profiles at the start of the incognito session.

### IP Protection Status

This component provides a mechanism to determine if IP Protection is actively
being used to proxy network requests within a given `WebContents` (i.e., a tab).
This is primarily used to inform the User Bypass UI (specifically, the
`CookieControlsController`) so that the appropriate indicator can be displayed
to the user.

**Key Classes:**

*   **`IpProtectionStatus`:** A `WebContentsObserver` and `WebContentsUserData`
    that tracks whether any subresource on the current primary page has been
    proxied by IP Protection.
    *   `CreateForWebContents(content::WebContents* web_contents)`: Creates an
        instance of `IpProtectionStatus` for the given `WebContents`. Does
        nothing if IP Protection is disabled via feature flag or if an instance
        already exists.
    *   `IsSubresourceProxiedOnCurrentPrimaryPage() const`: Returns `true` if IP
        Protection has proxied at least one subresource on the current primary
        page.
    *   `PrimaryPageChanged(content::Page& page)`: Resets the internal flag.
    *   `ResourceLoadComplete(...)`: Checks if the loaded resource used the IP
        Protection proxy chain.
*   **`IpProtectionStatusObserver`:** An interface that other components can
    implement to be notified when IP Protection becomes active on a page.
    *   `OnSubresourceProxied()`: Called when `IpProtectionStatus` detects that
        a subresource has been proxied on the current primary page.

**How it Works:**

1.  **Creation:** An instance of `IpProtectionStatus` is created and attached to
    a `WebContents` object during tab initialization. This ensures that an
    `IpProtectionStatus` instance is associated with each tab, *if* the IP
    Protection feature is enabled.
2.  **Observation:** The `IpProtectionStatus` instance observes the
    `WebContents`.
3.  **Resource Load:** When a resource finishes loading, `ResourceLoadComplete`
    is triggered.
4.  **Proxy Check:** The `proxy_chain` from the `ResourceLoadInfo` is examined.
    If `proxy_chain.is_for_ip_protection()` and `!proxy_chain.is_direct()` are
    both true, it means the resource was loaded through the IP Protection proxy.
5.  **Status Update:** The `is_subresource_proxied_on_current_primary_page_`
    flag is set to `true`.
6.  **Observer Notification:** `OnSubresourceProxied()` is called on all
    registered `IpProtectionStatusObserver`s.
7.  **Page Change:** When the user navigates to a new page, `PrimaryPageChanged`
    is triggered, and `is_subresource_proxied_on_current_primary_page_` is reset
    to `false`.

**Integration with User Bypass:**

The `CookieControlsController` (which manages the User Bypass UI) uses
`IpProtectionStatus` to determine when to show the IP Protection indicator. It
does this by:

1.  Having its `TabObserver` inner class implement `IpProtectionStatusObserver`.
2.  Registering the `TabObserver` with the `IpProtectionStatus` instance.
3.  In the `OnSubresourceProxied()` callback, calling
    `CookieControlsController::UpdateUserBypass()`.
4.  Within `ShouldUserBypassIconBeVisible()`, checking
    `GetIsSubresourceProxied()`, which in turn calls
    `IpProtectionStatus::IsSubresourceProxiedOnCurrentPrimaryPage()`.

## TODO

Note that `MaskedDomainListManager` does not yet use a fetcher.
