# Overview
The Shill D-Bus client is a critical component of the Chrome OS network stack
that facilitates communication between applications and system components with
Shill via D-Bus. This client provides a high-level API that enables interaction
with the following Shill services:
* [Shill manager service](#shill-manager-client)
* Shill device service
* Shill IPconfig service
* Shill profile service
* Shill service service

## Shill clients initialization and shutdown
ChromeOS interacts with Shill services through their corresponding Shill D-Bus
clients. To initialize all Shill D-Bus clients, the
[`shill_clients::Initialize()`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ash/dbus/ash_dbus_helper.cc;l=136;drc=1e745d6190686a85eea668b86350080be45b55f9)
function is used. This function is called from the
[`ash::InitializeDBus()`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/app/chrome_main_delegate.cc;l=650;drc=705fc04a0d21cfe6709b8a56750704a19290ce96)
function during the Ash/Chrome UI
[early initialization](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ash/dbus/ash_dbus_helper.cc;l=123;drc=33ab5f99dea200ad10ab79d898465e82f8a4ae77)
stage as there are many other components that depend on Shill clients.
The `shill_clients::Initialize()` function sets up a single global instance of
each Shill D-Bus client which are accessible by public static functions provided
by each client individually, e.g.: `ShillManagerClient::Get()`.\
Additionally, it also provides a
[`shill_clients::InitializeFakes()`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/dbus/shill/shill_clients.cc;l=34;drc=e4714ce987b39d3207473e0cd5cc77fbbbf37fda)
function which initializes the global singleton instance with fake implementation.
This function is mostly used for unit tests or ChromeOS-Linux build.\
Then, during Ash/Chrome UI shutdown, the
[`ash::ShutdownDBus()`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ash/dbus/ash_dbus_helper.cc;l=256;drc=1e745d6190686a85eea668b86350080be45b55f9)
function is called, and the
[`shill_clients::Shutdown()`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/dbus/shill/shill_clients.cc;l=47;drc=e4714ce987b39d3207473e0cd5cc77fbbbf37fda)
function is used to bring down all the Shill D-Bus clients.


## Shill manager client
The [ShillManagerClient](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/dbus/shill/shill_manager_client.h;drc=f10ad519eaa48f765938cc453b97f3333f1d1a9d)
class provides an interface for interacting with the Shill manager service
interface which is the top-level singleton exposed by Shill that provides
global system properties. It enables Chrome to:
* Get or set shill manager properties
* Enable or disable technologies such as Wi-Fi/cellular networks
* Scan and connect to the best service, like searching for nearby Wi-Fi networks
* Configure networks, such as setting up a Wi-Fi network with a password
* Add or remove passpoint credentials
* Tethering related functionality such as enable or disable hotspot tethering

For detailed documentation on the Shill Manager DBus API, please refer to
[manager-api.txt](https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform2/shill/doc/manager-api.txt;drc=89f1629aac064713d70908436fa9834c4f443551).

All Shill methods must be called from the origin thread that initializes the
DBusThreadManager instance. Most methods that make Shill Manager calls pass
two callbacks:
* callback: invoked if the method call succeeds
* error_callback: invoked if the method call fails or returns an error response

Additionally, ShillManagerClient provides a
[test interface](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/dbus/shill/shill_manager_client.h;l=51;drc=f10ad519eaa48f765938cc453b97f3333f1d1a9d)
that allows you to set up fake devices, services, and technologies for ChromeOS
unit testing purposes. This interface is implemented in the
[FakeShillManagerClient](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/dbus/shill/fake_shill_manager_client.h;drc=f10ad519eaa48f765938cc453b97f3333f1d1a9d)
class, which simulates the behavior of ShillManagerClient and provides APIs for
setting fake responses. For example, `SetInteractiveDelay` sets the fake
interactive delay for the following shill response for testing purposes, while
`SetSimulateConfigurationResult` sets the following `ConfigureService` call to
succeed, fail or timeout.