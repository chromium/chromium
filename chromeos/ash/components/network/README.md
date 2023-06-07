# ChromeOS Network - Chrome Layer

ChromeOS networking consists of several key components, shown in the diagram
below:

![ChromeOS Connectivity Layers](https://screenshot.googleplex.com/8c7M59NKf8DwBn4.png)

This document describes the Chrome layer (light blue rectangle above). This
layer is implemented within `//chromeos/ash/components/network`. To describe
this layer, we highlight three primary processes:

*   ***Chrome.*** Contains all system UI (e.g., settings) and processes inputs
    from the user as well as enterprise policies. Chrome sits atop the
    dependency tree and makes calls to the other components via D-Bus APIs.
*   ***Shill.*** Daemon process responsible for making network connections.
    Shill is the source of truth for which connection mediums are available and
    connected, as well as for properties of available networks.
*   ***Hermes.*** Daemon process responsible for configuring eSIM profiles.
    Hermes allows users to initiate SM-DS scans as well as communicate with
    SM-DP+ servers to install and uninstall profiles.

Shill and Hermes interface with several other components (e.g., ModemManager,
wpa_supplicant), but these are beyond the scope of this README since these
interactions are encapsulated from Chrome.

## Background

Before diving into the Chrome layer's details, we provide some background
information about the D-Bus APIs exposed by Shill and Hermes.

### Shill

Source: [platform2/shill](https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform2/shill/)

Shill is responsible for setting up network interfaces, connecting to networks
via these interfaces, persisting network metadata to disk, and providing support
for VPNs.

Shill exposes 5 key interfaces used by Chrome:

*   [`flimflam.Manager`](https://source.corp.google.com/chromeos_public/src/platform2/shill/dbus_bindings/org.chromium.flimflam.Manager.dbus-xml):
    Allows Chrome to enable/disable a technology (e.g., turning Wi-Fi on or
    off), perform a scan (e.g., look for nearby Wi-Fi networks), and configure a
    network (e.g., attempt to set up a Wi-Fi network with a password).
*   [`flimflam.Device`](https://source.corp.google.com/chromeos_public/src/platform2/shill/dbus_bindings/org.chromium.flimflam.Device.dbus-xml):
    A Shill "Device" refers to a connection medium (Wi-Fi, Cellular, and
    Ethernet are all Shill Devices). This interface allows Chrome to get or set
    properties of each connection medium as well as perform operations on each
    connection medium (e.g., unlocking the Cellular Device when it has a locked
    SIM).
*   [`flimflam.Service`](https://source.corp.google.com/chromeos_public/src/platform2/shill/dbus_bindings/org.chromium.flimflam.Service.dbus-xml):
    A Shill "Service" refers to an individual network (a Wi-Fi network or a
    cellular SIM are Shill services). This interface allows Chrome to get or set
    properties for a given network as well as initiate connections and
    disconnections.
*   [`flimflam.Profile`](https://source.corp.google.com/chromeos_public/src/platform2/shill/dbus_bindings/org.chromium.flimflam.Profile.dbus-xml):
    A Shill "Profile" refers to a grouping of services corresponding to a
    logged-in user. ChromeOS allows configuration of networks as part of the
    "default" (i.e., shared) Profile which is available to all users or as part
    of individual (i.e., per-user) Profiles.
*   [`flimflam.IPConfig`](https://source.corp.google.com/chromeos_public/src/platform2/shill/dbus_bindings/org.chromium.flimflam.IPConfig.dbus-xml):
    Allows Chrome to configure IP addresses (e.g., DNS and DHCP).

### Hermes

Source: [platform2/hermes](https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform2/hermes/)

Hermes is responsible for communicating with Embedded Universal Integrated
Circuit Cards (EUICCs) on a ChromeOS device. A EUICC can colloquially be
understood as an eSIM slot built into the device. Each EUICC has a unique
identifier called an EUICCID (or EID for short).

Hermes processes both "pending" profiles (i.e., those which have been registered
to an EID but not yet installed) as well as "installed" profiles, which have
been downloaded to a EUICC.

Hermes exposes 3 key interfaces used by Chrome:

*   [`Hermes.Manager`](https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform2/hermes/dbus_bindings/org.chromium.Hermes.Manager.xml): Allows Chrome to retrieve the list of all EUICCs and to
    observe changes to this list.
*   [`Hermes.Euicc`](https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform2/hermes/dbus_bindings/org.chromium.Hermes.Euicc.xml):
    Allows Chrome to request pending or installed profiles for a given EUICC;
    additionally, exposes functionality for installing and uninstalling
    profiles.
*   [`Hermes.Profile`](https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform2/hermes/dbus_bindings/org.chromium.Hermes.Profile.xml): Allows Chrome to enable or disable an individual profile.
    A profile must be enabled in order to be used for a connection.

## Chrome

There are many classes within the Chrome layer that are required to enable a
ChromeOS device to view and configure its connectivity functionality. These
Chrome "networking classes" are responsible for all-things networking and must
be flexible enough to support both the consumer use-case and the enterprise
use-case, e.g. enforce network policies.

### Network Stack

The network stack on the Software side of ChromeOS is initialized by the
[`NetworkHandler`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/network/network_handler.h;drc=44c626ecbc2cd37a10dae35466e404c0e7333ee5)
singleton. Beyond this initialization, `NetworkHandler` provides minimal APIs
except a comprehensive set of accessors that can be used to retrieve any of the
other more targeted network singletons e.g. `NetworkMetadataStore`.

Example of using `NetworkHandler` to retrieve one of these other singletons:
```
  if (NetworkHandler::IsInitialized()) {
    NetworkMetadataStore* network_metadata_store =
        NetworkHandler::Get()->network_metadata_store();
    ...
  }
```

#### Testing

Testings involving the ChromeOS networking stack have multiple solutions
available, and when writing tests it is important to be aware of these different
solutions for both consistency with other tests as well as ease of
implementation and maintenance.

For tests that involve code that directly calls `NetworkHandler::Get()`, the
[`NetworkHandlerTestHelper`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/network/network_handler_test_helper.h;drc=614b0b49c9e734fa7f6632df48542891b9f7f92a)
class will be be required. When instantiated, this class will handle the
initialization of Shill and Hermes DBus clients, and the `NetworkHandler`
singleton. Beyond this initialization, `NetworkHandlerTestHelper` also extends
the
[`NetworkTestHelperBase`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/network/network_test_helper_base.h;drc=d8468bb60e224d8797b843ee9d0258862bcbe87f)
class and provides many helper functions to simplify testing. There are many
examples of tests that take this approach throughout the
[`//chromeos/ash/components/network/`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/network/;drc=c338105b7b1b73ed02eba32eed2fb864477d2bf9)
directory.

For tests that do not need `NetworkHandler::Get()`, and instead only need
[`NetworkStateHandler`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/network/network_state_handler.h;drc=d8468bb60e224d8797b843ee9d0258862bcbe87f)
and/or
[`NetworkDeviceHandler`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/network/network_device_handler.h;drc=710fdab691deb78c181b67e0d74d34d623476b6e),
the
[`NetworkStateTestHelper`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/network/network_state_test_helper.h;drc=d80ebc30e3187f73e29019f2abc98b0c9bf9cdf8)
class should be used. This class is much more lightweight, and while it will
still initialize the Shill and Hermes DBus clients it will **not** initialize
the `NetworkHandler` singleton and the entire networking stack. The
`NetworkStateTestHelper` also extends `NetworkTestHelperBase` so the same helper
functions will still be available. There are many examples of tests that take
this approach throughout the `//chromeos/ash/components/network/` directory.

Tests that do not require the entire networking stack or the Shill and Hermes
DBus clients should opt to instantiate only the necessary classes and/or fakes
that they depend on. This case is much more situational and often tests will end
up looking quite different depending on the exact requirements and code
location.

##### Tests within //ash/system/network

The tests within `//ash/system/network` typically don't use the same classes
mentioned above. This difference is due to the
[`TrayNetworkStateModel`](https://source.chromium.org/chromium/chromium/src/+/main:ash/system/network/tray_network_state_model.h;drc=b8c7dcc70eebd36c4b68be590ca7b5654955002d)
which acts as a data model of all things related to networking that the system
UI, e.g. the Quick Settings and toolbar, are interested in. However, even the
`TrayNetworkStateModel` singleton does not directly use the network stack
discussed above and instead relies on the mojo
[`CrosNetworkConfig`](https://source.chromium.org/chromium/chromium/src/+/refs/heads/main:chromeos/services/network_config/public/mojom/cros_network_config.mojom;l=1147-1327;drc=28eec300d12693725de66c979962d9b8a4209a7d)
interface. While this class does directly use the network stack discussed above,
there are enough layers of abstraction that it would be far too painful to test
using the entire networking stack. Instead, these tests use
[`FakeCrosNetworkConfig`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/services/network_config/public/cpp/fake_cros_network_config.h;drc=5e476d249f1b36460280115db38fdc37b1c37128)
to directly configure the state of all things networking.

### Network States

While Shill and Hermes are the sources of truth for everything related to
networks and eSIM on ChromeOS devices it would be both inefficient and slow to
use D-Bus calls to these daemons each time the Chrome layer needed any
information about a network. Instead, all of the information about networks that
Chrome frequently accesses is cached in the Chrome layer by the
[`NetworkStateHandler`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/network/network_state_handler.h)
singleton. This class will listen for the creation, modification, and deletion
of Shill services over the D-Bus and will maintain a cache of the properties of
these services using the
[`NetworkState`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/network/network_state.h)
class.

`NetworkStateHandler` is typically used in one of four ways:

* Request changes to global (`flimflam.Manager`) properties, e.g. disable
  Cellular or start a WiFi scan.
* Listening for network, device, and global property changes by extending the
  [`NetworkStateHandlerObserver`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/network/network_state_handler_observer.h)
  class.
* Retrieve network, device, and global properties.
* Configure or retrieve the Tethering state.

Since `NetworkState` objects mirror the known properties of Shill services and
are not sources of truth themselves, they are immutable in the Chrome layer and
both their lifecycle and property updates are entirely managed by
`NetworkStateHandler`. Beyond this, `NetworkState` objects are frequently
updated and they should only be considered valid in the scope that they were
originally accessed in.

`NetworkState` objects are typically retrieved in one of two ways:

* Querying `NetworkStateHandler` for the state using a specific Shill service
  path.
* Querying `NetworkStateHandler` for all networks matching a specific criteria.

Example of retrieving and iterating over `NetworkState` objects:
```
  NetworkStateHandler::NetworkStateList state_list;
  NetworkHandler::Get()->network_state_handler()->GetNetworkListByType(
      NetworkTypePattern::WiFi(),
      /*configured_only=*/true,
      /*visible_only=*/false,
      /*limit=*/0, &state_list);

  for (const NetworkState* state : state_list) {
    ...
  }
```

### Configuring Networks

TODO: Discuss network\_configuration\_handler.h and friends.

### Device State

[`DeviceState`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/network/device_state.h;drc=ad947e92bd398452f42173e7a39ed7ab2e4ad094)
is a Chrome-layer cached representation of a Shill "Device". Similar to
`NetworkState`, Chrome caches Shill "Device" properties in a `DeviceState` to
decrease the amount of D-Bus calls needed. Shill "Devices" refer to connection
mediums (i.e. Wi-Fi, Cellular, Ethernet, etc.) and each `DeviceState` object
represent one of those mediums, providing APIs for [accessing properties](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/network/device_state.h;l=38-99;drc=ad947e92bd398452f42173e7a39ed7ab2e4ad094)
of the medium.

`DeviceState` objects are managed by [`NetworkStateHandler`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/network/network_state_handler.h;drc=14ccdeb9606a78fadd516d7c1d9dbc7ca28ad019),
which keeps its cached list up-to-date. Since `DeviceState` objects mirror the
known properties of Shill "Devices" and are not sources of truth themselves,
they can only be modified using the limited APIs they provide and both their
lifecycle and property updates are entirely managed by `NetworkStateHandler`.
There are a [few exceptions](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/network/device_state.h;l=42;drc=ad947e92bd398452f42173e7a39ed7ab2e4ad094)
where `DeviceState` objects are modified, but this should be avoided when
possible. Beyond this, `DeviceState` objects are frequently updated and they
should only be considered valid in the scope that they were originally accessed
in.

`DeviceState` objects can be retrieved in several ways:

* Querying `NetworkStateHandler` for the state using [a specific Shill device
  path](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/network/network_state_handler.h;l=171-172;drc=14ccdeb9606a78fadd516d7c1d9dbc7ca28ad019).
* Querying `NetworkStateHandler` for the state based on [its type](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/network/network_state_handler.h;l=174-175;drc=14ccdeb9606a78fadd516d7c1d9dbc7ca28ad019)
* Querying `NetworkStateHandler` for a list of [all `DeviceState` objects](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/network/network_state_handler.h;l=340-344;drc=14ccdeb9606a78fadd516d7c1d9dbc7ca28ad019)
* Querying `NetworkStateHandler` for a list of [all `DeviceState` objects with a specific type](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/network/network_state_handler.h;l=346-348;drc=14ccdeb9606a78fadd516d7c1d9dbc7ca28ad019)


Example of retrieving a single `DeviceState` object:
```
  const DeviceState* device =
    NetworkHandler::Get()->network_state_handler()->GetDeviceState(
      device_path);
```

Example of retrieving a list of `DeviceState` objects:
```
  NetworkStateHandler::DeviceStateList devices;
  NetworkHandler::Get()->network_state_handler()->GetDeviceList(&devices);
  for (const DeviceState* device : devices) {
    ...
  }
```

TODO: Finish README
