// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "chrome/services/sharing/nearby/platform/ble_v2_medium.h"
#include "chrome/services/sharing/nearby/platform/count_down_latch.h"
#include "chrome/services/sharing/nearby/test_support/fake_adapter.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nearby::chrome {

namespace {

const char kDeviceAddress[] = "DeviceAddress";
const char kDeviceServiceData1Str[] = "Device_Advertisement1";
const Uuid kFastAdvertisementServiceUuid1{0x0000FEF300001000,
                                          0x800000805F9B34FB};
const ByteArray kDeviceServiceData1ByteArray{
    std::string{kDeviceServiceData1Str}};
const device::BluetoothUUID kService1BluetoothUuid{
    base::span<const uint8_t>(reinterpret_cast<const uint8_t*>(
                                  kFastAdvertisementServiceUuid1.data().data()),
                              kFastAdvertisementServiceUuid1.data().size())};
const device::BluetoothUUID kService2BluetoothUuid("1000");

std::vector<uint8_t> GetByteVector(const std::string& str) {
  return std::vector<uint8_t>(str.begin(), str.end());
}

}  // namespace

class BleV2MediumTest : public testing::Test {
 public:
  BleV2MediumTest() = default;
  ~BleV2MediumTest() override = default;
  BleV2MediumTest(const BleV2MediumTest&) = delete;
  BleV2MediumTest& operator=(const BleV2MediumTest&) = delete;

  void SetUp() override {
    auto fake_adapter = std::make_unique<bluetooth::FakeAdapter>();
    fake_adapter_ = fake_adapter.get();

    mojo::PendingRemote<bluetooth::mojom::Adapter> pending_adapter;

    mojo::MakeSelfOwnedReceiver(
        std::move(fake_adapter),
        pending_adapter.InitWithNewPipeAndPassReceiver());

    remote_adapter_.Bind(std::move(pending_adapter),
                         /*bind_task_runner=*/nullptr);

    ble_v2_medium_ = std::make_unique<BleV2Medium>(remote_adapter_);
  }

  void OnPeripheralDiscovered() {
    if (on_expected_peripherals_discovered_callback_) {
      std::move(on_expected_peripherals_discovered_callback_).Run();
    }
  }

  void SetOnExpectedPeripheralsDiscoveredCallback(base::OnceClosure callback) {
    on_expected_peripherals_discovered_callback_ = std::move(callback);
  }

  bluetooth::mojom::DeviceInfoPtr CreateDeviceInfo(
      const std::string& address,
      const base::flat_map<device::BluetoothUUID, std::vector<uint8_t>>&
          service_data_map) {
    // Do not set |name|. This reflects Chrome's usual representation of a BLE
    // advertisement.
    auto device_info = bluetooth::mojom::DeviceInfo::New();
    device_info->address = address;
    device_info->name_for_display = address;
    device_info->service_data_map = service_data_map;
    for (auto pair : service_data_map) {
      device_info->service_uuids.push_back(pair.first);
    }

    return device_info;
  }

  raw_ptr<bluetooth::FakeAdapter, ExperimentalAsh> fake_adapter_;
  mojo::SharedRemote<bluetooth::mojom::Adapter> remote_adapter_;
  std::unique_ptr<BleV2Medium> ble_v2_medium_;

 private:
  std::unique_ptr<BleV2Medium::ScanningSession> scanning_session_;
  base::OnceClosure on_expected_peripherals_discovered_callback_;
  base::OnceClosure on_discovery_session_destroyed_callback_;
  base::test::TaskEnvironment task_environment_;
};

TEST_F(BleV2MediumTest, TestScanning_OneService) {
  CountDownLatch scanning_started_latch(1);
  CountDownLatch found_advertisement_latch(1);
  api::ble_v2::BleMedium::ScanningCallback scanning_callback = {
      .start_scanning_result =
          [&scanning_started_latch](absl::Status status) {
            scanning_started_latch.CountDown();
          },
      .advertisement_found_cb =
          [this, &found_advertisement_latch](
              api::ble_v2::BlePeripheral& peripheral,
              const api::ble_v2::BleAdvertisementData& advertisement_data) {
            EXPECT_EQ(peripheral.GetAddress(), kDeviceAddress);
            EXPECT_EQ(advertisement_data.service_data
                          .find(kFastAdvertisementServiceUuid1)
                          ->second,
                      kDeviceServiceData1ByteArray);
            found_advertisement_latch.CountDown();
            OnPeripheralDiscovered();
          }};

  auto scanning_session = ble_v2_medium_->StartScanning(
      kFastAdvertisementServiceUuid1, {}, std::move(scanning_callback));
  EXPECT_NE(scanning_session, nullptr);

  base::flat_map<device::BluetoothUUID, std::vector<uint8_t>> service_data_map;
  service_data_map.insert_or_assign(kService1BluetoothUuid,
                                    GetByteVector(kDeviceServiceData1Str));

  EXPECT_TRUE(scanning_started_latch.Await().Ok());

  base::RunLoop run_loop;
  SetOnExpectedPeripheralsDiscoveredCallback(run_loop.QuitClosure());
  fake_adapter_->NotifyDeviceAdded(
      CreateDeviceInfo(kDeviceAddress, service_data_map));
  run_loop.Run();

  EXPECT_TRUE(found_advertisement_latch.Await().Ok());
  EXPECT_TRUE(scanning_session->stop_scanning().ok());
}

