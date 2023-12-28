// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/ble_v2_gatt_client.h"

#include "base/notreached.h"

namespace nearby::chrome {

BleV2GattClient::BleV2GattClient() = default;

BleV2GattClient::~BleV2GattClient() = default;

bool BleV2GattClient::DiscoverServiceAndCharacteristics(
    const Uuid& service_uuid,
    const std::vector<Uuid>& characteristic_uuids) {
  // TODO(b/311430390): Implement this function.
  NOTIMPLEMENTED();
  return false;
}

absl::optional<api::ble_v2::GattCharacteristic>
BleV2GattClient::GetCharacteristic(const Uuid& service_uuid,
                                   const Uuid& characteristic_uuid) {
  // TODO(b/311430390): Implement this function.
  NOTIMPLEMENTED();
  return absl::nullopt;
}

absl::optional<std::string> BleV2GattClient::ReadCharacteristic(
    const api::ble_v2::GattCharacteristic& characteristic) {
  // TODO(b/311430390): Implement this function.
  NOTIMPLEMENTED();
  return absl::nullopt;
}

bool BleV2GattClient::WriteCharacteristic(
    const api::ble_v2::GattCharacteristic& characteristic,
    absl::string_view value,
    WriteType type) {
  // TODO(b/311430390): Implement this function.
  NOTIMPLEMENTED();
  return false;
}

bool BleV2GattClient::SetCharacteristicSubscription(
    const api::ble_v2::GattCharacteristic& characteristic,
    bool enable,
    absl::AnyInvocable<void(absl::string_view value)>
        on_characteristic_changed_cb) {
  // TODO(b/311430390): Implement this function.
  NOTIMPLEMENTED();
  return false;
}

void BleV2GattClient::Disconnect() {
  // TODO(b/311430390): Implement this function.
  NOTIMPLEMENTED();
}

}  // namespace nearby::chrome
