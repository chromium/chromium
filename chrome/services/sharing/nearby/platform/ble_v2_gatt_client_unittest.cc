// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/ble_v2_gatt_client.h"

#include "base/memory/raw_ptr.h"
#include "base/task/thread_pool.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/services/sharing/nearby/test_support/fake_device.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/nearby/src/internal/platform/uuid.h"

namespace {

const nearby::Uuid kServiceUuid1 = nearby::Uuid("0000");
const nearby::Uuid kServiceUuid2 = nearby::Uuid("1111");
const nearby::Uuid kCharacteristicUuid1 = nearby::Uuid("2222");
const nearby::Uuid kCharacteristicUuid2 = nearby::Uuid("3333");
const std::vector<uint8_t> kReadCharacteristicValue = {0x01, 0x02, 0x03, 0x04,
                                                       0x05};

std::vector<bluetooth::mojom::ServiceInfoPtr> GenerateServiceInfo(
    nearby::Uuid service_uuid) {
  std::vector<bluetooth::mojom::ServiceInfoPtr> service_infos;
  bluetooth::mojom::ServiceInfoPtr service_info =
      bluetooth::mojom::ServiceInfo::New();
  service_info->uuid = device::BluetoothUUID(std::string(service_uuid));
  service_infos.push_back(std::move(service_info));
  return service_infos;
}

std::vector<bluetooth::mojom::CharacteristicInfoPtr> GenerateCharacteristicInfo(
    nearby::Uuid service_uuid) {
  std::vector<bluetooth::mojom::CharacteristicInfoPtr> characteristic_infos;
  bluetooth::mojom::CharacteristicInfoPtr characteristic_info =
      bluetooth::mojom::CharacteristicInfo::New();
  characteristic_info->uuid = device::BluetoothUUID(std::string(service_uuid));
  characteristic_infos.push_back(std::move(characteristic_info));
  return characteristic_infos;
}

}  // namespace

namespace nearby::chrome {

class BleV2GattClientTest : public testing::Test {
 public:
  BleV2GattClientTest() = default;
  ~BleV2GattClientTest() override = default;
  BleV2GattClientTest(const BleV2GattClientTest&) = delete;
  BleV2GattClientTest& operator=(const BleV2GattClientTest&) = delete;

  void SetUp() override {
    auto fake_device = std::make_unique<bluetooth::FakeDevice>();
    fake_device_ = fake_device.get();
    mojo::PendingRemote<bluetooth::mojom::Device> pending_device;
    mojo::MakeSelfOwnedReceiver(
        std::move(fake_device),
        pending_device.InitWithNewPipeAndPassReceiver());
    ble_v2_gatt_client_ =
        std::make_unique<BleV2GattClient>(std::move(pending_device));
  }

  void TearDown() override {
    base::RunLoop run_loop;
    fake_device_->set_on_disconnected_callback(run_loop.QuitClosure());
    ble_v2_gatt_client_->Disconnect();
    run_loop.Run();

    // Need to reset `fake_device_` since it gets deleted on disconnect to avoid
    // dangling raw_ptr.
    fake_device_ = nullptr;
  }

  void CallDiscoverServiceAndCharacteristics(
      bool expected_success,
      const Uuid& service_uuid,
      const std::vector<Uuid>& characteristic_uuids) {
    EXPECT_EQ(expected_success,
              ble_v2_gatt_client_->DiscoverServiceAndCharacteristics(
                  service_uuid, characteristic_uuids));
  }

  void CallReadCharacteristic(bool expected_success,
                              const Uuid& service_uuid,
                              const Uuid& characteristic_uuid) {
    api::ble_v2::GattCharacteristic gatt_characteristic = {
        characteristic_uuid, service_uuid,
        nearby::api::ble_v2::GattCharacteristic::Permission::kRead,
        nearby::api::ble_v2::GattCharacteristic::Property::kRead};
    EXPECT_EQ(expected_success,
              ble_v2_gatt_client_->ReadCharacteristic(gatt_characteristic)
                  .has_value());
  }

  void SuccessfullyDiscoverServiceAndCharacteristics(
      const Uuid& service_uuid,
      const Uuid& characteristic_uuid) {
    fake_device_->set_services(GenerateServiceInfo(service_uuid));
    fake_device_->set_characteristics(
        GenerateCharacteristicInfo(characteristic_uuid));
    std::vector<Uuid> characteristic_uuids = {characteristic_uuid};
    base::RunLoop run_loop;
    base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})
        ->PostTaskAndReply(
            FROM_HERE,
            base::BindOnce(
                &BleV2GattClientTest::CallDiscoverServiceAndCharacteristics,
                base::Unretained(this), /*expected_success=*/true, service_uuid,
                /*characteristic_uuids=*/characteristic_uuids),
            run_loop.QuitClosure());
    run_loop.Run();
    EXPECT_TRUE(ble_v2_gatt_client_
                    ->GetCharacteristic(service_uuid, characteristic_uuid)
                    .has_value());
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<BleV2GattClient> ble_v2_gatt_client_;
  raw_ptr<bluetooth::FakeDevice> fake_device_;
};

TEST_F(BleV2GattClientTest, DiscoverServiceAndCharacteristics_Success) {
  SuccessfullyDiscoverServiceAndCharacteristics(kServiceUuid1,
                                                kCharacteristicUuid1);
}

