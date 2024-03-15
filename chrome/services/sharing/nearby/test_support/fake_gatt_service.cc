// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/test_support/fake_gatt_service.h"

namespace bluetooth {

FakeGattService::FakeGattService() = default;

FakeGattService::~FakeGattService() = default;

void FakeGattService::CreateCharacteristic(
    const device::BluetoothUUID& characteristic_uuid,
    const device::BluetoothGattCharacteristic::Permissions& permissions,
    const device::BluetoothGattCharacteristic::Properties& properties,
    CreateCharacteristicCallback callback) {
  characteristic_uuids_.push_back(characteristic_uuid);
  std::move(callback).Run(/*success=*/set_create_characteristic_result_);
}

void FakeGattService::SetCreateCharacteristicResult(bool success) {
  set_create_characteristic_result_ = success;
}

}  // namespace bluetooth
