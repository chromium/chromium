// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_NEARBY_PLATFORM_BLE_V2_GATT_CLIENT_H_
#define CHROME_SERVICES_SHARING_NEARBY_PLATFORM_BLE_V2_GATT_CLIENT_H_

#include "third_party/nearby/src/internal/platform/implementation/ble_v2.h"

namespace nearby::chrome {

class BleV2GattClient : public ::nearby::api::ble_v2::GattClient {
 public:
  BleV2GattClient();
  ~BleV2GattClient() override;

  BleV2GattClient(const BleV2GattClient&) = delete;
  BleV2GattClient& operator=(const BleV2GattClient&) = delete;

  // nearby::api::ble_v2::GattClient:
  bool DiscoverServiceAndCharacteristics(
      const Uuid& service_uuid,
      const std::vector<Uuid>& characteristic_uuids) override;
  absl::optional<api::ble_v2::GattCharacteristic> GetCharacteristic(
      const Uuid& service_uuid,
      const Uuid& characteristic_uuid) override;
  absl::optional<std::string> ReadCharacteristic(
      const api::ble_v2::GattCharacteristic& characteristic) override;
  bool WriteCharacteristic(
      const api::ble_v2::GattCharacteristic& characteristic,
      absl::string_view value,
      WriteType type) override;
  bool SetCharacteristicSubscription(
      const api::ble_v2::GattCharacteristic& characteristic,
      bool enable,
      absl::AnyInvocable<void(absl::string_view value)>
          on_characteristic_changed_cb) override;
  void Disconnect() override;
};

}  // namespace nearby::chrome

#endif  // CHROME_SERVICES_SHARING_NEARBY_PLATFORM_BLE_V2_GATT_CLIENT_H_
