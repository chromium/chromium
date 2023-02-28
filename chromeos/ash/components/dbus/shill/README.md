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