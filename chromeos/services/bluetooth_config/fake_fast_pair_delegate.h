// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_BLUETOOTH_CONFIG_FAKE_FAST_PAIR_DELEGATE_H_
#define CHROMEOS_SERVICES_BLUETOOTH_CONFIG_FAKE_FAST_PAIR_DELEGATE_H_

#include "chromeos/services/bluetooth_config/fast_pair_delegate.h"
#include "chromeos/services/bluetooth_config/public/cpp/device_image_info.h"

namespace chromeos {
namespace bluetooth_config {

class AdapterStateController;
class DeviceNameManager;

class FakeFastPairDelegate : public FastPairDelegate {
 public:
  FakeFastPairDelegate();
  FakeFastPairDelegate(const FakeFastPairDelegate&) = delete;
  FakeFastPairDelegate& operator=(const FakeFastPairDelegate&) = delete;
  ~FakeFastPairDelegate() override;

  DeviceNameManager* device_name_manager() { return device_name_manager_; }

  // Sets |images| for |device_id| that will be returned
  // by GetDeviceImageInfo(|device_id|).
  void SetDeviceImageInfo(const std::string& device_id,
                          DeviceImageInfo& images);

  // FastPairDelegate:
  absl::optional<DeviceImageInfo> GetDeviceImageInfo(
      const std::string& device_id) override;
  void SetAdapterStateController(
      chromeos::bluetooth_config::AdapterStateController*
          adapter_state_controller) override;
  void SetDeviceNameManager(DeviceNameManager* device_name_manager) override;

 private:
  base::flat_map<std::string, DeviceImageInfo> device_id_to_images_;
  AdapterStateController* adapter_state_controller_ = nullptr;
  DeviceNameManager* device_name_manager_ = nullptr;
};

}  // namespace bluetooth_config
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_BLUETOOTH_CONFIG_FAKE_FAST_PAIR_DELEGATE_H_