TEST_F(BleV2MediumTest, TestScanning_MultipleSessions) {
  // Expects session 1 found one advertisement.
  CountDownLatch session_1_found_advertisement_latch(1);
  // Expects session 2 found two advertisement.
  CountDownLatch session_2_found_advertisement_latch(2);
  // Expects both sessions succeeded to start scanning.
  CountDownLatch scanning_started_latch(2);

  api::ble_v2::BleMedium::ScanningCallback scanning_callback_1 = {
      .start_scanning_result =
          [&scanning_started_latch](absl::Status status) {
            scanning_started_latch.CountDown();
          },
      .advertisement_found_cb =
          [this, &session_1_found_advertisement_latch](
              api::ble_v2::BlePeripheral& peripheral,
              const api::ble_v2::BleAdvertisementData& advertisement_data) {
            session_1_found_advertisement_latch.CountDown();
            OnPeripheralDiscovered();
          }};
  api::ble_v2::BleMedium::ScanningCallback scanning_callback_2 = {
      .start_scanning_result =
          [&scanning_started_latch](absl::Status status) {
            scanning_started_latch.CountDown();
          },
      .advertisement_found_cb =
          [this, &session_2_found_advertisement_latch](
              api::ble_v2::BlePeripheral& peripheral,
              const api::ble_v2::BleAdvertisementData& advertisement_data) {
            session_2_found_advertisement_latch.CountDown();
            OnPeripheralDiscovered();
          }};

  auto scanning_session_1 = ble_v2_medium_->StartScanning(
      kFastAdvertisementServiceUuid1, {}, std::move(scanning_callback_1));
  auto scanning_session_2 = ble_v2_medium_->StartScanning(
      kFastAdvertisementServiceUuid1, {}, std::move(scanning_callback_2));
  // Verify both sessions start successfully.
  EXPECT_NE(scanning_session_1, nullptr);
  EXPECT_NE(scanning_session_2, nullptr);
  EXPECT_TRUE(scanning_started_latch.Await().Ok());

  base::flat_map<device::BluetoothUUID, std::vector<uint8_t>> service_data_map;
  service_data_map.insert_or_assign(kService1BluetoothUuid,
                                    GetByteVector(kDeviceServiceData1Str));
  base::RunLoop run_loop;
  SetOnExpectedPeripheralsDiscoveredCallback(run_loop.QuitClosure());
  // Both session should see the advertisement.
  fake_adapter_->NotifyDeviceAdded(
      CreateDeviceInfo(kDeviceAddress, service_data_map));
  run_loop.Run();

  // Verify session 1 did see one advertisement and stop successfully.
  EXPECT_TRUE(session_1_found_advertisement_latch.Await().Ok());
  EXPECT_TRUE(scanning_session_1->stop_scanning().ok());

  base::RunLoop run_loop_2;
  SetOnExpectedPeripheralsDiscoveredCallback(run_loop_2.QuitClosure());
  // Only session 2 should see the advertisement.
  fake_adapter_->NotifyDeviceAdded(
      CreateDeviceInfo(kDeviceAddress, service_data_map));
  run_loop_2.Run();

  // Verify session 2 did see two advertisements and stop successfully.
  EXPECT_TRUE(session_2_found_advertisement_latch.Await().Ok());
  EXPECT_TRUE(scanning_session_2->stop_scanning().ok());
}

TEST_F(BleV2MediumTest, TestScanning_IgnoreIrrelevantAdvertisement) {
  CountDownLatch scanning_started_latch(1);
  api::ble_v2::BleMedium::ScanningCallback scanning_callback = {
      .start_scanning_result =
          [&scanning_started_latch](absl::Status status) {
            scanning_started_latch.CountDown();
          },
      .advertisement_found_cb =
          [](api::ble_v2::BlePeripheral& peripheral,
             const api::ble_v2::BleAdvertisementData& advertisement_data) {
            // should not reached here for irrelavant advertisement.
            EXPECT_TRUE(false);
          }};

  auto scanning_session = ble_v2_medium_->StartScanning(
      kFastAdvertisementServiceUuid1, {}, std::move(scanning_callback));
  EXPECT_NE(scanning_session, nullptr);

  base::flat_map<device::BluetoothUUID, std::vector<uint8_t>> service_data_map;
  // Scan for kService1BluetoothUuid but notify with kService2BluetoothUuid.
  service_data_map.insert_or_assign(kService2BluetoothUuid,
                                    GetByteVector(kDeviceServiceData1Str));

  EXPECT_TRUE(scanning_started_latch.Await().Ok());

  base::RunLoop run_loop;
  fake_adapter_->NotifyDeviceAdded(
      CreateDeviceInfo(kDeviceAddress, service_data_map));
  fake_adapter_->SetDiscoverySessionDestroyedCallback(run_loop.QuitClosure());

  EXPECT_TRUE(scanning_session->stop_scanning().ok());
  run_loop.Run();
}

}  // namespace nearby::chrome
