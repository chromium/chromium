# Hermes' D-Bus Client Library

Hermes' D-Bus client library allows Chrome to make API calls to
[Hermes](https://osscs.corp.google.com/chromium/chromium/src/+/main:chromeos/ash/components/network/README.md;l=64;drc=4a50d13fc73268ef4a27cf67dc1eff40ea6f997a),
ChromeOS' eSIM configuration manager.

This document describes the various D-Bus clients that utilize Hermes' D-Bus
interfaces, the [**Manager Client**](#Hermes-Manager-Client),
[**EUICC Client**](#Hermes-EUICC-Client), and
[**Profile Client**](#Hermes-Profile-Client).

### Initialization and Shutdown

The D-Bus clients are initialized in the order of Profile client, EUICC client,
then Manager client when Ash D-Bus clients are initialized after
[Chrome's early initialization](https://osscs.corp.google.com/chromium/chromium/src/+/refs/heads/main:chrome/app/chrome_main_delegate.cc;l=755;drc=90b428dcab63b652cc91107b81d2758270e92ac0).

For extensions, the clients are initialized after the Shell Browser's
[main message loop is created](https://osscs.corp.google.com/chromium/chromium/src/+/refs/heads/main:extensions/shell/browser/shell_browser_main_parts.cc;l=129;drc=90b428dcab63b652cc91107b81d2758270e92ac0).

The clients are shutdown with the rest of the Ash D-Bus clients after Chrome
[destroys its threads](https://osscs.corp.google.com/chromium/chromium/src/+/refs/heads/main:chrome/browser/ash/chrome_browser_main_parts_ash.cc;l=1706;drc=90b428dcab63b652cc91107b81d2758270e92ac0).

### Constants

Success and error codes, along with helpful utility functions related to these
codes can be found in [hermes_response_status.h](https://osscs.corp.google.com/chromium/chromium/src/+/main:chromeos/ash/components/dbus/hermes/hermes_response_status.h;l=14;drc=07384a913575f611a42f063e7e273ac499af9ef1).

There are also [several constants](https://osscs.corp.google.com/chromium/chromium/src/+/main:chromeos/ash/components/dbus/hermes/constants.h;l=10;drc=789bec586d89e87ccb30ba132a12da2dd99b42e3)
which are shared between the D-Bus clients to implement a consistent timeout
for D-Bus calls.

### Hermes Manager Client

The [`HermesManagerClient`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/dbus/hermes/hermes_manager_client.h;drc=789bec586d89e87ccb30ba132a12da2dd99b42e3) class provides an interface for interacting
with available EUICCs on a device. This client can be used to:
* Fetch information about the available EUICC(s) on a device.
* Listen for additions or removals of EUICC(s).

For detailed documentation on the Hermes Manager DBus API, please refer to
[org.chromium.Hermes.Manager.xml](https://source.corp.google.com/h/chromium/chromiumos/codesearch/+/main:src/platform2/hermes/dbus_bindings/org.chromium.Hermes.Manager.xml;drc=938e77682349e4678ecc532c57fc1178a4c47978).

The [`HermesManagerClient::TestInterface`](https://source.chromium.org/chromium/chromium/src/+/dba55cb554820d613eb366d5051d4d9f84989cb2:chromeos/ash/components/dbus/hermes/hermes_manager_client.h;l=26)
allows you to add fake EUICC entries for ChromeOS unit testing purposes.
This interface is implemented in the [`FakeHermesManagerClient`](https://source.chromium.org/chromium/chromium/src/+/8d017dacfea36c8e7db2735a629226bab4f688bd:chromeos/ash/components/dbus/hermes/fake_hermes_manager_client.h).

### Hermes EUICC Client
The EUICC is an embedded UICC which seeks to rectify the physical SIM
shortcomings by allowing both for the storage of multiple profiles and
for the remote provisioning of profiles. The
[`HermesEuiccClient`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/dbus/hermes/hermes_euicc_client.h;drc=d0397558df545b433b085f4e894b7a61a77258c8)
class provides an interface for interacting with the Hermes EUICC objects.
This client can be used to:
* Read EUICC properties
* Install/uninstall eSIM profiles on the EUICC
* Force refresh the list of installed eSIM profiles on the EUICC
* Perform SM-DS scans for available eSIM profiles
* Reset the EUICC

For detailed documentation on the Hermes EUICC DBus API, please refer to
[org.chromium.Hermes.Euicc.xml](https://source.corp.google.com/h/chromium/chromiumos/codesearch/+/main:src/platform2/hermes/dbus_bindings/org.chromium.Hermes.Euicc.xml;drc=938e77682349e4678ecc532c57fc1178a4c47978).

The
[`HermesEuiccClient::TestInterface`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/dbus/hermes/hermes_euicc_client.h;l=38;drc=d0397558df545b433b085f4e894b7a61a77258c8)
also provides a number of different APIs for interacting with fake Hermes
EUICC service for testing purposes.
* Add/remove fake carrier profile on the EUICC
* Queues a Hermes error code that will be returned from a subsequent function call
* Sets the return for the next call to
`HermesEuiccClient::InstallProfileFromActivationCode()`
* Miscellaneous behavior such as generate fake activation code, etc

The `HermesEuiccClient::TestInterface` interface is implemented by
[`FakeHermesEuiccClient`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/dbus/hermes/fake_hermes_euicc_client.h;l=20;drc=d0397558df545b433b085f4e894b7a61a77258c8).

### Hermes Profile Client
The [`HermesProfileClient`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/dbus/hermes/hermes_profile_client.h;drc=e4714ce987b39d3207473e0cd5cc77fbbbf37fda)
class provides an interface for interacting with the to Hermes profile objects.
This client can be used to:
* Rename profile name
* Enable and disable a carrier profile
* Get properties for a profile entry
* Listen for profile property changes

For detailed documentation on the Hermes EUICC DBus API, please refer to
[org.chromium.Hermes.Profile.xml](https://source.corp.google.com/h/chromium/chromiumos/codesearch/+/main:src/platform2/hermes/dbus_bindings/org.chromium.Hermes.Profile.xml;drc=938e77682349e4678ecc532c57fc1178a4c47978).

The [`HermesProfileClient::TestInterface`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/dbus/hermes/hermes_profile_client.h;l=27-40;drc=e4714ce987b39d3207473e0cd5cc77fbbbf37fda)
allows you to add fake profile entries for ChromeOS unit testing purposes.
This interface is implemented in the [`FakeHermesProfileClient`](https://source.chromium.org/chromium/chromium/src/+/refs/heads/main:chromeos/ash/components/dbus/hermes/fake_hermes_profile_client.h;l=19;drc=e4714ce987b39d3207473e0cd5cc77fbbbf37fda).
