// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_NEARBY_PLATFORM_BLE_V2_PERIPHERAL_H_
#define CHROME_SERVICES_SHARING_NEARBY_PLATFORM_BLE_V2_PERIPHERAL_H_

#include <string>

#include "device/bluetooth/public/mojom/adapter.mojom.h"
#include "third_party/nearby/src/internal/platform/implementation/ble_v2.h"

namespace nearby::chrome {

// Concrete BleV2Peripheral implementation.
class BleV2Peripheral : public api::ble_v2::BlePeripheral {
 public:
  explicit BleV2Peripheral(bluetooth::mojom::DeviceInfoPtr device_info);
  BleV2Peripheral(const BleV2Peripheral&) = delete;
  BleV2Peripheral& operator=(const BleV2Peripheral&) = delete;
  BleV2Peripheral(BleV2Peripheral&&);
  BleV2Peripheral& operator=(BleV2Peripheral&&);

  ~BleV2Peripheral() override;

  std::string GetAddress() const override;
  UniqueId GetUniqueId() const override;
  void UpdateDeviceInfo(bluetooth::mojom::DeviceInfoPtr device_info);

 private:
  bluetooth::mojom::DeviceInfoPtr device_info_;
};

}  // namespace nearby::chrome

#endif  // CHROME_SERVICES_SHARING_NEARBY_PLATFORM_BLE_V2_PERIPHERAL_H_
