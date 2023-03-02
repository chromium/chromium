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

TODO: Discuss network\_handler.h / the network stack.

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

### `Device State`s

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
