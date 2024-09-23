// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/ble_v2_gatt_server.h"

#include <memory>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/services/sharing/nearby/platform/bluetooth_adapter.h"
#include "chrome/services/sharing/nearby/platform/bluetooth_utils.h"
#include "chrome/services/sharing/nearby/test_support/fake_adapter.h"
#include "chrome/services/sharing/nearby/test_support/fake_gatt_service.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/nearby/src/internal/platform/uuid.h"

namespace {

const device::BluetoothUUID kServiceId =
    device::BluetoothUUID("12345678-1234-5678-9abc-def123456789");
const device::BluetoothUUID kCharacteristicUuid1 =
    device::BluetoothUUID("00001101-0000-1000-8000-00805f9b34fb");
const device::BluetoothUUID kCharacteristicUuid2 =
    device::BluetoothUUID("00001102-0000-1000-8000-00805f9b34fc");
const std::string kNewCharacteristicValue = "123456";
const int kPartialBufferOffset = 2;

class FakeGattService : public nearby::chrome::BleV2GattServer::GattService {
 public:
  explicit FakeGattService(base::OnceClosure on_destroyed_callback)
      : on_destroyed_callback_(std::move(on_destroyed_callback)) {}

  ~FakeGattService() override {
    if (on_destroyed_callback_) {
      std::move(on_destroyed_callback_).Run();
    }
  }

 private:
  base::OnceClosure on_destroyed_callback_;
};

class FakeGattServiceFactory
    : public nearby::chrome::BleV2GattServer::GattService::Factory {
 public:
  std::unique_ptr<nearby::chrome::BleV2GattServer::GattService> Create()
      override {
    return std::make_unique<FakeGattService>(
        std::move(next_fake_gatt_service_destroyed_callback_));
  }

  void SetNextFakeGattServiceDestroyedCallback(
      base::OnceClosure on_destroyed_callback) {
    next_fake_gatt_service_destroyed_callback_ =
        std::move(on_destroyed_callback);
  }

 private:
  base::OnceClosure next_fake_gatt_service_destroyed_callback_;
};

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

    auto fake_gatt_service_factory = std::make_unique<FakeGattServiceFactory>();
    fake_gatt_service_factory_ = fake_gatt_service_factory.get();
    ble_v2_gatt_server_ = std::make_unique<BleV2GattServer>(
        remote_adapter_, std::move(fake_gatt_service_factory));
  }

  void CallCreateCharacteristic(
      device::BluetoothUUID characteristic_uuid,
      bool expected_success,
      api::ble_v2::GattCharacteristic::Permission permission =
          api::ble_v2::GattCharacteristic::Permission::kRead,
      api::ble_v2::GattCharacteristic::Property property =
          api::ble_v2::GattCharacteristic::Property::kRead) {
    gatt_characteristic_ = ble_v2_gatt_server_->CreateCharacteristic(
        /*service_uuid=*/BluetoothUuidToNearbyUuid(kServiceId),
        /*characteristic_uuid=*/BluetoothUuidToNearbyUuid(characteristic_uuid),
        /*permission=*/permission,
        /*property=*/property);
    EXPECT_EQ(expected_success, gatt_characteristic_.has_value());
  }

  void CallUpdateCharacteristic(
      device::BluetoothUUID characteristic_uuid,
      bool expected_success,
      api::ble_v2::GattCharacteristic::Permission permission =
          api::ble_v2::GattCharacteristic::Permission::kRead,
      api::ble_v2::GattCharacteristic::Property property =
          api::ble_v2::GattCharacteristic::Property::kRead) {
    api::ble_v2::GattCharacteristic gatt_characteristic = {
        BluetoothUuidToNearbyUuid(characteristic_uuid),
        BluetoothUuidToNearbyUuid(kServiceId), permission, property};
    bool result = ble_v2_gatt_server_->UpdateCharacteristic(
        /*characteristic=*/gatt_characteristic,
        /*value=*/nearby::ByteArray(kNewCharacteristicValue));
    EXPECT_EQ(expected_success, result);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::optional<api::ble_v2::GattCharacteristic> gatt_characteristic_;
  raw_ptr<bluetooth::FakeAdapter> fake_adapter_;
  mojo::SharedRemote<bluetooth::mojom::Adapter> remote_adapter_;
  std::unique_ptr<BleV2GattServer> ble_v2_gatt_server_;
  raw_ptr<FakeGattServiceFactory> fake_gatt_service_factory_;
  base::HistogramTester histogram_tester_;
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
    histogram_tester_.ExpectBucketCount(
        "Nearby.Connections.BleV2.GattServer.CreateLocalGattService.Result",
        /*bucket: success=*/1, 1);
    histogram_tester_.ExpectBucketCount(
        "Nearby.Connections.BleV2.GattServer.CreateCharacteristic.Result",
        /*bucket: success=*/1, 1);
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
    histogram_tester_.ExpectBucketCount(
        "Nearby.Connections.BleV2.GattServer.CreateLocalGattService.Result",
        /*bucket: success=*/1, 1);
    histogram_tester_.ExpectBucketCount(
        "Nearby.Connections.BleV2.GattServer.CreateCharacteristic.Result",
        /*bucket: success=*/1, 2);
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
  histogram_tester_.ExpectBucketCount(
      "Nearby.Connections.BleV2.GattServer.CreateLocalGattService.Result",
      /*bucket: success=*/1, 1);
  histogram_tester_.ExpectBucketCount(
      "Nearby.Connections.BleV2.GattServer.CreateCharacteristic.Result",
      /*bucket: success=*/1, 1);
}

