// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform_v2/ble_peripheral.h"

namespace location {
namespace nearby {
namespace chrome {

BlePeripheral::BlePeripheral(bluetooth::mojom::DeviceInfoPtr device_info)
    : device_info_(std::move(device_info)) {}

BlePeripheral::~BlePeripheral() = default;

std::string BlePeripheral::GetName() const {
  return device_info_->name_for_display;
}

ByteArray BlePeripheral::GetAdvertisementBytes(
    const std::string& service_id) const {
  const auto& service_data_map = device_info_->service_data_map;

  auto it = service_data_map.find(device::BluetoothUUID(service_id));
  if (it == service_data_map.end())
    return ByteArray();

  std::string service_data(it->second.begin(), it->second.end());
  return ByteArray(service_data);
}

void BlePeripheral::UpdateDeviceInfo(
    bluetooth::mojom::DeviceInfoPtr device_info) {
  DCHECK_EQ(device_info_->address, device_info->address);
  device_info_ = std::move(device_info);
}

}  // namespace chrome
}  // namespace nearby
}  // namespace location