TEST_F(BleV2GattClientTest,
       DiscoverServiceAndCharacteristics_FailureIfNoServiceUuidMatch) {
  fake_device_->set_services(GenerateServiceInfo(kServiceUuid2));
  std::vector<Uuid> characteristic_uuids;

  base::RunLoop run_loop;
  base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})
      ->PostTaskAndReply(
          FROM_HERE,
          base::BindOnce(
              &BleV2GattClientTest::CallDiscoverServiceAndCharacteristics,
              base::Unretained(this), /*expected_success=*/false, kServiceUuid1,
              /*characteristic_uuids=*/characteristic_uuids),
          run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_FALSE(ble_v2_gatt_client_
                   ->GetCharacteristic(kServiceUuid1, kCharacteristicUuid1)
                   .has_value());
}

TEST_F(BleV2GattClientTest,
       DiscoverServiceAndCharacteristics_FailureIfGetCharacteristicError) {
  fake_device_->set_services(GenerateServiceInfo(kServiceUuid1));
  fake_device_->set_characteristics(std::nullopt);

  std::vector<Uuid> characteristic_uuids = {kCharacteristicUuid1};

  base::RunLoop run_loop;
  base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})
      ->PostTaskAndReply(
          FROM_HERE,
          base::BindOnce(
              &BleV2GattClientTest::CallDiscoverServiceAndCharacteristics,
              base::Unretained(this), /*expected_success=*/false, kServiceUuid1,
              /*characteristic_uuids=*/characteristic_uuids),
          run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_FALSE(ble_v2_gatt_client_
                   ->GetCharacteristic(kServiceUuid1, kCharacteristicUuid1)
                   .has_value());
}

TEST_F(BleV2GattClientTest,
       DiscoverServiceAndCharacteristics_FailureIfNoCharacteristicUuidMatch) {
  fake_device_->set_services(GenerateServiceInfo(kServiceUuid1));
  fake_device_->set_characteristics(
      GenerateCharacteristicInfo(kCharacteristicUuid2));

  std::vector<Uuid> characteristic_uuids = {kCharacteristicUuid1};

  base::RunLoop run_loop;
  base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})
      ->PostTaskAndReply(
          FROM_HERE,
          base::BindOnce(
              &BleV2GattClientTest::CallDiscoverServiceAndCharacteristics,
              base::Unretained(this), /*expected_success=*/false, kServiceUuid1,
              /*characteristic_uuids=*/characteristic_uuids),
          run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_FALSE(ble_v2_gatt_client_
                   ->GetCharacteristic(kServiceUuid1, kCharacteristicUuid1)
                   .has_value());
}

TEST_F(BleV2GattClientTest, ReadCharacteristic_Success) {
  SuccessfullyDiscoverServiceAndCharacteristics(kServiceUuid1,
                                                kCharacteristicUuid1);
  fake_device_->set_read_value_for_characteristic_response(
      bluetooth::mojom::GattResult::SUCCESS, kReadCharacteristicValue);

  base::RunLoop run_loop;
  base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})
      ->PostTaskAndReply(
          FROM_HERE,
          base::BindOnce(&BleV2GattClientTest::CallReadCharacteristic,
                         base::Unretained(this), /*expected_success=*/true,
                         kServiceUuid1, kCharacteristicUuid1),
          run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(BleV2GattClientTest,
       ReadCharacteristic_FailureIfNoServiceUuidDiscovered) {
  SuccessfullyDiscoverServiceAndCharacteristics(kServiceUuid1,
                                                kCharacteristicUuid1);

  base::RunLoop run_loop;
  base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})
      ->PostTaskAndReply(
          FROM_HERE,
          base::BindOnce(&BleV2GattClientTest::CallReadCharacteristic,
                         base::Unretained(this), /*expected_success=*/false,
                         kServiceUuid2, kCharacteristicUuid1),
          run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(BleV2GattClientTest,
       ReadCharacteristic_FailureIfNoCharacteristicUuidDiscovered) {
  SuccessfullyDiscoverServiceAndCharacteristics(kServiceUuid1,
                                                kCharacteristicUuid1);

  base::RunLoop run_loop;
  base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})
      ->PostTaskAndReply(
          FROM_HERE,
          base::BindOnce(&BleV2GattClientTest::CallReadCharacteristic,
                         base::Unretained(this), /*expected_success=*/false,
                         kServiceUuid1, kCharacteristicUuid2),
          run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(BleV2GattClientTest,
       ReadCharacteristic_FailureIfReadValueForCharacteristicFails) {
  SuccessfullyDiscoverServiceAndCharacteristics(kServiceUuid1,
                                                kCharacteristicUuid1);
  fake_device_->set_read_value_for_characteristic_response(
      bluetooth::mojom::GattResult::NOT_PAIRED, std::nullopt);

  base::RunLoop run_loop;
  base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})
      ->PostTaskAndReply(
          FROM_HERE,
          base::BindOnce(&BleV2GattClientTest::CallReadCharacteristic,
                         base::Unretained(this), /*expected_success=*/false,
                         kServiceUuid1, kCharacteristicUuid1),
          run_loop.QuitClosure());
  run_loop.Run();
}

}  // namespace nearby::chrome
