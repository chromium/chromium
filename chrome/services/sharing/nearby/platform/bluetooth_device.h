// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_NEARBY_PLATFORM_BLUETOOTH_DEVICE_H_
#define CHROME_SERVICES_SHARING_NEARBY_PLATFORM_BLUETOOTH_DEVICE_H_

#include <optional>
#include <string>

#include "base/time/time.h"
#include "device/bluetooth/public/mojom/adapter.mojom.h"
#include "third_party/nearby/src/internal/platform/implementation/bluetooth_classic.h"

namespace nearby::chrome {

// Concrete BluetoothDevice implementation.
class BluetoothDevice : public api::BluetoothDevice {
 public:
  BluetoothDevice(
      bluetooth::mojom::DeviceInfoPtr device_info,
      std::optional<base::TimeTicks> last_discovered_time = std::nullopt);
  ~BluetoothDevice() override;

  BluetoothDevice(const BluetoothDevice&) = delete;
  BluetoothDevice& operator=(const BluetoothDevice&) = delete;

  // api::BluetoothDevice:
  std::string GetName() const override;
  std::string GetMacAddress() const override;

  std::optional<base::TimeTicks> GetLastDiscoveredTime() {
    return last_discovered_time_;
  }

  void UpdateDevice(bluetooth::mojom::DeviceInfoPtr device_info,
                    std::optional<base::TimeTicks> last_discovered_time);

 private:
  bluetooth::mojom::DeviceInfoPtr device_info_;

  // Time when last the Bluetooth device was added/changed by the adapter.
  // Used by BluetoothClassicMedium to remove stale devices during discovery.
  std::optional<base::TimeTicks> last_discovered_time_ = std::nullopt;
};

}  // namespace nearby::chrome

#endif  // CHROME_SERVICES_SHARING_NEARBY_PLATFORM_BLUETOOTH_DEVICE_H_
