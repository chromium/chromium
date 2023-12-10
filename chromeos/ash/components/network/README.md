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

### Stub Networks

In certain cases, cellular networks may not have an associated shill services.
For example, when a SIM is locked, mobile network is unavailable or if eSIM profiles
are unavailable through platform's modem manager. The most common cause for this is
eSIM profiles not being active though. In such cases, we create stub network instances
in place of those networks and make them available to the NetworkStateHandler.

The interface for the StubCellularNetworkProvider is defined in the [`NetworkStateHandler`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/network/network_state_handler.h;l=84;drc=d8468bb60e224d8797b843ee9d0258862bcbe87f) and
implemented by [`StubCellularNetworksProvider`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/network/stub_cellular_networks_provider.h;drc=d8468bb60e224d8797b843ee9d0258862bcbe87f)

StubCellularNetworkProvider interface provides two methods: AddOrRemoveStubCellularNetworks and GetStubNetworkMetadata

[`AddOrRemoveStubCellularNetworks`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/network/stub_cellular_networks_provider.h;l=38;drc=d8468bb60e224d8797b843ee9d0258862bcbe87f) takes in a list of managed networks, empty list of stub ids and cellular device state instance. This function then looks for cases where a corresponding network in the list of managed networks is missing for a eSIM profile or a pSIM and goes onto create a new stub instance. It also removes stub instances for a profile if a corresponding network has been added to the managed network list. A boolean is returned to indicate if any changes to stub networks have taken place and the input parameter containing list of stub ids will be filled if new stub networks have been created.

[`GetStubNetworkMetadata`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/network/stub_cellular_networks_provider.h;l=42;drc=d8468bb60e224d8797b843ee9d0258862bcbe87f) returns metadata for a stub network instance if one exists for the given iccid.

