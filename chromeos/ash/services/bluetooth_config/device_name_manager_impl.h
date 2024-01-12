// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_DEVICE_NAME_MANAGER_IMPL_H_
#define CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_DEVICE_NAME_MANAGER_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "chromeos/ash/services/bluetooth_config/device_name_manager.h"

#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"

class PrefRegistrySimple;

namespace ash::bluetooth_config {

// Concrete DeviceNameManager implementation that saves entries into Prefs.
class DeviceNameManagerImpl : public DeviceNameManager {
 public:
  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  explicit DeviceNameManagerImpl(
      scoped_refptr<device::BluetoothAdapter> bluetooth_adapter);
  ~DeviceNameManagerImpl() override;

  // DeviceNameManager:
  std::optional<std::string> GetDeviceNickname(
      const std::string& device_id) override;
  void SetDeviceNickname(const std::string& device_id,
                         const std::string& nickname) override;
  void RemoveDeviceNickname(const std::string& device_id) override;
  void SetPrefs(PrefService* local_state) override;

 private:
  // Migrates the IDs used to persist nicknames in |local_state_| from the BlueZ
  // format to the Floss format.
  void MigrateExistingNicknames();

  // Returns true if a BluetoothDevice* with identifier |device_id| exists in
  // |bluetooth_adapter_|, else false.
  bool DoesDeviceExist(const std::string& device_id) const;

  scoped_refptr<device::BluetoothAdapter> bluetooth_adapter_;
  raw_ptr<PrefService> local_state_ = nullptr;
};

}  // namespace ash::bluetooth_config

#endif  // CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_DEVICE_NAME_MANAGER_IMPL_H_