TEST_F(BleV2GattServerTest, CreateCharacteristic_Failure) {
  auto fake_gatt_service = std::make_unique<bluetooth::FakeGattService>();
  fake_gatt_service->SetCreateCharacteristicResult(/*success=*/false);
  fake_adapter_->SetCreateLocalGattServiceResult(
      /*gatt_service=*/std::move(fake_gatt_service));

  CallCreateCharacteristic(/*characteristic_uuid=*/kCharacteristicUuid1,
                           /*expected_success=*/false);
  histogram_tester_.ExpectBucketCount(
      "Nearby.Connections.BleV2.GattServer.CreateLocalGattService.Result",
      /*bucket: success=*/1, 1);
  histogram_tester_.ExpectBucketCount(
      "Nearby.Connections.BleV2.GattServer.CreateCharacteristic.Result",
      /*bucket: success=*/0, 1);
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
    histogram_tester_.ExpectBucketCount(
        "Nearby.Connections.BleV2.GattServer.CreateLocalGattService.Result",
        /*bucket: success=*/1, 1);
    histogram_tester_.ExpectBucketCount(
        "Nearby.Connections.BleV2.GattServer.CreateCharacteristic.Result",
        /*bucket: success=*/1, 1);
  }

  // Second time, expect no call to browser process since it already
  // exists for the same characteristic id.
  {
    CallCreateCharacteristic(
        /*characteristic_uuid=*/kCharacteristicUuid1,
        /*expected_success=*/true);
    EXPECT_EQ(1, fake_gatt_service_ptr->GetNumCharacteristicUuids());
    histogram_tester_.ExpectBucketCount(
        "Nearby.Connections.BleV2.GattServer.CreateLocalGattService.Result",
        /*bucket: success=*/1, 1);
    histogram_tester_.ExpectBucketCount(
        "Nearby.Connections.BleV2.GattServer.CreateCharacteristic.Result",
        /*bucket: success=*/1, 1);
  }
}

TEST_F(BleV2GattServerTest,
       UpdateCharacteristic_FailureIfCharacteristicDoesntExist) {
  CallUpdateCharacteristic(
      /*characteristic_uuid=*/kCharacteristicUuid1,
      /*expected_success=*/false);
  histogram_tester_.ExpectBucketCount(
      "Nearby.Connections.BleV2.GattServer.UpdateCharacteristic.Result",
      /*bucket: failure=*/0, 1);
}

