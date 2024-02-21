// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_NEARBY_PLATFORM_BLE_V2_GATT_CLIENT_H_
#define CHROME_SERVICES_SHARING_NEARBY_PLATFORM_BLE_V2_GATT_CLIENT_H_

#include "device/bluetooth/public/mojom/device.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/nearby/src/internal/platform/implementation/ble_v2.h"

namespace nearby::chrome {

class BleV2GattClient : public ::nearby::api::ble_v2::GattClient {
 public:
  explicit BleV2GattClient(
      mojo::PendingRemote<bluetooth::mojom::Device> device);
  ~BleV2GattClient() override;

  BleV2GattClient(const BleV2GattClient&) = delete;
  BleV2GattClient& operator=(const BleV2GattClient&) = delete;

  // nearby::api::ble_v2::GattClient:
  bool DiscoverServiceAndCharacteristics(
      const Uuid& service_uuid,
      const std::vector<Uuid>& characteristic_uuids) override;
  std::optional<api::ble_v2::GattCharacteristic> GetCharacteristic(
      const Uuid& service_uuid,
      const Uuid& characteristic_uuid) override;
  std::optional<std::string> ReadCharacteristic(
      const api::ble_v2::GattCharacteristic& characteristic) override;
  bool WriteCharacteristic(
      const api::ble_v2::GattCharacteristic& characteristic,
      std::string_view value,
      WriteType type) override;
  bool SetCharacteristicSubscription(
      const api::ble_v2::GattCharacteristic& characteristic,
      bool enable,
      absl::AnyInvocable<void(std::string_view value)>
          on_characteristic_changed_cb) override;
  void Disconnect() override;

 private:
  mojo::Remote<bluetooth::mojom::Device> remote_device_;
};

}  // namespace nearby::chrome

#endif  // CHROME_SERVICES_SHARING_NEARBY_PLATFORM_BLE_V2_GATT_CLIENT_H_
