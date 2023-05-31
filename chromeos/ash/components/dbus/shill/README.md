# Overview
The Shill D-Bus clients are a critical component of the Chrome OS network stack
that facilitate communication between applications and system components with
Shill via D-Bus. These clients provide a high-level API that enables interaction
with the following Shill services:
* [Shill manager service](#shill-manager-client)
* [Shill device service](#shill-device-client)
* [Shill IPConfig service](#shill-ipconfig-client)
* [Shill profile service](#shill-profile-client)
* [Shill service service](#shill-service-client)

All of the clients used to interact with these Shill services are instantiated
as singletons, and all usage of their interfaces must happen on the same thread
that was used to instantiate the client.

For more information about Shill, the connection manager for ChromeOS, see the
[official
README.md](https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform2/shill/README.md).

# Service clients

ChromeOS interacts with Shill services through their corresponding Shill D-Bus
clients. To initialize all Shill D-Bus clients, the
[`shill_clients::Initialize()`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ash/dbus/ash_dbus_helper.cc;l=136;drc=1e745d6190686a85eea668b86350080be45b55f9)
function is used. This function is called from the
[`ash::InitializeDBus()`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/app/chrome_main_delegate.cc;l=650;drc=705fc04a0d21cfe6709b8a56750704a19290ce96)
function during the Ash/Chrome UI [early
initialization](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ash/dbus/ash_dbus_helper.cc;l=123;drc=33ab5f99dea200ad10ab79d898465e82f8a4ae77)
stage as there are many other components that depend on Shill clients. The
`shill_clients::Initialize()` function sets up a single global instance of each
Shill D-Bus client which are accessible by public static functions provided by
each client individually, e.g. [`ShillManagerClient::Get()`](https://source.chromium.org/chromium/chromium/src/+/refs/heads/main:chromeos/ash/components/dbus/shill/shill_manager_client.h;l=160;drc=2450f2f5d0ce0da9b8cf493c533f9528ff17bab6). Similarly,
the
[`shill_clients::InitializeFakes()`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/dbus/shill/shill_clients.cc;l=34;drc=e4714ce987b39d3207473e0cd5cc77fbbbf37fda)
function is also provided which initializes the global singleton instance with fake
implementations. This function is mostly used for unit tests or ChromeOS-Linux
build. Finally, during Ash/Chrome UI shutdown, the
[`ash::ShutdownDBus()`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ash/dbus/ash_dbus_helper.cc;l=256;drc=1e745d6190686a85eea668b86350080be45b55f9)
function is called, and the
[`shill_clients::Shutdown()`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/dbus/shill/shill_clients.cc;l=47;drc=e4714ce987b39d3207473e0cd5cc77fbbbf37fda)
function is used to bring down all the Shill D-Bus clients.

## Shill manager client {#shill-manager-client}
The [`ShillManagerClient`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/dbus/shill/shill_manager_client.h;drc=f10ad519eaa48f765938cc453b97f3333f1d1a9d)
class provides the interface for interacting with the Shill manager service
interface which is the top-level singleton exposed by Shill that provides
global system properties. It enables Chrome to:
* Get or set Shill manager properties
* Enable or disable technologies such as Wi-Fi/cellular networks
* Scan and connect to the best service, like searching for nearby Wi-Fi networks
* Configure networks, such as setting up a Wi-Fi network with a password
* Add or remove passpoint credentials
* Tethering related functionality such as enable or disable hotspot tethering

For detailed documentation on the Shill Manager D-Bus API, please refer to
[manager-api.txt](https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform2/shill/doc/manager-api.txt;drc=89f1629aac064713d70908436fa9834c4f443551).

## Shill device client {#shill-device-client}

The [`ShillDeviceClient`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/dbus/shill/shill_device_client.h;drc=28eec300d12693725de66c979962d9b8a4209a7d) class provides an interface for performing operations on different device types. Devices are how Shill represents network interface that is used for different technologies (Cellular, WiFi, Ethernet, etc) within the platform layer. It maintains a map of [`ShillClientHelpers`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/dbus/shill/shill_client_helper.h;drc=af33e6b506bcb54e29efd850e2eb546f476ee63a) for each device type and through those helpers, makes calls to the corresponding dbus proxy objects. It enables Chrome to:
* Get, set and clear device properties
* Listen to or ignore changes in device properties
* Perform require/change/enter/unblock PIN operations on installed SIM
* Register a cellular network
* Restart the device at a specific path
* Set MAC address source for USB Ethernet adapter (Ethernet only)

For detailed documentation on the Shill Device DBus API, please refer to [`device-api.txt`](https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform2/shill/doc/device-api.txt;drc=f0a716aa5a39deb9c18faa9b589b25ccd68009cc)

## Shill IPConfig client {#shill-ipconfig-client}

The [`ShillIPConfigClient`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/dbus/shill/shill_ipconfig_client.h;drc=ad947e92bd398452f42173e7a39ed7ab2e4ad094)
class provides an interface for interacting with the Shill IPConfig interface
which is the top-level singleton exposed by Shill that provides Layer 3
configuration. It enables Chrome to:
* Get, set, clear, or observe changes to IPConfig properties
* Remove IPConfig entries

For detailed documentation on the Shill IPConfig DBus API, please refer to
[ipconfig-api.txt](https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform2/shill/doc/ipconfig-api.txt;drc=9a98a2fb4b28a8e3c32d7eafb39395ccbc730538).

## Shill profile client {#shill-profile-client}

The [`ShillProfileClient`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/dbus/shill/shill_profile_client.h;drc=2527cfc617c6cc4bbad415d49a00b44a773e1d9f)
class provides an interface for interacting with the Shill profile interface
which is the top-level singleton exposed by Shill that provides Layer 3
configuration. It enables Chrome to:
* Set the value of a profile property
* Get properties for a profile entry
* Delete a profile

For detailed documentation on the Shill Profile DBus API, please refer to
[profile-api.txt](https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform2/shill/doc/profile-api.txt;drc=06c14aa5039b8045a2c293e65f8924c9aa5fd22b).


## Shill service client {#shill-service-client}
The
[`ShillServiceClient`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/dbus/shill/shill_service_client.h;drc=af33e6b506bcb54e29efd850e2eb546f476ee63a)
class provides the interface for interacting with the Shill service service.
Services are how Shill represents network configurations and connections through
an associated device within the Platform layer and can be thought of the source
of truth for these network configurations
and connections on ChromeOS devices. While Chrome, i.e. the Software layer,
generally relies on the read-only
[`NetworkState`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/network/README.md;l=100-149;drc=4a50d13fc73268ef4a27cf67dc1eff40ea6f997a)
class for network properties, the `NetworkState` class is effectively just a
cache of a Shill service.

The `ShillServiceClient` class provides a thorough interface for interacting
with Shill services:
* Get, set, and clear Shill service properties
* Connect, disconnect, and remove Shill services
* Listen for changes to Shill service properties
* Retrieve the passphrase associated with the Shill service
* Miscellaneous behavior e.g. complete cellular activation

For detailed documentation on the Shill Service D-Bus API, please refer to
[service-api.txt](https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform2/shill/doc/service-api.txt).

# Testing interfaces

Each of the clients discussed provides a testing interface that can be used to
effectively configure Shill to be in a specific, custom state during testing.
These testing interfaces are declared within the corresponding client class and
can be used within tests in multiple ways. For example, the
[`NetworkTestHelperBase`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/network/network_test_helper_base.h;drc=e4714ce987b39d3207473e0cd5cc77fbbbf37fda)
class can be used within tests which will initialize the entire network stack,
including the test interfaces for each of the clients.

## Shill manager client {#shill-manager-client-testing}

The
[`ShillManagerClient::TestInterface`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/dbus/shill/shill_manager_client.h;l=51;drc=f10ad519eaa48f765938cc453b97f3333f1d1a9d)
class supports a wide variety of functionality that is very useful when testing:
* Creating fakes, e.g. devices, services, technologies and more
* Configuring the delay before success/failure callbacks should be invoked
* Configuring whether certain function calls should succeed, fail, or timeout

The `ShillManagerClient::TestInterface` interface is implemented by
[`FakeShillManagerClient`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/dbus/shill/fake_shill_manager_client.h;drc=f10ad519eaa48f765938cc453b97f3333f1d1a9d).

## Shill device client {#shill-device-client-testing}

The [`ShillDeviceClient::TestInterface`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/dbus/shill/shill_device_client.h;l=40;drc=28eec300d12693725de66c979962d9b8a4209a7d) provides various functions that help during testing:
* Add, Remove and Clear fake devices
* Set device properties
* Set SIM lock status
* Adding cellular networks etc

The `ShillDeviceClient::TestInterface` interface is implemented by [`FakeShillDeviceClient`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/dbus/shill/fake_shill_device_client.h;drc=28eec300d12693725de66c979962d9b8a4209a7d)

## Shill IPConfig client {#shill-ipconfig-client-testing}

The
[`ShillIPConfigClient::TestInterface`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/dbus/shill/shill_ipconfig_client.h;l=32-40;drc=ad947e92bd398452f42173e7a39ed7ab2e4ad094)
class supports adding fake IPConfig entries during testing. This interface is
implemented by
[`FakeShillIPConfigClient`](https://source.chromium.org/chromium/chromium/src/+/refs/heads/main:chromeos/ash/components/dbus/shill/fake_shill_ipconfig_client.h;drc=ad947e92bd398452f42173e7a39ed7ab2e4ad094).

## Shill profile client {#shill-profile-client-testing}
The [`ShillProfileClient::TestInterface`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/dbus/shill/shill_profile_client.h;l=37-98;drc=2527cfc617c6cc4bbad415d49a00b44a773e1d9f)
allows you to add fake profile entries for ChromeOS unit testing
purposes. This interface is implemented in the [`FakeShillProfileClient`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/dbus/shill/fake_shill_profile_client.h;drc=2527cfc617c6cc4bbad415d49a00b44a773e1d9f).


## Shill service client {#shill-service-client-testing}

The
[`ShillServiceClient::TestInterface`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/dbus/shill/shill_service_client.h;drc=af33e6b506bcb54e29efd850e2eb546f476ee63a)
class provides a number of different APIs for interacting with services:
* Configuring fake services
* Getting, setting, and clearing service properties
* Configuring whether certain function calls should succeed, fail, or timeout
* Configuring the connect behavior of services, e.g. custom behavior on
  connection

The `ShillServiceClient::TestInterface` interface is implemented by
[`FakeShillServiceClient`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/dbus/shill/fake_shill_service_client.h;drc=af33e6b506bcb54e29efd850e2eb546f476ee63a).