TEST_F(BleV2GattServerTest,
       UpdateCharacteristic_ReadCharacteristicRequest_Success) {
  auto fake_gatt_service = std::make_unique<bluetooth::FakeGattService>();
  fake_gatt_service->SetCreateCharacteristicResult(/*success=*/true);
  auto* fake_gatt_service_ptr = fake_gatt_service.get();
  fake_adapter_->SetCreateLocalGattServiceResult(
      /*gatt_service=*/std::move(fake_gatt_service));
  CallCreateCharacteristic(
      /*characteristic_uuid=*/kCharacteristicUuid1,
      /*expected_success=*/true);

  CallUpdateCharacteristic(
      /*characteristic_uuid=*/kCharacteristicUuid1,
      /*expected_success=*/true);
  histogram_tester_.ExpectBucketCount(
      "Nearby.Connections.BleV2.GattServer.UpdateCharacteristic.Result",
      /*bucket: success=*/1, 1);

  base::test::TestFuture<bluetooth::mojom::LocalCharacteristicReadResultPtr>
      future;
  fake_gatt_service_ptr->TriggerReadCharacteristicRequest(
      device::BluetoothUUID(kServiceId),
      device::BluetoothUUID(kCharacteristicUuid1), future.GetCallback());
  auto read_result = future.Take();
  EXPECT_FALSE(read_result->is_error_code());
  EXPECT_TRUE(read_result->is_data());
  EXPECT_EQ(kNewCharacteristicValue,
            base::as_string_view(
                base::as_chars(base::make_span(read_result->get_data()))));
  histogram_tester_.ExpectBucketCount(
      "Nearby.Connections.BleV2.GattServer.OnLocalCharacteristicRead.Result",
      /*bucket: success=*/1, 1);
}

TEST_F(
    BleV2GattServerTest,
    UpdateCharacteristic_ReadCharacteristicRequest_FailureIfCharacteristicDoesntSupportRead) {
  auto fake_gatt_service = std::make_unique<bluetooth::FakeGattService>();
  fake_gatt_service->SetCreateCharacteristicResult(/*success=*/true);
  auto* fake_gatt_service_ptr = fake_gatt_service.get();
  fake_adapter_->SetCreateLocalGattServiceResult(
      /*gatt_service=*/std::move(fake_gatt_service));
  CallCreateCharacteristic(
      /*characteristic_uuid=*/kCharacteristicUuid1,
      /*expected_success=*/true,
      /*permission=*/api::ble_v2::GattCharacteristic::Permission::kWrite,
      /*property=*/api::ble_v2::GattCharacteristic::Property::kWrite);

  CallUpdateCharacteristic(
      /*characteristic_uuid=*/kCharacteristicUuid1,
      /*expected_success=*/true,
      /*permission=*/api::ble_v2::GattCharacteristic::Permission::kWrite,
      /*property=*/api::ble_v2::GattCharacteristic::Property::kWrite);
  histogram_tester_.ExpectBucketCount(
      "Nearby.Connections.BleV2.GattServer.UpdateCharacteristic.Result",
      /*bucket: success=*/1, 1);

  base::test::TestFuture<bluetooth::mojom::LocalCharacteristicReadResultPtr>
      future;
  fake_gatt_service_ptr->TriggerReadCharacteristicRequest(
      device::BluetoothUUID(kServiceId),
      device::BluetoothUUID(kCharacteristicUuid1), future.GetCallback());
  auto read_result = future.Take();
  EXPECT_TRUE(read_result->is_error_code());
  EXPECT_FALSE(read_result->is_data());
  EXPECT_EQ(device::BluetoothGattService::GattErrorCode::kNotPermitted,
            read_result->get_error_code());
  histogram_tester_.ExpectBucketCount(
      "Nearby.Connections.BleV2.GattServer.OnLocalCharacteristicRead.Result",
      /*bucket: failure=*/0, 1);
}

