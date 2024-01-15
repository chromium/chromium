// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/ble_v2_gatt_server.h"

namespace nearby::chrome {

BleV2GattServer::BleV2GattServer(
    const mojo::SharedRemote<bluetooth::mojom::Adapter>& adapter)
    : bluetooth_adapter_(std::make_unique<BluetoothAdapter>(adapter)) {}

BleV2GattServer::~BleV2GattServer() = default;

BluetoothAdapter& BleV2GattServer::GetBlePeripheral() {
  CHECK(bluetooth_adapter_);
  return *bluetooth_adapter_;
}

std::optional<api::ble_v2::GattCharacteristic>
BleV2GattServer::CreateCharacteristic(
    const Uuid& service_uuid,
    const Uuid& characteristic_uuid,
    api::ble_v2::GattCharacteristic::Permission permission,
    api::ble_v2::GattCharacteristic::Property property) {
  // TODO(b/311430390): Implement to a call on the Mojo remote to create
  // a GATT Characteristic and construct a `GattCharacteristic` on the success
  // of creation.
  NOTIMPLEMENTED();
  return std::nullopt;
}

bool BleV2GattServer::UpdateCharacteristic(
    const api::ble_v2::GattCharacteristic& characteristic,
    const nearby::ByteArray& value) {
  // TODO(b/311430390):Implement to call on the Mojo remote to update the value
  // of the GATT Characteristic, and returns the success/failure of the
  // operation.
  NOTIMPLEMENTED();
  return false;
}

absl::Status BleV2GattServer::NotifyCharacteristicChanged(
    const api::ble_v2::GattCharacteristic& characteristic,
    bool confirm,
    const nearby::ByteArray& new_value) {
  // TODO(b/311430390): Implement to call on the Mojo remote to update the value
  // of the GATT Characteristic, and notify remote devices, and returns the
  // resulting Status of the operation.
  NOTIMPLEMENTED();
  return absl::Status();
}

void BleV2GattServer::Stop() {
  // TODO(b/311430390): Implement to call on the Mojo remote to stop the GATT
  // server.
  NOTIMPLEMENTED();
}

}  // namespace nearby::chrome
