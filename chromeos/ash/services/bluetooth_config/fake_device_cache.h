// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_FAKE_DEVICE_CACHE_H_
#define CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_FAKE_DEVICE_CACHE_H_

#include "chromeos/ash/services/bluetooth_config/device_cache.h"

namespace ash::bluetooth_config {

class FakeDeviceCache : public DeviceCache {
 public:
  explicit FakeDeviceCache(AdapterStateController* adapter_state_controller);
  ~FakeDeviceCache() override;

  void SetPairedDevices(
      std::vector<mojom::PairedBluetoothDevicePropertiesPtr> paired_devices);

  void SetUnpairedDevices(
      std::vector<mojom::BluetoothDevicePropertiesPtr> unpaired_devices);

 private:
  // DeviceCache:
  std::vector<mojom::PairedBluetoothDevicePropertiesPtr>
  PerformGetPairedDevices() const override;

  std::vector<mojom::BluetoothDevicePropertiesPtr> PerformGetUnpairedDevices()
      const override;

  std::vector<mojom::PairedBluetoothDevicePropertiesPtr> paired_devices_;

  std::vector<mojom::BluetoothDevicePropertiesPtr> unpaired_devices_;
};

}  // namespace ash::bluetooth_config

#endif  // CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_FAKE_DEVICE_CACHE_H_
