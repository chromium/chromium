// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_DEVICE_CACHE_H_
#define CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_DEVICE_CACHE_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom.h"

namespace ash::bluetooth_config {

class AdapterStateController;

// Caches known Bluetooth devices, providing getters and an observer interface
// for receiving updates when devices change.
class DeviceCache {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    // Invoked when the list of paired devices has changed. This callback is
    // used when a device has been added/removed from the list, or when one or
    // more properties of a device in the list has changed.
    virtual void OnPairedDevicesListChanged() {}

    // Invoked when the list of unpaired devices has changed. This callback is
    // used when a device has been added/removed from the list, or when one or
    // more properties of a device in the list has changed.
    virtual void OnUnpairedDevicesListChanged() {}
  };

  virtual ~DeviceCache();

  // Returns a sorted list of all paired devices. The list is sorted such that
  // connected devices appear before connecting devices, which appear before
  // disconnected devices. If Bluetooth is disabled, disabling, or unavailable,
  // this function returns an empty list.
  std::vector<mojom::PairedBluetoothDevicePropertiesPtr> GetPairedDevices()
      const;

  // Returns a sorted list of unpaired devices. This list is sorted by signal
  // strength.
  std::vector<mojom::BluetoothDevicePropertiesPtr> GetUnpairedDevices() const;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  explicit DeviceCache(AdapterStateController* adapter_state_controller);

  // Implementation-specific version of GetPairedDevices(), which is invoked
  // only if Bluetooth is enabled or enabling.
  virtual std::vector<mojom::PairedBluetoothDevicePropertiesPtr>
  PerformGetPairedDevices() const = 0;

  // Implementation-specific version of GetUnpairedDevices(), which is invoked
  // only if Bluetooth is enabled or enabling.
  virtual std::vector<mojom::BluetoothDevicePropertiesPtr>
  PerformGetUnpairedDevices() const = 0;

  AdapterStateController* adapter_state_controller() {
    return adapter_state_controller_;
  }

  void NotifyPairedDevicesListChanged();

  void NotifyUnpairedDevicesListChanged();

 private:
  bool IsBluetoothEnabledOrEnabling() const;

  raw_ptr<AdapterStateController> adapter_state_controller_;

  base::ObserverList<Observer> observers_;
};

}  // namespace ash::bluetooth_config

#endif  // CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_DEVICE_CACHE_H_
