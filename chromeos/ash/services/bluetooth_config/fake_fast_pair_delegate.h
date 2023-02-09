// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_FAKE_FAST_PAIR_DELEGATE_H_
#define CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_FAKE_FAST_PAIR_DELEGATE_H_

#include <vector>

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

  // FastPairDelegate:
  absl::optional<DeviceImageInfo> GetDeviceImageInfo(
      const std::string& mac_address) override;
  void ForgetDevice(const std::string& mac_address) override;
  void SetAdapterStateController(
      AdapterStateController* adapter_state_controller) override;
  void SetDeviceNameManager(DeviceNameManager* device_name_manager) override;

 private:
  base::flat_map<std::string, DeviceImageInfo> mac_address_to_images_;
  std::vector<std::string> forgotten_device_addresses_;
  AdapterStateController* adapter_state_controller_ = nullptr;
  DeviceNameManager* device_name_manager_ = nullptr;
};

}  // namespace ash::bluetooth_config

#endif  // CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_FAKE_FAST_PAIR_DELEGATE_H_