TEST_F(
    BleV2GattServerTest,
    UpdateCharacteristic_ReadCharacteristicRequest_FailureIfReadRequestForCharacteristicValueDoesntExist) {
  auto fake_gatt_service = std::make_unique<bluetooth::FakeGattService>();
  fake_gatt_service->SetCreateCharacteristicResult(/*success=*/true);
  auto* fake_gatt_service_ptr = fake_gatt_service.get();
  fake_adapter_->SetCreateLocalGattServiceResult(
      /*gatt_service=*/std::move(fake_gatt_service));
  CallCreateCharacteristic(
      /*characteristic_uuid=*/kCharacteristicUuid1,
      /*expected_success=*/true);

  base::test::TestFuture<bluetooth::mojom::LocalCharacteristicReadResultPtr>
      future;
  fake_gatt_service_ptr->TriggerReadCharacteristicRequest(
      device::BluetoothUUID(kServiceId),
      device::BluetoothUUID(kCharacteristicUuid1), future.GetCallback());
  auto read_result = future.Take();
  EXPECT_TRUE(read_result->is_error_code());
  EXPECT_FALSE(read_result->is_data());
  EXPECT_EQ(device::BluetoothGattService::GattErrorCode::kNotSupported,
            read_result->get_error_code());
  histogram_tester_.ExpectBucketCount(
      "Nearby.Connections.BleV2.GattServer.OnLocalCharacteristicRead.Result",
      /*bucket: failure=*/0, 1);
}

TEST_F(BleV2GattServerTest, Stop) {
  auto fake_gatt_service = std::make_unique<bluetooth::FakeGattService>();
  fake_gatt_service->SetCreateCharacteristicResult(/*success=*/true);

  base::RunLoop run_loop;
  bool fake_gatt_service_destroyed = false;
  fake_gatt_service->SetOnDestroyedCallback(base::BindLambdaForTesting([&]() {
    fake_gatt_service_destroyed = true;
    run_loop.Quit();
  }));

  fake_adapter_->SetCreateLocalGattServiceResult(
      /*gatt_service=*/std::move(fake_gatt_service));
  CallCreateCharacteristic(
      /*characteristic_uuid=*/kCharacteristicUuid1,
      /*expected_success=*/true);

  // Expect the underlying objects to have been destroyed.
  ble_v2_gatt_server_->Stop();
  run_loop.Run();
  EXPECT_TRUE(fake_gatt_service_destroyed);
}

TEST_F(BleV2GattServerTest, Register_Success) {
  auto fake_gatt_service = std::make_unique<bluetooth::FakeGattService>();
  fake_gatt_service->SetShouldRegisterSucceed(true);
  fake_gatt_service->SetCreateCharacteristicResult(/*success=*/true);
  fake_adapter_->SetCreateLocalGattServiceResult(
      /*gatt_service=*/std::move(fake_gatt_service));
  CallCreateCharacteristic(
      /*characteristic_uuid=*/kCharacteristicUuid1,
      /*expected_success=*/true);

  base::test::TestFuture<bool> future;
  ble_v2_gatt_server_->RegisterGattServices(future.GetCallback());
  EXPECT_TRUE(future.Take());
  histogram_tester_.ExpectBucketCount(
      "Nearby.Connections.BleV2.GattServer.RegisterGattServices.Result",
      /*bucket: success=*/1, 1);
}

TEST_F(BleV2GattServerTest, Register_Failure) {
  auto fake_gatt_service = std::make_unique<bluetooth::FakeGattService>();
  fake_gatt_service->SetShouldRegisterSucceed(false);
  fake_gatt_service->SetCreateCharacteristicResult(/*success=*/true);
  fake_adapter_->SetCreateLocalGattServiceResult(
      /*gatt_service=*/std::move(fake_gatt_service));
  CallCreateCharacteristic(
      /*characteristic_uuid=*/kCharacteristicUuid1,
      /*expected_success=*/true);

  base::test::TestFuture<bool> future;
  ble_v2_gatt_server_->RegisterGattServices(future.GetCallback());
  EXPECT_FALSE(future.Take());
  histogram_tester_.ExpectBucketCount(
      "Nearby.Connections.BleV2.GattServer.RegisterGattServices.Result",
      /*bucket: failure=*/0, 1);
  histogram_tester_.ExpectBucketCount(
      "Nearby.Connections.BleV2.GattServer.RegisterGattService.FailureReason",
      /*bucket: Failed=*/1, 1);
}

