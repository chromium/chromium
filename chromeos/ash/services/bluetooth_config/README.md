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
implementation within the top-level directory, which routes public API calls to
the internal classes it's composed of.

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

* [`SystemPropertiesProvider`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/services/bluetooth_config/system_properties_provider.h;drc=321047b607bc69f5d6dce6e47319d0c198d0616e):
Provides system Bluetooth properties, including Bluetooth availability and
on/off state
* [`AdapterStateController`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/services/bluetooth_config/adapter_state_controller.h;drc=e4714ce987b39d3207473e0cd5cc77fbbbf37fda):
Handles retrieving and modifying the Bluetooth adapter powered state
* [`DeviceCache`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/services/bluetooth_config/device_cache.h;drc=d8468bb60e224d8797b843ee9d0258862bcbe87f):
Maintains the list of paired and unpaired Bluetooth devices
* [`DiscoverySessionManager`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/services/bluetooth_config/discovery_session_manager.h;drc=d8468bb60e224d8797b843ee9d0258862bcbe87f):
Starts and stops Bluetooth device discovery sessions
* [`DevicePairingHandler`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/services/bluetooth_config/device_pairing_handler.h;drc=d8468bb60e224d8797b843ee9d0258862bcbe87f):
Executes pairing attempts and manages pairing authentication if required
* [`DeviceOperationHandler`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/services/bluetooth_config/device_operation_handler.h;drc=d8468bb60e224d8797b843ee9d0258862bcbe87f):
Executes operations on paired devices

Each of these classes provides a fake implementation that can be used standalone
or with `CrosBluetoothConfig`. New functionality should either
be implemented in one of the internal classes, or if an entirely new class must
be created, a fake implementation should also be created for it so it can be
used for unit testing.

`CrosBluetoothConfig` has a [concrete implementation](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/services/bluetooth_config/cros_bluetooth_config.h;drc=6b2b6f5aa258a1616fab24634c4e9477cfef5daf)
which routes public API calls to the internal classes it's composed of.

### [`SystemPropertiesProvider`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/services/bluetooth_config/system_properties_provider.h;drc=321047b607bc69f5d6dce6e47319d0c198d0616e)
This class computes the current [`BluetoothSystemProperties`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom;l=159-171;drc=5e4575c404a5571e5997be05352ab16fdf912361).
These system properties provide information on the Bluetooth adapter state,
whether Bluetooth can be modified, and the list of paired devices. It provides
an observer method which clients can be used to be notified of system changes.
```
// Adds an observer of system properties. |observer| will be notified of
// the current properties immediately as a result of this function, then again
// each time system properties change. To stop observing, clients should
// disconnect the Mojo pipe to |observer| by deleting the associated Receiver.
void Observe(mojo::PendingRemote<mojom::SystemPropertiesObserver> observer);
```

### [`AdapterStateController`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/services/bluetooth_config/adapter_state_controller.h;drc=e4714ce987b39d3207473e0cd5cc77fbbbf37fda)
This class controls the state of the Bluetooth adapter and serves as the source
of truth for the adapter's current state. This class modifies the Bluetooth
adapter directly and should only be used by classes that do not wish to persist
the adapter state to prefs. For classes that do wish to persist the adapter
state to prefs, such as those processing incoming user requests,
[`BluetoothPowerController`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/services/bluetooth_config/bluetooth_power_controller.h;drc=e4714ce987b39d3207473e0cd5cc77fbbbf37fda)
should be used instead.

Clients can call `SetBluetoothEnabledState()` to update the adapter state, and
can implement the `Observer` to be notified of adapter state changes:
```
class Observer : public base::CheckedObserver {
  // Invoked when the state has changed; use GetAdapterState() to retrieve the
  // updated state.
  virtual void OnAdapterStateChanged() = 0;
};

// Returns the system state as obtained from the Bluetooth adapter.
virtual mojom::BluetoothSystemState GetAdapterState() const = 0;

// Turns Bluetooth on or off. If Bluetooth is unavailable or already in the
// desired state, this function is a no-op.
// This does not save to |enabled| to prefs. If |enabled| is wished to be
// saved to prefs, BluetoothPowerController::SetBluetoothEnabledState() should
// be used instead.
virtual void SetBluetoothEnabledState(bool enabled) = 0;
```

The implementation of this class is [`AdapterStateControllerImpl`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/services/bluetooth_config/adapter_state_controller_impl.h;drc=659445b23950e6a6358ea8a1035ea893cbee7398).
This class queues adapter state change requests to ensure only one
`BluetoothAdapter::SetPowered()` call is invoked at a time. It maintains the
operation being executed and one queued operation, and if another operation
comes in, the queued operation is overwritten.

### [`BluetoothPowerController`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/services/bluetooth_config/bluetooth_power_controller.h;drc=e4714ce987b39d3207473e0cd5cc77fbbbf37fda)
This class sets the Bluetooth power state and saves the state to prefs. It also
initializes the Bluetooth power state during system startup and user session
startup.

