// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/ble_v2_remote_peripheral.h"

#include "base/compiler_specific.h"
#include "device/bluetooth/public/cpp/bluetooth_address.h"

namespace {

nearby::chrome::BleV2RemotePeripheral::UniqueId GenerateUniqueId(
    const std::string& device_address) {
  std::array<uint8_t, 6> address_bytes;
  if (!device::ParseBluetoothAddress(device_address, address_bytes)) {
    LOG(WARNING) << __func__ << ": failed to parse device address";
    return 0;
  }

  uint64_t unique_id = 0;
  UNSAFE_TODO(
      std::memcpy(&unique_id, address_bytes.data(), address_bytes.size()));
  return unique_id;
}

}  // namespace

namespace nearby::chrome {

BleV2RemotePeripheral::BleV2RemotePeripheral(
    bluetooth::mojom::DeviceInfoPtr device_info)
    : address_(std::move(device_info->address)),
      unique_id_(GenerateUniqueId(address_)) {}

BleV2RemotePeripheral::BleV2RemotePeripheral(BleV2RemotePeripheral&&) = default;

BleV2RemotePeripheral& BleV2RemotePeripheral::operator=(
    BleV2RemotePeripheral&&) = default;

BleV2RemotePeripheral::~BleV2RemotePeripheral() = default;

std::string BleV2RemotePeripheral::GetAddress() const {
  return address_;
}

BleV2RemotePeripheral::UniqueId BleV2RemotePeripheral::GetUniqueId() const {
  return unique_id_;
}

void BleV2RemotePeripheral::UpdateDeviceInfo(
    bluetooth::mojom::DeviceInfoPtr device_info) {
  DCHECK_EQ(address_, device_info->address);
}

}  // namespace nearby::chrome
