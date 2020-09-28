// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_NEARBY_PLATFORM_V2_BLUETOOTH_DEVICE_H_
#define CHROME_SERVICES_SHARING_NEARBY_PLATFORM_V2_BLUETOOTH_DEVICE_H_

#include <string>

#include "device/bluetooth/public/mojom/adapter.mojom.h"
#include "third_party/nearby/src/cpp/platform_v2/api/bluetooth_classic.h"

namespace location {
namespace nearby {
namespace chrome {

// Concrete BluetoothDevice implementation.
class BluetoothDevice : public api::BluetoothDevice {
 public:
  explicit BluetoothDevice(bluetooth::mojom::DeviceInfoPtr device_info);
  ~BluetoothDevice() override;

  BluetoothDevice(const BluetoothDevice&) = delete;
  BluetoothDevice& operator=(const BluetoothDevice&) = delete;

  // api::BluetoothDevice:
  std::string GetName() const override;
  std::string GetMacAddress() const override;

  void UpdateDeviceInfo(bluetooth::mojom::DeviceInfoPtr device_info);

 private:
  bluetooth::mojom::DeviceInfoPtr device_info_;
};

}  // namespace chrome
}  // namespace nearby
}  // namespace location

#endif  // CHROME_SERVICES_SHARING_NEARBY_PLATFORM_V2_BLUETOOTH_DEVICE_H_
