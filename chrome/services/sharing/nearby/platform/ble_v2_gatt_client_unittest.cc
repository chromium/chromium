// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/ble_v2_gatt_client.h"

#include "base/memory/raw_ptr.h"
#include "base/task/thread_pool.h"
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
    fake_device_ = nullptr;

    ble_v2_gatt_client_->Disconnect();

    // TODO(b/316395226): Rework to avoid RunUntilIdle().
    base::RunLoop().RunUntilIdle();
  }

  void CallDiscoverServiceAndCharacteristics(
      bool expected_success,
      const Uuid& service_uuid,
      const std::vector<Uuid>& characteristic_uuids) {
    EXPECT_EQ(expected_success,
              ble_v2_gatt_client_->DiscoverServiceAndCharacteristics(
                  service_uuid, characteristic_uuids));
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<BleV2GattClient> ble_v2_gatt_client_;
  raw_ptr<bluetooth::FakeDevice> fake_device_;
};

TEST_F(BleV2GattClientTest, DiscoverServiceAndCharacteristics_Success) {
  fake_device_->set_services(GenerateServiceInfo(kServiceUuid1));
  fake_device_->set_characteristics(
      GenerateCharacteristicInfo(kCharacteristicUuid1));

  std::vector<Uuid> characteristic_uuids = {kCharacteristicUuid1};

  base::RunLoop run_loop;
  base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})
      ->PostTaskAndReply(
          FROM_HERE,
          base::BindOnce(
              &BleV2GattClientTest::CallDiscoverServiceAndCharacteristics,
              base::Unretained(this), /*expected_success=*/true, kServiceUuid1,
              /*characteristic_uuids=*/characteristic_uuids),
          run_loop.QuitClosure());
  run_loop.Run();

  auto characteristic = ble_v2_gatt_client_->GetCharacteristic(
      kServiceUuid1, kCharacteristicUuid1);
  EXPECT_TRUE(characteristic.has_value());
  EXPECT_EQ(kServiceUuid1, characteristic->service_uuid);
  EXPECT_EQ(kCharacteristicUuid1, characteristic->uuid);
  EXPECT_EQ(nearby::api::ble_v2::GattCharacteristic::Permission::kNone,
            characteristic->permission);
  EXPECT_EQ(nearby::api::ble_v2::GattCharacteristic::Property::kNone,
            characteristic->property);
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

}  // namespace nearby::chrome