NetworkStateHandler is the primary caller of StubNetworkProvider. It attempts to make a change in stub networks if there is a change to the managed network list, change in the property of a network or a change in cellular technology state. For any of these changes, a call is made to the AddOrRemoveStubCellularNetworks function in stub network provider.

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
the active policy this API should not be called outside of tests. The [`NetworkConfigurationUpdater`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/policy/networking/network_configuration_updater.h;drc=148f8a073813914fe0de89b7785d5750b3bb5520) class
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
6. Wait until the associated [`NetworkState`](#network-state) becomes connectable
7. Wait until Shill automatically connects if the SIM slot is switched

## Apply Networking Policies

Chrome uses [ONC](https://chromium.googlesource.com/chromium/src/+/main/components/onc/docs/onc_spec.md)
to represent and apply network policy. These ONC includes configurations which
can be used to configure a new network (i.e.: WiFi or eSIM) or update an
existing network, and also global policies which will affect all networks in a
certain way on ChromeOS devices, and whether a network was configured via ONC
will be reflected in the
[`OncSource`](https://osscs.corp.google.com/chromium/chromium/src/+/main:third_party/cros_system_api/dbus/shill/dbus-constants.h;l=175;drc=c13d041e8414a890e2f24863a121c639d33237c2)
property in the corresponding Shill service configuration.

The [`ManagedNetworkConfigurationHandler`](#managednetworkconfigurationhandler)
class is the entry point for policy application. This class provides a
[`SetPolicy()`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/network/managed_network_configuration_handler.h;l=145-154;drc=5e476d249f1b36460280115db38fdc37b1c37128)
API that manages the complexity around both queuing and performing policy
applications. This class internally delegates much of the policy application
logic to [`PolicyApplicator`](#policyapplicator).

### `PolicyApplicator`

[`PolicyApplicator`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/network/policy_applicator.h;drc=d8468bb60e224d8797b843ee9d0258862bcbe87f)
is responsible for network policy application. The policy application process is
started via the
[`Run()`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/network/policy_applicator.cc;l=123;drc=acd0e1034f101c2ef8bafa49186bcb84e550dc27;bpv=1;bpt=1)
API. This API fetches all existing entries from the provided Shill profile in
parallel through
[`GetProfilePropertiesCallback`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/network/policy_applicator.cc;l=133;drc=eee0ccfe31638a5a0a0b62eab20120021b945071)
and compares each profile entry with the policies currently being applied in
[`GetEntryCallback`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/network/policy_applicator.cc;l=175;drc=eee0ccfe31638a5a0a0b62eab20120021b945071).
The applicator tries to find the matching network configuration by first
comparing the GUID. If no Shill configuration could be found with a matching
GUID, this API will then try to match using additional network properties,
e.g. the ICCID of a cellular network, using the
[`FindMatchingPolicy`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/network/policy_applicator.cc;l=53;drc=eee0ccfe31638a5a0a0b62eab20120021b945071)
to ascertain if the policy matches. The following are the main cases handled in
the `GetEntryCallback`:
* If no existing profile entries match with the policy being applied,
[`ApplyRemainingPolicies`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/network/policy_applicator.cc;drc=acd0e1034f101c2ef8bafa49186bcb84e550dc27;l=426)
is invoked to apply missing policies. For cellular, it delegates the
application of the new policies in
[`CellularPolicyHandler`](#cellularpolicyhandler).
* If the policy being applied matches an existing profile entry, the applicator
proceeds to enforce the new policy through
[`ApplyNewPolicy`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/network/policy_applicator.cc;l=291;drc=eee0ccfe31638a5a0a0b62eab20120021b945071).
* If there's an existing profile entry indicating the service is managed but no
matching policy is discovered, it will delete the entry from the profile.
* Finally, it will apply the global policy on all unmanaged profile entries.

### `CellularPolicyHandler`

[`CellularPolicyHandler`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/network/cellular_policy_handler.h;drc=e8286e2f4c1e24abdc6a0633073b4973f240a450)
encapsulates the logic for installing eSIM profiles configured by policy.
Installation requests are added to a queue, and each request will be retried a
fixed number of times with a retry delay between each attempt. When installing
policy eSIM profiles, the activation code is constructed from either SM-DP+
address or SM-DS address in the policy configuration.

### `PolicyCertificateProvider`

[`PolicyCertificateProvider`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/network/policy_certificate_provider.h;l=25;drc=5b6979c43621893d550e829e4e68ef13980a2415)
is an interface which makes server and authority certificates available from
enterprise policy. Clients of this interface can register as observers to
receive update when:
* The list of policy-set server and authority certificates changes.
* The PolicyCertificateProvider is being destroyed.

### Notes on Cellular networks

Unlike Wi-Fi networks (i.e Wi-Fi Shill services), which can be per-Chromebook
(e.g networks added during OOBE/Login) or per-user as they are generally
configured. Cellular networks (i.e Cellular Shill services) are per-Chromebook
and there is no way to configure them as per-user.

In other words, a particular Wi-Fi network may have different GUIDs when logged
into different accounts on a device if it is configured per-user. However, a
Cellular network will always have the same GUID when logged into different
accounts on a device, as it is always configured per-Chromebook in Shill.

#### Examples and Technical Details

##### Auto-Connect

The state of whether auto-connect is enabled or disabled is preserved across any
logged in account. In other words, if user A logs in to the device and enables
auto-connect, then the auto-connect Cellular property remains enabled if user B
were to subsequently log into the device. This is not the case for user-configured
Wi-Fi, where auto-connect for a specific network X could be disabled for user A and
enabled for user B, or vice versa.

* In Shill, the auto-connect property is stored in [`kAutoConnectProperty`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/cros_system_api/dbus/Shill/dbus-constants.h;l=135;drc=6f4c64436342c818aa41e6a5c55034e74ec9c6b6)
  which is a base service property that is shared across different types of Shill
  services (i.e Wi-Fi and Cellular ones alike)

##### Roaming

The state of whether roaming is enabled or disabled for a cellular network is
preserved across any logged in account. In other words, if user A logs in to
the device and enables roaming, then the roaming property remains enabled if
user B were to subsequently log into the device. A similar analog to use for
contrast may be a user-configured Wi-Fi's "Configure IP address automatically"
property, which can be true for user A but false for user B on a device.

* In Shill, the allow roaming property is stored in [`kCellularAllowRoamingProperty`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/cros_system_api/dbus/Shill/dbus-constants.h;l=185;drc=6f4c64436342c818aa41e6a5c55034e74ec9c6b6)
  which is a cellular Shill service property
* `kCellularAllowRoamingProperty` is used to populate a cellular [`NetworkState's |allow_roaming()|`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/network/network_state.h;l=170;drc=d83e99de89c0ccc6fee4ced1e450739b142d4b2c)
  property.

##### Text Messages

SMSes received will be shown regardless of which account is logged on the
device. Note that users and admins have the ability to configure cellular
networks at the Chrome layer such that text messages for a cellular network
of a particular GUID are suppressed. Note that the associated cellular Shill
service(s) will not know about this Chrome owned configuration (i.e no
Shill property associated to suppression of text messages).

* The [`SMSObserver`](https://source.chromium.org/chromium/chromium/src/+/main:ash/system/network/sms_observer.h;drc=6f4c64436342c818aa41e6a5c55034e74ec9c6b6)
  observes for SMSes received by the modem for the active cellular Shill
  service via [`NetworkSmsDeviceHandler`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/network/network_sms_handler.cc;drc=6f4c64436342c818aa41e6a5c55034e74ec9c6b6?q=NetworkSmsDeviceHandler)
* The SMSObserver is [`instantiated during the System UI initialization`](https://source.chromium.org/chromium/chromium/src/+/main:ash/shell.cc;l=1641;drc=6f4c64436342c818aa41e6a5c55034e74ec9c6b6;bpv=1;bpt=1)
  and will show the notification regardless of whose logged in.

##### SIM Lock

If a SIM is PIN or PUK locked, it is locked for any user logged into the
device. Whether the SIM is locked and how many unlock retry attempts left,
among other SIM lock related cellular properties, is stored in cellular Shill
Device properties, which a cellular Shill Service is associated with.

* In Shill, SIM lock information is stored in [`kSIMLockStatusProperty`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/cros_system_api/dbus/Shill/dbus-constants.h;l=533-538;drc=d83e99de89c0ccc6fee4ced1e450739b142d4b2c;bpv=0;bpt=1)
  which is a Cellular device property
* `kSIMLockStatusProperty` is used to populate cellular-specific [`DeviceState properties`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/network/device_state.cc;l=100-127;drc=d83e99de89c0ccc6fee4ced1e450739b142d4b2c;bpv=1;bpt=1)

## Persisting eSIM Profiles

eSIM profiles are displayed in some cellular UI surfaces. However, they can
only be retrieved from the device hardware when an EUICC is the "active" slot
on the device, and only one slot can be active at a time. This means that if
the physical SIM slot is active, we cannot fetch an updated list of profiles
without switching slots, which can be disruptive if the user is utilizing a
cellular connection from the physical SIM slot.

To ensure that clients can access eSIM metadata regardless of the active slot,
[`CellularESimProfileHandler`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/network/cellular_esim_profile_handler.h;l=31;drc=28eec300d12693725de66c979962d9b8a4209a7d)
stores all known eSIM profiles persistently in prefs. It does this when the
available EUICC list changes, when an EUICC property changes, a carrier profile
property changes or when an profile is installed.

Clients can access these eSIM profiles through the
[`GetESimProfiles()`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/network/cellular_esim_profile_handler.h;l=87-90;drc=28eec300d12693725de66c979962d9b8a4209a7d)
and can observe changes to the list by observing
[`CellularESimProfileHandler`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/network/cellular_esim_profile_handler.h;l=40-41;drc=28eec300d12693725de66c979962d9b8a4209a7d).
Additionally, `CellularESimProfileHandlerImpl` tracks all known EUICC
paths. If it detects a new EUICC which it previously had not known about, it
automatically refreshes profile metadata from that slot. This ensures that
after a powerwash, since all local data will be erased and we will no longer
have information on which slots we have metadata for, we will refresh the
metadata for all slots. This is done in [`AutoRefreshEuiccsIfNecessary()`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/network/cellular_esim_profile_handler_impl.h;l=69;drc=d8468bb60e224d8797b843ee9d0258862bcbe87f).


## Hotspot(Tethering)

The [`HotspotStateHandler`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/network/hotspot_state_handler.h;drc=7134a1c6cac8c7bd23d8214bd5479f6f2d837d76)
class is responsible for caching the latest hotspot state and notifying its
observers whenever there is a change in the hotspot state.

The [`HotspotCapabilitiesProvider`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/network/hotspot_capabilities_provider.h;drc=a6cec85709049281bc688f04bff6deb3f1691571)
class calculates and caches the latest hotspot capabilities. This calculation
is triggered whenever the cellular network state changes or when Shill signals
changes in the "TetheringCapabilities" property. The calculation involves the
following operations:
1. Checks if the policy allows hotspot; it exits early if the policy prohibits
it.
2. Checks if the hotspot is supported by the platform, considering factors such
as cellular support for upstream technology and Wi-Fi for downstream technology.
3. Checks if the active cellular network state is online; it exits early if it
is not.
4. Calls `CheckTetheringReadiness` from Shill to verify if it passes the
readiness check.

The [`HotspotController`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/network/hotspot_controller.h;l=6;drc=28050c2b4975c08c93c35630e64800db12b8676c)
class manages set the admin policy of the hotspot, enable and disable hotspot.
When enabling the hotspot, it performs the following operations:
1. Checks the hotspot capabilities from `HotspotCapabilitiesProvider`. If not
allowed, it exits early.
2. Calls `CheckTetheringReadiness` from Shill and exits early if the check is not
passed.
3. Disables Wi-Fi if it is active.
4. Enables or disables the hotspot using the Shill service.

The `HotspotController` also observes changes in hotspot state and restores
Wi-Fi to its previous status when the hotspot is turned off.

The [`HotspotConfigurationHandler`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/network/hotspot_configuration_handler.h;drc=1e7275664ba566e4e3521e520f45f1c9aef6768a)
class is responsible for caching the
latest hotspot configuration and handling the update of the hotspot
configuration.

TODO: Finish README
