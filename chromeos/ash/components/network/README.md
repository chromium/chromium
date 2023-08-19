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

#### Tests within `//ash/system/network`

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

### Network State

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

Networks on ChromeOS are created and configured using three primary classes.
Each of the classes operates at a different level of abstraction and which class
is used will be determined by the goals of the user.

#### `ManagedNetworkConfigurationHandler`

The
[`ManagedNetworkConfigurationHandler`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/network/managed_network_configuration_handler.h;drc=5e476d249f1b36460280115db38fdc37b1c37128)
class is used to create and configure networks on ChromeOS. This class is
responsible for taking care of network policies and specifically only accepts
ONC with a stated goal of decoupling users from direct interaction with Shill.
Most of the APIs provided by this class accept both a callback to invoke on
success and a callback to invoke on failure. The failure callback will be
provided with an error message that is suitable for logging.

This class is typically used to:

* Configure and remove networks, including policy-defined networks
* Set the properties of a network
* Get the properties of a network *with policies applied*
* Set or update the policies actively applied
* Get the policies actively applied
  * Many helper methods are provided for checking specific policy features

When creating networks or retrieving the properties of a network, the caller is
required to provide a user hash that is used to make sure that both the device
policies (applied to all users) and the user policies (specific for a user) are
considered. The result of this is that when creating or configuring networks,
some properties may be assigned specific values; this can mean that properties
receive default values, or it can mean that properties that the caller specific
are overridden and restricted from being any different value. When retrieving
the properties of a network, many of the properties received will have
additional information beyond the value. This information includes:

* The policy source and enforcement level, if any, e.g. provided by a device
  policy and the value provided is recommended.
* The value provided by policy. When a policy is recommended this will be the
  default value for the property, and when a policy is required the property
  will be forced to have this value.

While the `ManagedNetworkConfigurationHandler` class does provide an API to set
the active policy this API should not be called outside of tests. The [`NetworkConfigurationUpdated`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/policy/networking/network_configuration_updater.h;drc=148f8a073813914fe0de89b7785d5750b3bb5520) class
is the class responsible for tracking device and user policies and applying them
to the device. When there are policy changes this class will use
[`ManagedNetworkConfigurationHandler::SetPolicy()`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/network/managed_network_configuration_handler.h;l=145-154;drc=5e476d249f1b36460280115db38fdc37b1c37128)
which will result in all existing user profiles, and all networks that they
have, having the new policy applied to them. While applying the policy to
existing user profiles and networks is not an instantaneous operation, any
networks created or configured after `SetPolicy()` is called will have the
policy enforced.

