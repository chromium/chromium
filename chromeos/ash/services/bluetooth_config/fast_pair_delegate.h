// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_FAST_PAIR_DELEGATE_H_
#define CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_FAST_PAIR_DELEGATE_H_

#include <optional>

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom.h"

namespace ash::bluetooth_config {

class AdapterStateController;
class DeviceImageInfo;
class DeviceNameManager;

// Delegate class used to connect the bluetooth_config and Fast Pair systems,
// which live in different parts of the dependency tree and cannot directly
// call each other.
class FastPairDelegate {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    // Invoked when the list of fast pairable devices has changed. This callback
    // is used when a device has been added/removed from the list, or when one
    // or more properties of a device in the list has changed.
    virtual void OnFastPairableDevicesChanged(
        const std::vector<mojom::PairedBluetoothDevicePropertiesPtr>&
            fast_pairable_devices) = 0;
  };

  virtual std::optional<DeviceImageInfo> GetDeviceImageInfo(
      const std::string& mac_address) = 0;
  virtual void ForgetDevice(const std::string& mac_address) = 0;
  virtual void UpdateDeviceNickname(const std::string& mac_address,
                                    const std::string& nickname) = 0;
  virtual void SetAdapterStateController(
      AdapterStateController* adapter_state_controller) = 0;
  virtual void SetDeviceNameManager(DeviceNameManager* device_name_manager) = 0;
  virtual std::vector<mojom::PairedBluetoothDevicePropertiesPtr>
  GetFastPairableDeviceProperties() = 0;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  virtual ~FastPairDelegate();
  FastPairDelegate();

  // For inherited classes to call, notifying observers tracked by base class.
  void NotifyFastPairableDevicesChanged(
      const std::vector<mojom::PairedBluetoothDevicePropertiesPtr>&
          fast_pairable_devices);

 private:
  base::ObserverList<Observer> observers_;
};

}  // namespace ash::bluetooth_config

#endif  // CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_FAST_PAIR_DELEGATE_H_
