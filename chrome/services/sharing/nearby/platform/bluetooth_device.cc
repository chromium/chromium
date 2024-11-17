// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/bluetooth_device.h"

namespace nearby::chrome {

BluetoothDevice::BluetoothDevice(
    bluetooth::mojom::DeviceInfoPtr device_info,
    std::optional<base::TimeTicks> last_discovered_time)
    : device_info_(std::move(device_info)),
      last_discovered_time_(last_discovered_time) {}

BluetoothDevice::~BluetoothDevice() = default;

std::string BluetoothDevice::GetName() const {
  return device_info_->name_for_display;
}

std::string BluetoothDevice::GetMacAddress() const {
  return device_info_->address;
}

void BluetoothDevice::UpdateDevice(
    bluetooth::mojom::DeviceInfoPtr device_info,
    std::optional<base::TimeTicks> last_discovered_time) {
  DCHECK_EQ(device_info_->address, device_info->address);
  device_info_ = std::move(device_info);
  last_discovered_time_ = last_discovered_time;
}

}  // namespace nearby::chrome
