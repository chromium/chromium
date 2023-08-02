// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/bluetooth_config/fake_fast_pair_delegate.h"

namespace ash::bluetooth_config {

FakeFastPairDelegate::FakeFastPairDelegate() = default;

FakeFastPairDelegate::~FakeFastPairDelegate() = default;

void FakeFastPairDelegate::SetDeviceImageInfo(const std::string& device_id,
                                              DeviceImageInfo& images) {
  mac_address_to_images_[device_id] = images;
}

// Unimplemented.
std::vector<mojom::PairedBluetoothDevicePropertiesPtr>
FakeFastPairDelegate::GetFastPairableDeviceProperties() {
  return std::vector<mojom::PairedBluetoothDevicePropertiesPtr>();
}

void FakeFastPairDelegate::SetAdapterStateController(
    AdapterStateController* adapter_state_controller) {
  adapter_state_controller_ = adapter_state_controller;
}

void FakeFastPairDelegate::UpdateDeviceNickname(const std::string& mac_address,
                                                const std::string& nickname) {
  mac_address_to_nickname_.insert_or_assign(mac_address, nickname);
}

void FakeFastPairDelegate::SetDeviceNameManager(
    DeviceNameManager* device_name_manager) {
  device_name_manager_ = device_name_manager;
}

absl::optional<DeviceImageInfo> FakeFastPairDelegate::GetDeviceImageInfo(
    const std::string& mac_address) {
  const auto it = mac_address_to_images_.find(mac_address);
  if (it == mac_address_to_images_.end()) {
    return absl::nullopt;
  }
  return it->second;
}

void FakeFastPairDelegate::ForgetDevice(const std::string& mac_address) {
  forgotten_device_addresses_.push_back(mac_address);
}

}  // namespace ash::bluetooth_config
