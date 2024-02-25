// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/ble_v2_remote_peripheral.h"

namespace nearby::chrome {

BleV2RemotePeripheral::BleV2RemotePeripheral(
    bluetooth::mojom::DeviceInfoPtr device_info)
    : device_info_(std::move(device_info)) {}

BleV2RemotePeripheral::BleV2RemotePeripheral(BleV2RemotePeripheral&&) = default;

BleV2RemotePeripheral& BleV2RemotePeripheral::operator=(
    BleV2RemotePeripheral&&) = default;

BleV2RemotePeripheral::~BleV2RemotePeripheral() = default;

std::string BleV2RemotePeripheral::GetAddress() const {
  return device_info_->address;
}

BleV2RemotePeripheral::UniqueId BleV2RemotePeripheral::GetUniqueId() const {
  NOTIMPLEMENTED();
  return 0;
}

void BleV2RemotePeripheral::UpdateDeviceInfo(
    bluetooth::mojom::DeviceInfoPtr device_info) {
  DCHECK_EQ(device_info_->address, device_info->address);
  device_info_ = std::move(device_info);
}

}  // namespace nearby::chrome
