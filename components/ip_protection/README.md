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
the direct fetchers fetch the data directly from the external source.

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
core. The core owns the managers, which own the fetchers.

Within the browser, the core host owns its direct fetchers.

### Profiles

Blind-signed tokens are fetched using authentication associated with the
signed-in Chrome profile. For OTR (Incognito) profiles, uses authentication
from the "parent" profile's signed-in user.

## TODO

Note that `MaskedDomainListManager` does not yet use a fetcher.
