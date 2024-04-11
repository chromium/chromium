// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/test_support/fake_gatt_service.h"

namespace {

const uint32_t kReadCharacteristicOffset = 0;

}  // namespace

namespace bluetooth {

FakeGattService::FakeGattService() = default;

FakeGattService::~FakeGattService() {
  if (on_destroyed_callback_) {
    std::move(on_destroyed_callback_).Run();
  }
}

void FakeGattService::CreateCharacteristic(
    const device::BluetoothUUID& characteristic_uuid,
    const device::BluetoothGattCharacteristic::Permissions& permissions,
    const device::BluetoothGattCharacteristic::Properties& properties,
    CreateCharacteristicCallback callback) {
  characteristic_uuids_.push_back(characteristic_uuid);
  std::move(callback).Run(/*success=*/set_create_characteristic_result_);
}

void FakeGattService::SetObserver(
    mojo::PendingRemote<mojom::GattServiceObserver> observer) {
  observer_remote_.Bind(std::move(observer));
}

void FakeGattService::SetCreateCharacteristicResult(bool success) {
  set_create_characteristic_result_ = success;
}

void FakeGattService::TriggerReadCharacteristicRequest(
    const device::BluetoothUUID& service_uuid,
    const device::BluetoothUUID& characteristic_uuid,
    ValueCallback callback) {
  observer_remote_->OnLocalCharacteristicRead(
      /*device=*/mojom::DeviceInfo::New(),
      /*characteristic_uuid=*/characteristic_uuid,
      /*service_uuid=*/service_uuid,
      /*offset=*/kReadCharacteristicOffset,
      /*callback=*/
      base::BindOnce(&FakeGattService::OnLocalCharacteristicReadResponse,
                     base::Unretained(this), std::move(callback)));
}

void FakeGattService::OnLocalCharacteristicReadResponse(
    ValueCallback callback,
    mojom::LocalCharacteristicReadResultPtr read_result) {
  std::move(callback).Run(std::move(read_result));
}

}  // namespace bluetooth
