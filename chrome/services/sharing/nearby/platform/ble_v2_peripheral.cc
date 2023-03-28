// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/ble_v2_peripheral.h"

namespace nearby::chrome {

BleV2Peripheral::BleV2Peripheral(bluetooth::mojom::DeviceInfoPtr device_info)
    : device_info_(std::move(device_info)) {}

BleV2Peripheral::BleV2Peripheral(BleV2Peripheral&&) = default;

BleV2Peripheral& BleV2Peripheral::operator=(BleV2Peripheral&&) = default;

BleV2Peripheral::~BleV2Peripheral() = default;

std::string BleV2Peripheral::GetAddress() const {
  return device_info_->address;
}

void BleV2Peripheral::UpdateDeviceInfo(
    bluetooth::mojom::DeviceInfoPtr device_info) {
  DCHECK_EQ(device_info_->address, device_info->address);
  device_info_ = std::move(device_info);
}

}  // namespace nearby::chrome
