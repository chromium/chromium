// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/ble_v2_gatt_client.h"

#include "base/logging.h"
#include "base/notreached.h"

namespace nearby::chrome {

BleV2GattClient::BleV2GattClient(
    mojo::PendingRemote<bluetooth::mojom::Device> device)
    : remote_device_(std::move(device)) {
  // TODO(b/311430390): For now, just tear down the connection on disconnect. In
  // the future, this should call Disconnect.
  remote_device_.reset_on_disconnect();
}

BleV2GattClient::~BleV2GattClient() {
  Disconnect();
}

bool BleV2GattClient::DiscoverServiceAndCharacteristics(
    const Uuid& service_uuid,
    const std::vector<Uuid>& characteristic_uuids) {
  // TODO(b/311430390): Implement this function.
  NOTIMPLEMENTED();
  return false;
}

std::optional<api::ble_v2::GattCharacteristic>
BleV2GattClient::GetCharacteristic(const Uuid& service_uuid,
                                   const Uuid& characteristic_uuid) {
  // TODO(b/311430390): Implement this function.
  NOTIMPLEMENTED();
  return std::nullopt;
}

std::optional<std::string> BleV2GattClient::ReadCharacteristic(
    const api::ble_v2::GattCharacteristic& characteristic) {
  // TODO(b/311430390): Implement this function.
  NOTIMPLEMENTED();
  return std::nullopt;
}

bool BleV2GattClient::WriteCharacteristic(
    const api::ble_v2::GattCharacteristic& characteristic,
    std::string_view value,
    WriteType type) {
  // TODO(b/311430390): Implement this function.
  NOTIMPLEMENTED();
  return false;
}

bool BleV2GattClient::SetCharacteristicSubscription(
    const api::ble_v2::GattCharacteristic& characteristic,
    bool enable,
    absl::AnyInvocable<void(std::string_view value)>
        on_characteristic_changed_cb) {
  // TODO(b/311430390): Implement this function.
  NOTIMPLEMENTED();
  return false;
}

void BleV2GattClient::Disconnect() {
  // TODO(b/311430390): For now, just tear down the connection when we call
  // Disconnect. In the future this should clean up state.
  remote_device_.reset();
}

}  // namespace nearby::chrome