Classes that wish to set the Bluetooth adapter state and save that value to
prefs should use this class. Classes that do not want to persist the state to
prefs should use `AdapterStateController` instead. Internally, this class
serves as a wrapper around `AdapterStateController`.
```
// Changes the Bluetooth power setting to |enabled|, persisting |enabled| to
// user prefs if a user is logged in. If no user is logged in, the pref is
// persisted to local state.
virtual void SetBluetoothEnabledState(bool enabled) = 0;

// Enables Bluetooth but doesn't persist the state to prefs. This should be
// called to enable Bluetooth when OOBE HID detection starts.
virtual void SetBluetoothHidDetectionActive() = 0;

// If |is_using_bluetooth| is false, restores the Bluetooth enabled state that
// was last persisted to local state. This should be called when OOBE HID
// detection ends.
virtual void SetBluetoothHidDetectionInactive(bool is_using_bluetooth) = 0;
```

### [`DeviceCache`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/services/bluetooth_config/device_cache.h;drc=d8468bb60e224d8797b843ee9d0258862bcbe87f)
This class caches known Bluetooth devices, providing getters and an observer
interface for receiving updates when devices change. Classes can use this to
get the list of paired and unpaired devices and be notified when items in these
lists change.
```
class Observer : public base::CheckedObserver {
  // Invoked when the list of paired devices has changed. This callback is
  // used when a device has been added/removed from the list, or when one or
  // more properties of a device in the list has changed.
  virtual void OnPairedDevicesListChanged() {}

  // Invoked when the list of unpaired devices has changed. This callback is
  // used when a device has been added/removed from the list, or when one or
  // more properties of a device in the list has changed.
  virtual void OnUnpairedDevicesListChanged() {}
};

// Returns a sorted list of all paired devices. The list is sorted such that
// connected devices appear before connecting devices, which appear before
// disconnected devices. If Bluetooth is disabled, disabling, or unavailable,
// this function returns an empty list.
std::vector<mojom::PairedBluetoothDevicePropertiesPtr> GetPairedDevices() const;

// Returns a sorted list of unpaired devices. This list is sorted by signal
// strength.
std::vector<mojom::BluetoothDevicePropertiesPtr> GetUnpairedDevices() const;
```
This class is implemented at [`DeviceCacheImpl`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/services/bluetooth_config/device_cache_impl.h;drc=6b2b6f5aa258a1616fab24634c4e9477cfef5daf).
Using `BluetoothAdapter` observers methods, it maintains in-memory lists of
paired and unpaired devices.

### [`DiscoverySessionManager`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/services/bluetooth_config/discovery_session_manager.h;drc=d8468bb60e224d8797b843ee9d0258862bcbe87f)
When unpaired devices wish to be found, a `BluetoothAdapter` discovery session
must be started. This class handles requests to start discovery sessions.
Clients invoke `StartDiscovery()` to begin the flow and disconnect the delegate
passed to `StartDiscovery()` to end the flow.
```
// Starts a discovery attempt. |delegate| is notified when the discovery
// session has started and stopped. To cancel a discovery attempt, disconnect
// |delegate|.
void StartDiscovery(
    mojo::PendingRemote<mojom::BluetoothDiscoveryDelegate> delegate);
```

When a discovery session is started, observers of `DeviceCache` are immediately
informed of the current discovered Bluetooth devices and will continue to
receive updates whenever a device is added, updated or removed. Once the
session has ended, clients will no longer receive updates.

This class is implemented at [`DiscoverySessionManagerImpl`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/services/bluetooth_config/discovery_session_manager_impl.h;drc=6b2b6f5aa258a1616fab24634c4e9477cfef5daf).
Internally, this class ensures that Bluetooth discovery remains active as long
as at least one discovery client is active.

### [`DiscoveredDevicesProvider`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/services/bluetooth_config/discovered_devices_provider.h;drc=e4714ce987b39d3207473e0cd5cc77fbbbf37fda)
Clients that start a discovery session via `DiscoverySessionManager` can
observe `DeviceCache` in order to be notified of when unpaired devices are
discovered. However, for UI surfaces, updating each time the list changes can
provide an undesired UX behavior where the list is changing too rapidly.
`DiscoveredDevicesProvider` is a wrapper for `DeviceCache` that batches
unpaired devices list updates. If the device list has changed, this
implementation waits [`kNotificationDelay`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/services/bluetooth_config/discovered_devices_provider_impl.h;l=33-35;drc=6b2b6f5aa258a1616fab24634c4e9477cfef5daf)
before sorting and notifying clients that the list has changed. This is to
reduce the frequency of changes to the device list in UI surfaces, giving users
more time to view the list between updates.
```
class Observer : public base::CheckedObserver {
  // Invoked when the list of discovered devices has changed. This callback is
  // used when a device has been added/removed from the list, or when one or
  // more properties of a device in the list has changed.
  virtual void OnDiscoveredDevicesListChanged() = 0;
};
```

