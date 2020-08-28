// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_NEARBY_PLATFORM_V2_BLE_MEDIUM_H_
#define CHROME_SERVICES_SHARING_NEARBY_PLATFORM_V2_BLE_MEDIUM_H_

#include <string>

#include "device/bluetooth/public/mojom/adapter.mojom.h"
#include "third_party/nearby/src/cpp/platform_v2/api/ble.h"

namespace location {
namespace nearby {
namespace chrome {

// Concrete BleMedium implementation.
class BleMedium : public api::BleMedium {
 public:
  BleMedium();
  ~BleMedium() override;

  BleMedium(const BleMedium&) = delete;
  BleMedium& operator=(const BleMedium&) = delete;

  // api::BleMedium:
  bool StartAdvertising(const std::string& service_id,
                        const ByteArray& advertisement) override;
  bool StopAdvertising(const std::string& service_id) override;
  bool StartScanning(
      const std::string& service_id,
      DiscoveredPeripheralCallback discovered_peripheral_callback) override;
  bool StopScanning(const std::string& service_id) override;
  bool StartAcceptingConnections(
      const std::string& service_id,
      AcceptedConnectionCallback accepted_connection_callback) override;
  bool StopAcceptingConnections(const std::string& service_id) override;
  std::unique_ptr<api::BleSocket> Connect(
      api::BlePeripheral& ble_peripheral,
      const std::string& service_id) override;
};

}  // namespace chrome
}  // namespace nearby
}  // namespace location

#endif  // CHROME_SERVICES_SHARING_NEARBY_PLATFORM_V2_BLE_MEDIUM_H_
