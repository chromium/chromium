// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_EMULATOR_DEVICE_EMULATOR_MESSAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_EMULATOR_DEVICE_EMULATOR_MESSAGE_HANDLER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/system/pointer_device_observer.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "device/bluetooth/bluetooth_adapter.h"

namespace chromeos {
class FakePowerManagerClient;
}

namespace dbus {
class ObjectPath;
}  // namespace dbus

namespace bluez {
class FakeBluetoothDeviceClient;
}

namespace ash {

// Handler class for the Device Emulator page operations.
class DeviceEmulatorMessageHandler
    : public system::PointerDeviceObserver::Observer,
      public content::WebUIMessageHandler {
 public:
  DeviceEmulatorMessageHandler();

  DeviceEmulatorMessageHandler(const DeviceEmulatorMessageHandler&) = delete;
  DeviceEmulatorMessageHandler& operator=(const DeviceEmulatorMessageHandler&) =
      delete;

  ~DeviceEmulatorMessageHandler() override;

  // Adds |this| as an observer to all necessary objects.
  void Init(const base::Value::List& args);

  // Callback for the "removeBluetoothDevice" message. This is called by
  // the view to remove a bluetooth device from the FakeBluetoothDeviceClient's
  // observed list of devices.
  void HandleRemoveBluetoothDevice(const base::Value::List& args);

  // Callback for the "requestBluetoothDiscover" message. This asynchronously
  // requests for the system to discover a certain device. The device's data
  // should be passed into |args| as a dictionary. If the device does not
  // already exist, then it will be created and attached to the main adapter.
  void HandleRequestBluetoothDiscover(const base::Value::List& args);

  // Callback for the "requestBluetoothInfo" message. This asynchronously
  // requests for the devices which are already paired with the device.
  void HandleRequestBluetoothInfo(const base::Value::List& args);

  // Callback for the "requestBluetoothPair" message. This asynchronously
  // requests for the system to pair a certain device. The device's data should
  // be passed into |args| as a dictionary. If the device does not already
  // exist, then it will be created and attached to the main adapter.
  void HandleRequestBluetoothPair(const base::Value::List& args);

  // Callback for the "requestAudioNodes" message. This asynchronously
  // requests the audio node that is current set to active. It is possible
  // that there can be multiple current active nodes.
  void HandleRequestAudioNodes(const base::Value::List& args);

  // Create a node and add the node to the current AudioNodeList in the
  // FakeCrasAudioClient.
  void HandleInsertAudioNode(const base::Value::List& args);

  // Removes an AudioNode from the current list in the FakeCrasAudioClient
  // based on the node id.
  void HandleRemoveAudioNode(const base::Value::List& args);

  // Connects or disconnects a fake touchpad.
  void HandleSetHasTouchpad(const base::Value::List& args);

  // Connects or disconnects a fake mouse.
  void HandleSetHasMouse(const base::Value::List& args);

  // Callbacks for JS update methods. All these methods work
  // asynchronously.
  void UpdateBatteryPercent(const base::Value::List& args);
  void UpdateBatteryState(const base::Value::List& args);
  void UpdateTimeToEmpty(const base::Value::List& args);
  void UpdateTimeToFull(const base::Value::List& args);
  void UpdatePowerSources(const base::Value::List& args);
  void UpdatePowerSourceId(const base::Value::List& args);

  // content::WebUIMessageHandler:
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  // Callback for the "requestPowerInfo" message. This asynchronously requests
  // for power settings such as battery percentage, external power, etc. to
  // update the view.
  void RequestPowerInfo(const base::Value::List& args);

 private:
  class BluetoothObserver;
  class CrasAudioObserver;
  class PowerObserver;

  void BluetoothDeviceAdapterReady(
      scoped_refptr<device::BluetoothAdapter> adapter);

  // Creates a bluetooth device with the properties given in |args|. |args|
  // should contain a dictionary so that each dictionary value can be mapped
  // to its respective property upon creating the device. Returns the device
  // path.
  std::string CreateBluetoothDeviceFromListValue(const base::Value::List& args);

  // Builds a dictionary with each key representing a property of the device
  // with path |object_path|.
  base::Value::Dict GetDeviceInfo(const dbus::ObjectPath& object_path);

  void ConnectToBluetoothDevice(const std::string& address);

  // system::PointerDeviceObserver::Observer:
  void TouchpadExists(bool exists) override;
  void HapticTouchpadExists(bool exists) override;
  void MouseExists(bool exists) override;
  void PointingStickExists(bool exists) override;

  void UpdateAudioNodes();

  raw_ptr<bluez::FakeBluetoothDeviceClient> fake_bluetooth_device_client_;
  std::unique_ptr<BluetoothObserver> bluetooth_observer_;

  std::unique_ptr<CrasAudioObserver> cras_audio_observer_;

  raw_ptr<chromeos::FakePowerManagerClient> fake_power_manager_client_;
  std::unique_ptr<PowerObserver> power_observer_;

  scoped_refptr<device::BluetoothAdapter> bluetooth_adapter_;

  base::WeakPtrFactory<DeviceEmulatorMessageHandler> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_EMULATOR_DEVICE_EMULATOR_MESSAGE_HANDLER_H_