TEST_F(BleV2GattServerTest, MojoGattServiceDisconnect) {
  auto fake_gatt_service = std::make_unique<bluetooth::FakeGattService>();
  fake_gatt_service->SetShouldRegisterSucceed(false);
  fake_gatt_service->SetCreateCharacteristicResult(/*success=*/true);
  fake_adapter_->SetCreateLocalGattServiceResult(
      /*gatt_service=*/std::move(fake_gatt_service));

  base::RunLoop run_loop;
  bool fake_gatt_service_destroyed = false;
  fake_gatt_service_factory_->SetNextFakeGattServiceDestroyedCallback(
      base::BindLambdaForTesting([&]() {
        fake_gatt_service_destroyed = true;
        run_loop.Quit();
      }));

  CallCreateCharacteristic(
      /*characteristic_uuid=*/kCharacteristicUuid1,
      /*expected_success=*/true);

  // Close the Mojo pipe.
  fake_adapter_->gatt_service_receiver_->Close();

  run_loop.Run();
  EXPECT_TRUE(fake_gatt_service_destroyed);
}

TEST_F(
    BleV2GattServerTest,
    UpdateCharacteristic_ReadCharacteristicRequest_Success_PartialBufferReads) {
  auto fake_gatt_service = std::make_unique<bluetooth::FakeGattService>();
  fake_gatt_service->SetCreateCharacteristicResult(/*success=*/true);
  auto* fake_gatt_service_ptr = fake_gatt_service.get();
  fake_adapter_->SetCreateLocalGattServiceResult(
      /*gatt_service=*/std::move(fake_gatt_service));
  CallCreateCharacteristic(
      /*characteristic_uuid=*/kCharacteristicUuid1,
      /*expected_success=*/true);

  CallUpdateCharacteristic(
      /*characteristic_uuid=*/kCharacteristicUuid1,
      /*expected_success=*/true);

  base::test::TestFuture<bluetooth::mojom::LocalCharacteristicReadResultPtr>
      future;
  fake_gatt_service_ptr->TriggerReadCharacteristicRequest(
      device::BluetoothUUID(kServiceId),
      device::BluetoothUUID(kCharacteristicUuid1), future.GetCallback(),
      /*offset=*/kPartialBufferOffset);
  auto read_result = future.Take();
  EXPECT_FALSE(read_result->is_error_code());
  EXPECT_TRUE(read_result->is_data());
  EXPECT_EQ(kNewCharacteristicValue.substr(kPartialBufferOffset),
            base::as_string_view(
                base::as_chars(base::make_span(read_result->get_data()))));
}

TEST_F(BleV2GattServerTest,
       UpdateCharacteristic_ReadCharacteristicRequest_FailureIfInvalidOffset) {
  auto fake_gatt_service = std::make_unique<bluetooth::FakeGattService>();
  fake_gatt_service->SetCreateCharacteristicResult(/*success=*/true);
  auto* fake_gatt_service_ptr = fake_gatt_service.get();
  fake_adapter_->SetCreateLocalGattServiceResult(
      /*gatt_service=*/std::move(fake_gatt_service));
  CallCreateCharacteristic(
      /*characteristic_uuid=*/kCharacteristicUuid1,
      /*expected_success=*/true);

  CallUpdateCharacteristic(
      /*characteristic_uuid=*/kCharacteristicUuid1,
      /*expected_success=*/true);

  base::test::TestFuture<bluetooth::mojom::LocalCharacteristicReadResultPtr>
      future;
  fake_gatt_service_ptr->TriggerReadCharacteristicRequest(
      device::BluetoothUUID(kServiceId),
      device::BluetoothUUID(kCharacteristicUuid1), future.GetCallback(),
      /*offset=*/10);
  auto read_result = future.Take();
  EXPECT_TRUE(read_result->is_error_code());
  EXPECT_FALSE(read_result->is_data());
  EXPECT_EQ(device::BluetoothGattService::GattErrorCode::kInvalidLength,
            read_result->get_error_code());
}

}  // namespace nearby::chrome
