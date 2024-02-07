# CrosBluetoothConfig

[`CrosBluetoothConfig`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom;drc=321047b607bc69f5d6dce6e47319d0c198d0616e)
is a high-level Bluetooth API implemented using [Mojo](https://source.chromium.org/chromium/chromium/src/+/main:mojo/README.md;drc=d5888c9f83d076bfba4e3bb5d749b182d35610ec)
and is the primary Bluetooth API used by clients within Chrome.

This document describes various details of the API such as what it does, how
the API can be used, what the primary clients of the API are, and what its
dependencies are.

## ChromeOS Bluetooth Design

Within Chrome, Bluetooth functionality is exposed via
[//device/bluetooth](https://source.chromium.org/chromium/chromium/src/+/main:device/bluetooth/;drc=fad13429ad5c09a01d5ee5a58f4575f2affc4abd)
(see [README](https://source.chromium.org/chromium/chromium/src/+/main:device/bluetooth/README.md;drc=cf64a67e4ef4684287ca64db4948cb2e6f25b492)).
The entrypoint to this library is [BluetoothAdapterFactory](https://source.chromium.org/chromium/chromium/src/+/main:device/bluetooth/bluetooth_adapter_factory.h;drc=bceeb1f38c2dad765b51d012fdf6c32ca36108ae),
which exposes an API for interacting with the [BluetoothAdapter](https://source.chromium.org/chromium/chromium/src/+/main:device/bluetooth/bluetooth_adapter.h;drc=84d29de4c159e89404aceafbfecaeecba6443e15).
On ChromeOS, `BluetoothAdapter` utilizes [`FlossDBusManager`](https://source.chromium.org/chromium/chromium/src/+/main:device/bluetooth/floss/floss_dbus_manager.h;drc=3ed9cf1119782c06147dac3bf548d5a1f7ee9336)
or [`BluezDBusManager`](https://source.chromium.org/chromium/chromium/src/+/main:device/bluetooth/dbus/bluez_dbus_manager.h;drc=bb9cd02c17a2e8a8bdeca3712d357ca3d15a95bf),
depending on whether Floss or BlueZ is being used, to communicate with the
Bluetooth stack library via D-Bus. //device/bluetooth is not specific to
ChromeOS, and a lot of it is shared with other platforms and products (e.g.,
Chrome Browser).

`BluetoothAdapter` exposes APIs for powering Bluetooth on and off, discovering,
pairing and connecting to devices, and sending/receiving information from
these devices.

Because Chromium supports other operating systems, the `BluetoothAdapter` class
and the interfaces it exposes are intentionally generic and use different
implementations depending on the platform. Thus, in order to implement
Bluetooth UI in ChromeOS, there is additional logic wrapping.

There are four top-level UI surfaces used for ChromeOS Bluetooth:

* Quick Settings: Native UI written in C++
* Settings: WebUI written in Polymer
* Pairing Dialog: WebUI written in Polymer
* OOBE UI: WebUI written in Polymer, implemented using WebUI message handlers

![CrosBluetoothConfig](https://screenshot.googleplex.com/8iiMoNmUFXFGSfw.png)

The diagram above shows a simplified view of these relationships, in which
these all use `CrosBluetoothConfig`.

The `CrosBluetoothConfig` API does not contain all Bluetooth functionality in
ChromeOS. Specifically, some ChromeOS features (e.g., Nearby Share, Phone Hub,
Instant Tethering, Smart Lock) utilize lower-level Bluetooth APIs which are not
directly exposed by the UI. However, most Chrome clients should default to
using `CrosBluetoothConfig` unless specific, lower-level functionality is
required.

### Bluetooth Stack

Currently in ChromeOS, there are two Bluetooth stacks: BlueZ and Floss. BlueZ
is the legacy stack that is being replaced by Floss (as of Q4 2023). For more
information see [here](https://sites.google.com/corp/google.com/flossproject/home).

## API

`CrosBluetoothConfig` functionality resides in
[//chromeos/ash/services/bluetooth_config/](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/services/bluetooth_config/;drc=a906730dd23299a558eb3783dafcd6f24186bbe8).
Mojo interfaces are defined within a
[public/mojom](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/services/bluetooth_config/public/mojom/;drc=321047b607bc69f5d6dce6e47319d0c198d0616e)
subdirectory. The primary interface `CrosBluetoothConfig` has a concrete
implementation within the top-level directory.

`CrosBluetoothConfig`'s [dependencies](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/services/bluetooth_config/cros_bluetooth_config.h;l=41-43;drc=d8468bb60e224d8797b843ee9d0258862bcbe87f)
are various lower-level Bluetooth classes such as `BluetoothAdapter` and
`BluetoothDevice`, `UserManager`, system prefs (both device-wide
"local" prefs, and per-profile prefs belonging to the primary login) and
`FastPairDelegate`.

### Bluetooth State

The following APIs can be used by clients for observing and modifying the
Bluetooth state:
```
// Notifies observer with initial set of Bluetooth properties when observer
// is first added, then again whenever properties are updated.
ObserveSystemProperties(pending_remote<SystemPropertiesObserver> observer);

// Informs observer when a device is newly paired, connected or
// disconnected.
ObserveDeviceStatusChanges(
  pending_remote<BluetoothDeviceStatusObserver> observer);

// Turns Bluetooth on or off.
SetBluetoothEnabledState(bool enabled);
```
Clients that wish to fetch the Bluetooth adapter's power state or the paired
device list should use
[`ObserveSystemProperties()`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom;l=316;drc=321047b607bc69f5d6dce6e47319d0c198d0616e).
To update the adapter's power state clients should use
[`SetBluetoothEnabledState()`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom;l=331;drc=321047b607bc69f5d6dce6e47319d0c198d0616e).

### Bluetooth Device Properties

Unpaired Bluetooth devices are represented as `BluetoothDeviceProperties`, and
paired Bluetooth devices are represented as `PairedBluetoothDeviceProperties`
which is composed of `BluetoothDeviceProperties` and additional paired-specific
properties. Device information such as address, type, and battery level can be
accessed through these properties.
```
// Properties belonging to a Bluetooth device.
struct BluetoothDeviceProperties {
  // Unique identifier for this device, which is stable across device reboots.
  string id;

  // The Bluetooth address of this device.
  string address;

  // Publicly-visible name provided by the device. If no name is provided by
  // the device, the device address is used as a name.
  mojo_base.mojom.String16 public_name;

  // Device type, derived from the ClassOfDevice attribute for the device.
  DeviceType device_type;

  ...
};

// Properties belonging to a Bluetooth device which has been paired to this
// Chrome OS device.
struct PairedBluetoothDeviceProperties {
  BluetoothDeviceProperties device_properties;

  // Nickname for this device as provided by the user. Local to the device
  // (i.e., other devices do not have access to this name). Null if the
  // device has not been nicknamed by the user.
  string? nickname;

  ...
};
```

### Pairing

To pair with a device, clients should first start a discovery session to scan
for Bluetooth devices:
```
// Starts a discovery session, during which time it will be possible
// to find new devices and pair them.
StartDiscovery(pending_remote<BluetoothDiscoveryDelegate> delegate);
```
Once discovery has started, clients should use the
[`DevicePairingHandler`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom;l=260;drc=321047b607bc69f5d6dce6e47319d0c198d0616e)
to initiate pairing with a device:
```
interface BluetoothDiscoveryDelegate {
  // Invoked when discovery has started.
  // |handler| can be used to initiate pairing to a discovered device.
  OnBluetoothDiscoveryStarted(
      pending_remote<DevicePairingHandler> handler);
};

// Handles requests to pair to a device.
interface DevicePairingHandler {
  // Attempts to pair to the device with ID |device_id|. Pairing often
  // requires additional interaction from the user, so callers must
  // provide a |delegate| which handles requests for these interactions.
  // For example, pairing a Bluetooth keyboard usually requires that
  // users type in a PIN.
  //
  // |result| is returned when the pairing attempt completes. It is
  // possible that |result| is returned before any delegate function
  // is invoked.
  PairDevice(
      string device_id,
      pending_remote<DevicePairingDelegate> delegate) =>
          (PairingResult result);
};
```
Pairing may require additional authentication. Clients who call pair device
should provide a `DevicePairingDelegate` which implements the handling of
different authentication scenarios that could possibly be required:
```
// Provided by the pairing UI to handle pairing requests of
// different types.
interface DevicePairingDelegate {
  // Requests that a PIN be provided to complete pairing.
  RequestPinCode() => (string pin_code);

  // Requests that a passkey be provided to complete pairing.
  RequestPasskey() => (string passkey);

  // Requests that |pin_code| be displayed to the user, who should
  // enter the PIN via a Bluetooth keyboard.
  DisplayPinCode(string pin_code,
                 pending_receiver<KeyEnteredHandler> handler);

  // Requests that |passkey| be displayed to the user, who should
  // enter the passkey via a Bluetooth keyboard.
  DisplayPasskey(string passkey,
                 pending_receiver<KeyEnteredHandler> handler);

  // Requests that |passkey| be displayed to the user, who should
  // confirm or reject a pairing request. Returns whether or not the
  // user confirmed the passkey.
  ConfirmPasskey(string passkey) => (bool confirmed);

  // Requests that the user is asked to confirm or reject a pairing
  // request. Returns whether or not the user confirmed the pairing.
  AuthorizePairing() => (bool confirmed);
};
```

### Connecting/Disconnecting/Forgetting

`CrosBluetoothConfig` exposes APIs for operations on paired Bluetooth devices,
such as connecting or forgetting, with the following APIs:

```
// Initiates a connection to the device with ID |device_id|.
Connect(string device_id) => (bool success);

// Initiates a disconnection from the device with ID |device_id|.
Disconnect(string device_id) => (bool success);

// Forgets the device with ID |device_id|, which in practice means
// un-pairing from the device.
Forget(string device_id) => (bool success);
```

### Testing

Clients that use `CrosBluetoothConfig` can utilize a fake implementation of the
API by initializing `CrosBluetoothConfig` with
[`ScopedBluetoothConfigTestHelper`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/services/bluetooth_config/scoped_bluetooth_config_test_helper.h;drc=e8286e2f4c1e24abdc6a0633073b4973f240a450)
rather than
[`InitializerImpl`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/services/bluetooth_config/initializer_impl.h;drc=e4714ce987b39d3207473e0cd5cc77fbbbf37fda).

## Implementation

`CrosBluetoothConfig` is composed of many internal classes which implement
specific functionality of the API. Some notable ones are:

* [`AdapterStateController`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/services/bluetooth_config/adapter_state_controller.h;drc=e4714ce987b39d3207473e0cd5cc77fbbbf37fda):
Handles retrieving and modifying the Bluetooth adapter powered state
* [`DeviceCache`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/services/bluetooth_config/device_cache.h;drc=d8468bb60e224d8797b843ee9d0258862bcbe87f):
Maintains the list of paired and unpaired Bluetooth devices
* [`DevicePairingHandler`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/services/bluetooth_config/device_pairing_handler.h;drc=d8468bb60e224d8797b843ee9d0258862bcbe87f):
Executes pairing attempts and manages pairing authentication if required
* [`DeviceOperationHandler`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/services/bluetooth_config/device_operation_handler.h;drc=d8468bb60e224d8797b843ee9d0258862bcbe87f):
Executes operations on paired devices
* [`DiscoverySessionManager`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/services/bluetooth_config/discovery_session_manager.h;drc=d8468bb60e224d8797b843ee9d0258862bcbe87f):
Starts and stops Bluetooth device discovery sessions

Each of these classes provides a fake implementation that can be used standalone
or with `CrosBluetoothConfig`. New functionality should either
be implemented in one of the internal classes, or if an entirely new class must
be created, a fake implementation should also be created for it so it can be
used for unit testing.