The `ManagedNetworkConfigurationHandler` class also provides an observer
interface,
[`NetworkPolicyObserver`](https://source.chromium.org/chromium/chromium/src/+/refs/heads/main:chromeos/ash/components/network/network_policy_observer.h;drc=614b0b49c9e734fa7f6632df48542891b9f7f92a),
that can be used to be notified when policies have changed, or have been
applied.

Unlike most of the network classes this class provides a mock to help simplify
testing:
[`MockManagedNetworkConfigurationHandler`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/network/mock_managed_network_configuration_handler.h;drc=5e476d249f1b36460280115db38fdc37b1c37128).
If it is not possible to use this mock the `ManagedNetworkConfigurationHandler`
class can still be accessed via `NetworkHandler`. For more information please
see the [Testing](#testing) section.

#### `NetworkConfigurationHandler`

The
[`NetworkConfigurationHandler`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/network/network_configuration_handler.h;drc=5e476d249f1b36460280115db38fdc37b1c37128)
class is used to create and configure networks on ChromeOS. This class
interacts with the platforms layer by making calls to the
[`Shill APIs`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/dbus/shill/;drc=5e476d249f1b36460280115db38fdc37b1c37128)
to perform a variety of tasks related to shill
[`services and profiles`](https://chromium.googlesource.com/chromium/src/+/HEAD/chromeos/ash/components/network/README.md#shill)
which include methods to:

* Get, set, and clear
[`shill properties`](https://source.chromium.org/chromium/chromium/src/+/refs/heads/main:third_party/cros_system_api/dbus/shill/dbus-constants.h;drc=b8143cf1dfd24842890fcd831c4f5d909bef4fc4)
tied to a shill service. Note that when setting properties, existing properties
are not cleared, and removals must be done explicitly.
* Create a shill service and associate it to a shill profile
* Remove or change the association between a shill service and shill profile(s)
  
Further, the `NetworkConfigurationHandler` class also provides an observer
interface,
[` NetworkConfigurationObserver`](https://source.chromium.org/chromium/chromium/src/+/refs/heads/main:chromeos/ash/components/network/network_configuration_observer.h;drc=b8143cf1dfd24842890fcd831c4f5d909bef4fc4;)
, that can be used to be notified when the properties of shill service(s)
change.

In unit tests, the `NetworkConfigurationHandler` can be initialized for testing
purposes.

#### `ShillServiceClient`

The
[`ShillServiceClient`](https://source.chromium.org/chromium/chromium/src/+/refs/heads/main:chromeos/ash/components/dbus/shill/shill_service_client.h;drc=af33e6b506bcb54e29efd850e2eb546f476ee63a)
class provides APIs for interacting with the Platform equivalent of networks:
Shill services. This class is typically chosen when the user wants to directly
interact with Shill, or is already directly interacting with Shill for other
functionality and it is easier to continue to do so than transition to one of
the higher-level classes discussed above. For more information on this class
please see the associated
[README.md](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/dbus/shill/README.md;l=96-118;drc=1585cdd820a4a1db5b6e91ad2827f8db31fe33fc).

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

## Connecting to a Network

### `NetworkConnect`

The [`NetworkConnect`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/network/network_connect.h;drc=14ccdeb9606a78fadd516d7c1d9dbc7ca28ad019)
class is used to handle the complex UI flows associated with connecting to a
network on ChromeOS. This class does not show any UI itself, but
instead delegates that responsibility to [`NetworkConnect::Delegate`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/network/network_connect.h;l=30;drc=14ccdeb9606a78fadd516d7c1d9dbc7ca28ad019)
implementations.

Further, this class is also not responsible for making Shill connect calls and
delegates this responsibility to [`NetworkConnectionHandler`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/network/network_connection_handler.h;drc=14ccdeb9606a78fadd516d7c1d9dbc7ca28ad019).

The `NetworkConnect` class provides APIs for:

* Connect or disconnect to a network by passing in a network ID
* Enable or disable a particular technology type (e.g., WiFi)
* Configure a network and connect to that network

### `NetworkConnectionHandler`

`NetworkConnectionHandler` is responsible for managing network connection
requests. It is the only class that should make Shill connect calls. It
provides APIs to:
* Connect to a network
* Disconnect from a network

It also defines a set of results that can be returned by the connection attempt.

[`NetworkConnectionHandlerImpl`](https://osscs.corp.google.com/chromium/chromium/src/+/main:chromeos/ash/components/network/network_connection_handler_impl.h;drc=d8468bb60e224d8797b843ee9d0258862bcbe87f)
is the class that implements `NetworkConnectionHandler`. When there is a
connection request, it follows these steps:
1. Determines whether or not sufficient information (e.g. passphrase) is known
to be available to connect to the network
2. Requests additional information (e.g. user data which contains certificate
information) and determines whether enough information is available. If
certificates have not been loaded yet then the connection request is queued.
3. Possibly configures the network certificate info
4. Sends the connect request
5. Waits for the network state to change to a non-connecting state
6. Invokes the appropriate callback (always) on success or failure

If the network is of type `Tether`, `NetworkConnectionHandler` delegates
actions to the [`TetherDelegate`](https://osscs.corp.google.com/chromium/chromium/src/+/main:chromeos/ash/components/network/network_connection_handler.h;l=138-159;drc=d8468bb60e224d8797b843ee9d0258862bcbe87f).

If the network is of type `Cellular`, `CellularConnectionHandler` is used to
prepare the network before having Shill initiate the connection.

Classes can observe the following network connection events by implementing
[`NetworkConnectionObserver`](https://osscs.corp.google.com/chromium/chromium/src/+/refs/heads/main:chromeos/ash/components/network/network_connection_observer.h;l=15;drc=afec9eaf1d11cc77e8e06f06cb026fadf0dbf758):
* When a connection to a specific network is requested
* When a connection requests succeeds
* When a connection requests fails
* When a disconnection from a specific network is requested

These observer methods may be preferred over the observer methods in
[`NetworkStateHandlerObserver`](https://osscs.corp.google.com/chromium/chromium/src/+/main:chromeos/ash/components/network/network_state_handler_observer.h;drc=614b0b49c9e734fa7f6632df48542891b9f7f92a)
when a class wishes to receive notifications for specific connect/disconnect
operations rather than more gross network activity.

### `CellularConnectionHandler`

[`CellularConnectionHandler`](https://source.chromium.org/chromium/chromium/src/+/refs/heads/main:chromeos/ash/components/network/cellular_connection_handler.h;drc=d8468bb60e224d8797b843ee9d0258862bcbe87f)
provides the functions to prepare both pSIM and eSIM cellular
networks for connection. Is provides API to:
* Prepare connection for a newly installed cellular network
* Prepare connection for an existing cellular network

Before we can connect to a cellular network, the
network must be backed by Shill service and must have its `Connectable`
property set to true which means that it is the selected SIM profile in its
slot.

ChromeOS only supports a single physical SIM slot, so
pSIM networks should always have their `Connectable` properties set to true as
long as they are backed by Shill. Since Shill is expected to create a service
for each pSIM, the only thing that the caller needs to do is wait for the
corresponding Shill service to be configured.

For eSIM networks, it is possible that there are multiple eSIM profiles on a
single EUICC; in this case, `Connectable` being false means that the eSIM
profile is disabled and must be enabled via Hermes before a connection can
succeed. The steps for preparing an eSIM network are:

1. Check to see if the profile is already enabled; if so, skip to step #6.
2. Inhibit cellular scans
3. Request installed profiles from Hermes
4. Enable the relevant profile
5. Uninhibit cellular scans
6. Wait until the associated [`NetworkState`](#Network-State) becomes connectable
7. Wait until Shill automatically connects if the SIM slot is switched

TODO: Finish README
