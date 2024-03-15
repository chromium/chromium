// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_NEARBY_TEST_SUPPORT_FAKE_GATT_SERVICE_H_
#define CHROME_SERVICES_SHARING_NEARBY_TEST_SUPPORT_FAKE_GATT_SERVICE_H_

#include "device/bluetooth/public/mojom/adapter.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace bluetooth {

class FakeGattService : public mojom::GattService {
 public:
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

  void SetCreateCharacteristicResult(bool success);
  int GetNumCharacteristicUuids() { return characteristic_uuids_.size(); }

 private:
  std::vector<device::BluetoothUUID> characteristic_uuids_;
  bool set_create_characteristic_result_ = false;
  mojo::Receiver<mojom::GattService> gatt_server_{this};
};

}  // namespace bluetooth

#endif  // CHROME_SERVICES_SHARING_NEARBY_TEST_SUPPORT_FAKE_GATT_SERVICE_H_
