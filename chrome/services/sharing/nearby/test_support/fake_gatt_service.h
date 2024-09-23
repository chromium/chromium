// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_NEARBY_TEST_SUPPORT_FAKE_GATT_SERVICE_H_
#define CHROME_SERVICES_SHARING_NEARBY_TEST_SUPPORT_FAKE_GATT_SERVICE_H_

#include "device/bluetooth/public/mojom/adapter.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace bluetooth {

class FakeGattService : public mojom::GattService {
 public:
  using ValueCallback = base::OnceCallback<void(
      bluetooth::mojom::LocalCharacteristicReadResultPtr)>;

  FakeGattService();
  FakeGattService(const FakeGattService&) = delete;
  FakeGattService& operator=(const FakeGattService&) = delete;
  ~FakeGattService() override;

  // mojom::GattService:
  void CreateCharacteristic(
      const device::BluetoothUUID& characteristic_uuid,
      const device::BluetoothGattCharacteristic::Permissions& permission,
      const device::BluetoothGattCharacteristic::Properties& property,
      CreateCharacteristicCallback callback) override;
  void Register(RegisterCallback callback) override;

  void SetObserver(mojo::PendingRemote<mojom::GattServiceObserver> observer);

  void TriggerReadCharacteristicRequest(
      const device::BluetoothUUID& service_uuid,
      const device::BluetoothUUID& characteristic_uuid,
      ValueCallback callback,
      uint32_t offset = 0);

  void SetCreateCharacteristicResult(bool success);
  int GetNumCharacteristicUuids() { return characteristic_uuids_.size(); }

  void SetOnDestroyedCallback(base::OnceClosure callback) {
    on_destroyed_callback_ = std::move(callback);
  }

  void SetShouldRegisterSucceed(bool should_register_succeed);
  void CloseReceiver();

 private:
  void OnLocalCharacteristicReadResponse(
      ValueCallback callback,
      mojom::LocalCharacteristicReadResultPtr read_result);

  std::vector<device::BluetoothUUID> characteristic_uuids_;
  mojo::Remote<mojom::GattServiceObserver> observer_remote_;
  bool set_create_characteristic_result_ = false;
  base::OnceClosure on_destroyed_callback_;
  bool should_register_succeed_ = false;
  mojo::Receiver<mojom::GattService> gatt_service_{this};
};

}  // namespace bluetooth

#endif  // CHROME_SERVICES_SHARING_NEARBY_TEST_SUPPORT_FAKE_GATT_SERVICE_H_
