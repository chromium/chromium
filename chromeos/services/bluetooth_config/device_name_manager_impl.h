// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_BLUETOOTH_CONFIG_DEVICE_NAME_MANAGER_IMPL_H_
#define CHROMEOS_SERVICES_BLUETOOTH_CONFIG_DEVICE_NAME_MANAGER_IMPL_H_

#include "chromeos/services/bluetooth_config/device_name_manager.h"

#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"

class PrefRegistrySimple;

namespace chromeos {
namespace bluetooth_config {

// Concrete DeviceNameManager implementation that saves entries into Prefs.
class DeviceNameManagerImpl : public DeviceNameManager {
 public:
  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  explicit DeviceNameManagerImpl(
      scoped_refptr<device::BluetoothAdapter> bluetooth_adapter);
  ~DeviceNameManagerImpl() override;

  // DeviceNameManager:
  absl::optional<std::string> GetDeviceNickname(
      const std::string& device_id) override;
  void SetDeviceNickname(const std::string& device_id,
                         const std::string& nickname) override;
  void RemoveDeviceNickname(const std::string& device_id) override;
  void SetPrefs(PrefService* local_state) override;

 private:
  // Returns true if a BluetoothDevice* with identifier |device_id| exists in
  // |bluetooth_adapter_|, else false.
  bool DoesDeviceExist(const std::string& device_id) const;

  scoped_refptr<device::BluetoothAdapter> bluetooth_adapter_;
  PrefService* local_state_ = nullptr;
};

}  // namespace bluetooth_config
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_BLUETOOTH_CONFIG_DEVICE_NAME_MANAGER_IMPL_H_
