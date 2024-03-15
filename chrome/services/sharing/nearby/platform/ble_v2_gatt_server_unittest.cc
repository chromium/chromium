// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/ble_v2_gatt_server.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "chrome/services/sharing/nearby/platform/bluetooth_adapter.h"
#include "chrome/services/sharing/nearby/test_support/fake_adapter.h"
#include "chrome/services/sharing/nearby/test_support/fake_gatt_service.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/nearby/src/internal/platform/uuid.h"

namespace {

const char kServiceId[] = "TestServiceId";
const char kCharacteristicUuid1[] = "1234";
const char kCharacteristicUuid2[] = "4321";

}  // namespace

namespace nearby::chrome {

class BleV2GattServerTest : public testing::Test {
 public:
  BleV2GattServerTest() = default;
  ~BleV2GattServerTest() override = default;
  BleV2GattServerTest(const BleV2GattServerTest&) = delete;
  BleV2GattServerTest& operator=(const BleV2GattServerTest&) = delete;

  void SetUp() override {
    auto fake_adapter = std::make_unique<bluetooth::FakeAdapter>();
    fake_adapter_ = fake_adapter.get();
    mojo::PendingRemote<bluetooth::mojom::Adapter> pending_adapter;
    mojo::MakeSelfOwnedReceiver(std::move(fake_adapter),
                                remote_adapter_.BindNewPipeAndPassReceiver());
    ble_v2_gatt_server_ = std::make_unique<BleV2GattServer>(remote_adapter_);
  }

  void CallCreateCharacteristic(std::string characteristic_uuid,
                                bool expected_success) {
    gatt_characteristic_ = ble_v2_gatt_server_->CreateCharacteristic(
        /*service_uuid=*/Uuid(/*data=*/kServiceId),
        /*characteristic_uuid=*/Uuid(/*data=*/characteristic_uuid),
        /*permission=*/api::ble_v2::GattCharacteristic::Permission::kRead,
        /*property=*/api::ble_v2::GattCharacteristic::Property::kRead);
    EXPECT_EQ(expected_success, gatt_characteristic_.has_value());
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::optional<api::ble_v2::GattCharacteristic> gatt_characteristic_;
  raw_ptr<bluetooth::FakeAdapter> fake_adapter_;
  mojo::SharedRemote<bluetooth::mojom::Adapter> remote_adapter_;
  std::unique_ptr<BleV2GattServer> ble_v2_gatt_server_;
};

TEST_F(BleV2GattServerTest, GetBlePeripheral) {
  BluetoothAdapter& ble_v2_periphral = ble_v2_gatt_server_->GetBlePeripheral();
  EXPECT_EQ(fake_adapter_->address_, ble_v2_periphral.GetAddress());
}

TEST_F(BleV2GattServerTest,
       CreateCharacteristic_CreateGattService_AlreadyExists) {
  auto fake_gatt_service = std::make_unique<bluetooth::FakeGattService>();
  fake_gatt_service->SetCreateCharacteristicResult(/*success=*/true);
  auto* fake_gatt_service_ptr = fake_gatt_service.get();
  fake_adapter_->SetCreateLocalGattServiceResult(
      /*gatt_service=*/std::move(fake_gatt_service));

  // First time, expect a call to the browser process to get or create
  // a `GattService` since it doesn't exist yet.
  {
    base::MockCallback<base::OnceClosure> callback;
    EXPECT_CALL(callback, Run).Times(1);
    fake_adapter_->SetCreateLocalGattServiceCallback(callback.Get());
    CallCreateCharacteristic(/*characteristic_uuid=*/kCharacteristicUuid1,
                             /*expected_success=*/true);
  }

  // Second time, expect no call to browser process since it already
  // exists for the same service id.
  {
    base::MockCallback<base::OnceClosure> callback;
    EXPECT_CALL(callback, Run).Times(0);
    fake_adapter_->SetCreateLocalGattServiceCallback(callback.Get());
    CallCreateCharacteristic(
        /*characteristic_uuid=*/kCharacteristicUuid2,
        /*expected_success=*/true);
  }

  EXPECT_EQ(2, fake_gatt_service_ptr->GetNumCharacteristicUuids());
}

TEST_F(BleV2GattServerTest, CreateCharacteristic_Success) {
  auto fake_gatt_service = std::make_unique<bluetooth::FakeGattService>();
  fake_gatt_service->SetCreateCharacteristicResult(/*success=*/true);
  fake_adapter_->SetCreateLocalGattServiceResult(
      /*gatt_service=*/std::move(fake_gatt_service));

  CallCreateCharacteristic(/*characteristic_uuid=*/kCharacteristicUuid1,
                           /*expected_success=*/true);
}

TEST_F(BleV2GattServerTest, CreateCharacteristic_Failure) {
  auto fake_gatt_service = std::make_unique<bluetooth::FakeGattService>();
  fake_gatt_service->SetCreateCharacteristicResult(/*success=*/false);
  fake_adapter_->SetCreateLocalGattServiceResult(
      /*gatt_service=*/std::move(fake_gatt_service));

  CallCreateCharacteristic(/*characteristic_uuid=*/kCharacteristicUuid1,
                           /*expected_success=*/false);
}

TEST_F(BleV2GattServerTest,
       CreateCharacteristic_CreateCharacteristic_AlreadyExists) {
  auto fake_gatt_service = std::make_unique<bluetooth::FakeGattService>();
  fake_gatt_service->SetCreateCharacteristicResult(/*success=*/true);
  auto* fake_gatt_service_ptr = fake_gatt_service.get();
  fake_adapter_->SetCreateLocalGattServiceResult(
      /*gatt_service=*/std::move(fake_gatt_service));

  // First time, expect a call to the browser process to get or create
  // a GATT characteristic since it isn't in the map yet.
  {
    CallCreateCharacteristic(
        /*characteristic_uuid=*/kCharacteristicUuid1,
        /*expected_success=*/true);
    EXPECT_EQ(1, fake_gatt_service_ptr->GetNumCharacteristicUuids());
  }

  // Second time, expect no call to browser process since it already
  // exists for the same characteristic id.
  {
    CallCreateCharacteristic(
        /*characteristic_uuid=*/kCharacteristicUuid1,
        /*expected_success=*/true);
    EXPECT_EQ(1, fake_gatt_service_ptr->GetNumCharacteristicUuids());
  }
}

}  // namespace nearby::chrome
