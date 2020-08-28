// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_NEARBY_PLATFORM_V2_BLE_PERIPHERAL_H_
#define CHROME_SERVICES_SHARING_NEARBY_PLATFORM_V2_BLE_PERIPHERAL_H_

#include "third_party/nearby/src/cpp/platform_v2/api/ble.h"

namespace location {
namespace nearby {
namespace chrome {

// Concrete BlePeripheral implementation.
class BlePeripheral : public api::BlePeripheral {
 public:
  explicit BlePeripheral(api::BluetoothDevice& bluetooth_device);
  ~BlePeripheral() override;

  BlePeripheral(const BlePeripheral&) = delete;
  BlePeripheral& operator=(const BlePeripheral&) = delete;

  // api::BlePeripheral:
  std::string GetName() const override;
  ByteArray GetAdvertisementBytes(const std::string& service_id) const override;
};

}  // namespace chrome
}  // namespace nearby
}  // namespace location

#endif  // CHROME_SERVICES_SHARING_NEARBY_PLATFORM_V2_BLE_PERIPHERAL_H_
