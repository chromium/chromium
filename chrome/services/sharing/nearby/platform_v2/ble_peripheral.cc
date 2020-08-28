// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform_v2/ble_peripheral.h"

namespace location {
namespace nearby {
namespace chrome {

BlePeripheral::BlePeripheral(api::BluetoothDevice& bluetooth_device) {}

BlePeripheral::~BlePeripheral() = default;

std::string BlePeripheral::GetName() const {
  // TODO(hansberry): Implement.
  return std::string();
}

ByteArray BlePeripheral::GetAdvertisementBytes(
    const std::string& service_id) const {
  // TODO(hansberry): Implement.
  return ByteArray();
}

}  // namespace chrome
}  // namespace nearby
}  // namespace location
