// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_NEARBY_PLATFORM_BLE_V2_GATT_SERVER_H_
#define CHROME_SERVICES_SHARING_NEARBY_PLATFORM_BLE_V2_GATT_SERVER_H_

#include "chrome/services/sharing/nearby/platform/bluetooth_adapter.h"
#include "device/bluetooth/public/mojom/adapter.mojom.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "third_party/nearby/src/internal/platform/implementation/ble_v2.h"

namespace nearby::chrome {

class BleV2GattServer : public ::nearby::api::ble_v2::GattServer {
 public:
  explicit BleV2GattServer(
      const mojo::SharedRemote<bluetooth::mojom::Adapter>& adapter);
  ~BleV2GattServer() override;

  BleV2GattServer(const BleV2GattServer&) = delete;
  BleV2GattServer& operator=(const BleV2GattServer&) = delete;

  // nearby::api::ble_v2::GattServer:
  BluetoothAdapter& GetBlePeripheral() override;
  std::optional<api::ble_v2::GattCharacteristic> CreateCharacteristic(
      const Uuid& service_uuid,
      const Uuid& characteristic_uuid,
      api::ble_v2::GattCharacteristic::Permission permission,
      api::ble_v2::GattCharacteristic::Property property) override;
  bool UpdateCharacteristic(
      const api::ble_v2::GattCharacteristic& characteristic,
      const nearby::ByteArray& value) override;
  absl::Status NotifyCharacteristicChanged(
      const api::ble_v2::GattCharacteristic& characteristic,
      bool confirm,
      const nearby::ByteArray& new_value) override;
  void Stop() override;

 private:
  std::unique_ptr<BluetoothAdapter> bluetooth_adapter_;
};

}  // namespace nearby::chrome

#endif  // CHROME_SERVICES_SHARING_NEARBY_PLATFORM_BLE_V2_GATT_SERVER_H_
