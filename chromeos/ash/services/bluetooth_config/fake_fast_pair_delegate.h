// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_FAKE_FAST_PAIR_DELEGATE_H_
#define CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_FAKE_FAST_PAIR_DELEGATE_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "chromeos/ash/services/bluetooth_config/fast_pair_delegate.h"
#include "chromeos/ash/services/bluetooth_config/public/cpp/device_image_info.h"

namespace ash::bluetooth_config {

class AdapterStateController;
class DeviceNameManager;

class FakeFastPairDelegate : public FastPairDelegate {
 public:
  FakeFastPairDelegate();
  FakeFastPairDelegate(const FakeFastPairDelegate&) = delete;
  FakeFastPairDelegate& operator=(const FakeFastPairDelegate&) = delete;
  ~FakeFastPairDelegate() override;

  DeviceNameManager* device_name_manager() { return device_name_manager_; }

  // Sets |images| for |mac_address| that will be returned
  // by GetDeviceImageInfo(|mac_address|).
  void SetDeviceImageInfo(const std::string& mac_address,
                          DeviceImageInfo& images);

  std::vector<std::string> forgotten_device_addresses() {
    return forgotten_device_addresses_;
  }

  std::optional<std::string> GetDeviceNickname(const std::string& mac_address) {
    const auto it = mac_address_to_nickname_.find(mac_address);
    if (it == mac_address_to_nickname_.end()) {
      return std::nullopt;
    }
    return it->second;
  }

  void SetFastPairableDeviceProperties(
      std::vector<mojom::PairedBluetoothDevicePropertiesPtr>&
          fast_pairable_device_properties);

  // FastPairDelegate:
  std::optional<DeviceImageInfo> GetDeviceImageInfo(
      const std::string& mac_address) override;
  std::vector<mojom::PairedBluetoothDevicePropertiesPtr>
  GetFastPairableDeviceProperties() override;
  void ForgetDevice(const std::string& mac_address) override;
  void SetAdapterStateController(
      AdapterStateController* adapter_state_controller) override;
  void UpdateDeviceNickname(const std::string& mac_address,
                            const std::string& nickname) override;
  void SetDeviceNameManager(DeviceNameManager* device_name_manager) override;

 private:
  base::flat_map<std::string, DeviceImageInfo> mac_address_to_images_;
  base::flat_map<std::string, std::string> mac_address_to_nickname_;
  std::vector<std::string> forgotten_device_addresses_;
  raw_ptr<AdapterStateController> adapter_state_controller_ = nullptr;
  raw_ptr<DeviceNameManager> device_name_manager_ = nullptr;
  std::vector<bluetooth_config::mojom::PairedBluetoothDevicePropertiesPtr>
      fast_pairable_device_properties_;
};

}  // namespace ash::bluetooth_config

#endif  // CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_FAKE_FAST_PAIR_DELEGATE_H_