When the list of unpaired devices changes, this method implements the following
logic in effort to limit the frequency of device position changes clients
observe:
* If a device has been added, it's appended to the end of the list and clients
are notified. If no timer is currently running, after |kNotificationDelay|, the
list is sorted (by signal strength), and clients are notified again.
* If a device has been updated, it's properties are updated but its position
un-updated, and clients are notified. If no timer is currently running, after
|kNotificationDelay|, the list is sorted, and clients are notified again.
* If a device has been removed, it's removed from the list and clients are
notified. If no timer is currently running, after |kNotificationDelay|, the
list is sorted, and clients are notified again. This last sorting and
notification are unnecessary but simplify this method.

### [`DevicePairingHandler`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/services/bluetooth_config/device_pairing_handler.h;drc=d8468bb60e224d8797b843ee9d0258862bcbe87f)
This class handles requests to pair to a Bluetooth device. This handler can be
reused to pair to more than one device. Only one device should be attempted to
be paired to at a time. Callees must pass in a `DevicePairingDelegate` to
handle potential authentication being required. If the delegate is
disconnected, any in-progress pairing is canceled.
```
void PairDevice(const std::string& device_id,
                  mojo::PendingRemote<mojom::DevicePairingDelegate> delegate,
                  PairDeviceCallback callback) override;
```
This class is implemented at [`DevicePairingHandlerImpl`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/services/bluetooth_config/device_pairing_handler_impl.h;drc=6b2b6f5aa258a1616fab24634c4e9477cfef5daf),
which interacts with the `BluetoothDevice`, serving as the device's
`PairingDelegate`, and relays the PairingDelegate method calls back to the
client that initiated the pairing request via the request's
`DevicePairingDelegate`.

### [`DeviceOperationHandler`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/services/bluetooth_config/device_operation_handler.h;drc=d8468bb60e224d8797b843ee9d0258862bcbe87f)
This class provides operations that can be performed on paired devices, such
as connecting or disconnecting to a device. Operations are performed
sequentially, queueing requests that occur simultaneously.
```
// Initiates a connection to the device with ID |device_id|.
void Connect(const std::string& device_id, OperationCallback callback);

// Initiates a disconnection from the device with ID |device_id|.
void Disconnect(const std::string& device_id, OperationCallback callback);

// Forgets the device with ID |device_id|, which in practice means
// un-pairing from the device.
void Forget(const std::string& device_id, OperationCallback callback);
```
Operations have a [timeout](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/services/bluetooth_config/device_operation_handler.h;l=91-92;drc=6b2b6f5aa258a1616fab24634c4e9477cfef5daf),
which when exceeded, returns to the client a failure result and the next
operation is processed. This class is implemented at
[`DeviceOperationHandlerImpl`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/services/bluetooth_config/device_operation_handler_impl.h;drc=6b2b6f5aa258a1616fab24634c4e9477cfef5daf)
which interfaces with the `BluetoothDevice` APIs.

### [`BluetoothDeviceStatusNotifier`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/services/bluetooth_config/bluetooth_device_status_notifier.h;drc=e4714ce987b39d3207473e0cd5cc77fbbbf37fda)
This class manages notifying listeners of changes in individual Bluetooth
devices status. Status changes includes a newly paired device, new connection
and new disconnection (which includes forgetting a connected device).
```
// Adds an observer of Bluetooth device status. |observer| will be notified
// each time Bluetooth device status changes. To stop observing, clients
// should disconnect the Mojo pipe to |observer| by deleting the associated
// Receiver.
void ObserveDeviceStatusChanges(
    mojo::PendingRemote<mojom::BluetoothDeviceStatusObserver> observer);
```
This class is implemented at [`BluetoothDevicesStatusNotifierImpl`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/services/bluetooth_config/bluetooth_device_status_notifier_impl.h;drc=6b2b6f5aa258a1616fab24634c4e9477cfef5daf),
which observes `DeviceCache` under the hood to compute changes to device
statuses.

### [`DeviceNameManager`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/services/bluetooth_config/device_name_manager.h;drc=a323c83e92a59f87c4c12cc20c4c2aabf8bc414a)
This class manages saving and retrieving nicknames for Bluetooth devices. This
nickname is local to only the Chromebook and is visible to all users of the
Chromebook.
```
class Observer : public base::CheckedObserver {
  // Invoked when the nickname of device with id |device_id| has changed to
  // |nickname|. If |nickname| is null, the nickname has been removed for
  // |device_id|.
  virtual void OnDeviceNicknameChanged(
      const std::string& device_id,
      const std::optional<std::string>& nickname) = 0;
};

// Retrieves the nickname of the Bluetooth device with ID |device_id| or
// abs::nullopt if not found.
virtual std::optional<std::string> GetDeviceNickname(
      const std::string& device_id) = 0;

// Sets the nickname of the Bluetooth device with ID |device_id| for all users
// of the current device, if |nickname| is valid.
virtual void SetDeviceNickname(const std::string& device_id,
                                 const std::string& nickname) = 0;

// Removes the nickname of the Bluetooth device with ID |device_id| for all
// users of the current device.
virtual void RemoveDeviceNickname(const std::string& device_id) = 0;
```
This class is implemented at [`DeviceNameManagerImpl`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/services/bluetooth_config/device_name_manager_impl.h;drc=6b2b6f5aa258a1616fab24634c4e9477cfef5daf),
which saves entries to Prefs.