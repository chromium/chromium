// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/ble_v2_medium.h"

#include <memory>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/task/thread_pool.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/services/sharing/nearby/platform/count_down_latch.h"
#include "chrome/services/sharing/nearby/platform/nearby_platform_metrics.h"
#include "chrome/services/sharing/nearby/test_support/fake_adapter.h"
#include "chrome/services/sharing/nearby/test_support/fake_device.h"
#include "components/cross_device/nearby/nearby_features.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nearby::chrome {

namespace {

const char kDeviceAddress[] = "DeviceAddress";
const char kDeviceServiceData1Str[] = "Device_Advertisement1";
const char kDeviceServiceData2Str[] = "Device_Advertisement2";
const ByteArray kDeviceServiceData1ByteArray{
    std::string{kDeviceServiceData1Str}};
const ByteArray kDeviceServiceData2ByteArray{
    std::string{kDeviceServiceData2Str}};
const Uuid kFastAdvertisementServiceUuid1{0x0000FEF300001000,
                                          0x800000805F9B34FB};
const Uuid kTestServiceUuid2{0x0000FEF300001000, 0xA0000060ABCDEF12};
const device::BluetoothUUID kService1BluetoothUuid{
    base::span<const uint8_t>(reinterpret_cast<const uint8_t*>(
                                  kFastAdvertisementServiceUuid1.data().data()),
                              kFastAdvertisementServiceUuid1.data().size())};
const device::BluetoothUUID kService2BluetoothUuid{base::span<const uint8_t>(
    reinterpret_cast<const uint8_t*>(kTestServiceUuid2.data().data()),
    kTestServiceUuid2.data().size())};
const char kServiceId[] = "TestServiceId";
const char kCharacteristicUuid[] = "1234";
const uint64_t kUniqueId = 1053256082272529;

std::vector<uint8_t> GetByteVector(const std::string& str) {
  return std::vector<uint8_t>(str.begin(), str.end());
}

class FakeBleV2RemotePeripheral : public api::ble_v2::BlePeripheral {
 public:
  FakeBleV2RemotePeripheral() = default;
  ~FakeBleV2RemotePeripheral() override = default;

  std::string GetAddress() const override { return kDeviceAddress; }

  UniqueId GetUniqueId() const override { return kUniqueId; }
};

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
    fake_adapter_->SetExtendedAdvertisementSupport(true);

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

    // TODO(b/285637726): Once the bug is resolved, test whether the
    // service_uuids are being populated correctly.
    for (auto pair : service_data_map) {
      device_info->service_uuids.push_back(pair.first);
    }

    return device_info;
  }

  void SetUpGattServerForAdvertising(bool should_register_succeed) {
    base::ScopedAllowBaseSyncPrimitivesForTesting allow_sync_primitives;
    fake_adapter_->is_dual_role_supported_ = true;
    gatt_server_ = ble_v2_medium_->StartGattServer({});
    EXPECT_TRUE(gatt_server_);

    // Set up a `GattService` that will succeed/fail registration when
    // `RegisterGattServices()` is called by `BleV2Medium`.
    auto fake_gatt_service = std::make_unique<bluetooth::FakeGattService>();
    fake_gatt_service->SetShouldRegisterSucceed(should_register_succeed);
    fake_gatt_service->SetCreateCharacteristicResult(/*success=*/true);
    fake_adapter_->SetCreateLocalGattServiceResult(
        /*gatt_service=*/std::move(fake_gatt_service));
    auto gatt_characteristic = gatt_server_->CreateCharacteristic(
        /*service_uuid=*/Uuid(/*data=*/kServiceId),
        /*characteristic_uuid=*/Uuid(/*data=*/kCharacteristicUuid),
        /*permission=*/api::ble_v2::GattCharacteristic::Permission::kRead,
        /*property=*/api::ble_v2::GattCharacteristic::Property::kRead);
    EXPECT_TRUE(gatt_characteristic);
  }

  void CallStartAdvertisingForGattService(bool expected_success) {
    api::ble_v2::BleAdvertisementData advertising_data;
    advertising_data.is_extended_advertisement = true;
    advertising_data.service_data.insert(
        {kFastAdvertisementServiceUuid1, kDeviceServiceData1ByteArray});

    base::ScopedAllowBaseSyncPrimitivesForTesting allow_sync_primitives;
    EXPECT_EQ(expected_success,
              ble_v2_medium_->StartAdvertising(
                  advertising_data,
                  {.tx_power_level = api::ble_v2::TxPowerLevel::kHigh,
                   .is_connectable = true}));
  }

  void CallConnectToGattServer(bool expected_success) {
    base::ScopedAllowBaseSyncPrimitivesForTesting allow_sync_primitives;
    FakeBleV2RemotePeripheral peripheral;
    auto gatt_client = ble_v2_medium_->ConnectToGattServer(
        peripheral, api::ble_v2::TxPowerLevel::kHigh,
        /*callback=*/{});
    EXPECT_EQ(expected_success, (gatt_client != nullptr));
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;
  mojo::SharedRemote<bluetooth::mojom::Adapter> remote_adapter_;
  std::unique_ptr<api::ble_v2::GattServer> gatt_server_;
  std::unique_ptr<BleV2Medium::ScanningSession> scanning_session_;
  base::OnceClosure on_expected_peripherals_discovered_callback_;
  base::OnceClosure on_discovery_session_destroyed_callback_;
  raw_ptr<bluetooth::FakeAdapter> fake_adapter_;
  std::unique_ptr<BleV2Medium> ble_v2_medium_;
};

TEST_F(BleV2MediumTest, TestScanning_OneService) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{::features::kEnableNearbyBleV2},
      /*disabled_features=*/{});

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

            EXPECT_TRUE(ble_v2_medium_->GetRemotePeripheral(
                peripheral.GetUniqueId(),
                [&](api::ble_v2::BlePeripheral& device) {
                  EXPECT_EQ(kDeviceAddress, device.GetAddress());
                }));
          }};

  auto scanning_session = ble_v2_medium_->StartScanning(
      kFastAdvertisementServiceUuid1, {}, std::move(scanning_callback));
  EXPECT_NE(scanning_session, nullptr);

  base::flat_map<device::BluetoothUUID, std::vector<uint8_t>> service_data_map;
  service_data_map.insert_or_assign(kService1BluetoothUuid,
                                    GetByteVector(kDeviceServiceData1Str));

  EXPECT_TRUE(scanning_started_latch.Await().Ok());
  histogram_tester_.ExpectBucketCount(
      "Nearby.Connections.BleV2.StartScanning.Result",
      /*bucket: Success=*/1, 1);

  base::RunLoop run_loop;
  SetOnExpectedPeripheralsDiscoveredCallback(run_loop.QuitClosure());
  fake_adapter_->NotifyDeviceAdded(
      CreateDeviceInfo(kDeviceAddress, service_data_map));
  run_loop.Run();

  EXPECT_TRUE(found_advertisement_latch.Await().Ok());
  EXPECT_TRUE(scanning_session->stop_scanning().ok());
}

TEST_F(BleV2MediumTest, TestScanning_MultipleSessions) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{::features::kEnableNearbyBleV2},
      /*disabled_features=*/{});

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

            EXPECT_TRUE(ble_v2_medium_->GetRemotePeripheral(
                peripheral.GetUniqueId(),
                [&](api::ble_v2::BlePeripheral& device) {
                  EXPECT_EQ(kDeviceAddress, device.GetAddress());
                }));
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

            EXPECT_TRUE(ble_v2_medium_->GetRemotePeripheral(
                peripheral.GetUniqueId(),
                [&](api::ble_v2::BlePeripheral& device) {
                  EXPECT_EQ(kDeviceAddress, device.GetAddress());
                }));
          }};

  auto scanning_session_1 = ble_v2_medium_->StartScanning(
      kFastAdvertisementServiceUuid1, {}, std::move(scanning_callback_1));
  auto scanning_session_2 = ble_v2_medium_->StartScanning(
      kFastAdvertisementServiceUuid1, {}, std::move(scanning_callback_2));
  // Verify both sessions start successfully.
  EXPECT_NE(scanning_session_1, nullptr);
  EXPECT_NE(scanning_session_2, nullptr);
  EXPECT_TRUE(scanning_started_latch.Await().Ok());
  histogram_tester_.ExpectBucketCount(
      "Nearby.Connections.BleV2.StartScanning.Result",
      /*bucket: Success=*/1, 2);

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
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{::features::kEnableNearbyBleV2},
      /*disabled_features=*/{});

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
  histogram_tester_.ExpectBucketCount(
      "Nearby.Connections.BleV2.StartScanning.Result",
      /*bucket: Success=*/1, 1);

  base::RunLoop run_loop;
  fake_adapter_->NotifyDeviceAdded(
      CreateDeviceInfo(kDeviceAddress, service_data_map));
  fake_adapter_->SetDiscoverySessionDestroyedCallback(run_loop.QuitClosure());

  EXPECT_TRUE(scanning_session->stop_scanning().ok());
  run_loop.Run();
}

TEST_F(BleV2MediumTest, TestAdvertising_AdapterFails) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{::features::kEnableNearbyBleV2},
      /*disabled_features=*/{});

  fake_adapter_->SetShouldAdvertisementRegistrationSucceed(false);
  api::ble_v2::BleAdvertisementData advertising_data;
  advertising_data.is_extended_advertisement = false;
  advertising_data.service_data.insert(
      {kFastAdvertisementServiceUuid1, kDeviceServiceData1ByteArray});
  EXPECT_FALSE(ble_v2_medium_->StartAdvertising(
      advertising_data, {.tx_power_level = api::ble_v2::TxPowerLevel::kLow,
                         .is_connectable = true}));
  histogram_tester_.ExpectBucketCount(
      "Nearby.Connections.BleV2.StartAdvertising.Result",
      /*bucket: Failure=*/0, 1);
  histogram_tester_.ExpectBucketCount(
      "Nearby.Connections.BleV2.StartAdvertising.Result.RegularAdvertisement",
      /*bucket: Failure=*/0, 1);
  histogram_tester_.ExpectBucketCount(
      "Nearby.Connections.BleV2.StartAdvertising.FailureReason",
      metrics::StartAdvertisingFailureReason::
          kAdapterRegisterAdvertisementFailed,
      1);
  histogram_tester_.ExpectBucketCount(
      "Nearby.Connections.BleV2.StartAdvertising.FailureReason."
      "RegularAdvertisement",
      metrics::StartAdvertisingFailureReason::
          kAdapterRegisterAdvertisementFailed,
      1);
}

TEST_F(BleV2MediumTest, TestAdvertising_AdapterFailsInAsyncStartAdvertising) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{::features::kEnableNearbyBleV2},
      /*disabled_features=*/{});

  fake_adapter_->SetShouldAdvertisementRegistrationSucceed(false);
  api::ble_v2::BleAdvertisementData advertising_data;
  advertising_data.is_extended_advertisement = false;
  advertising_data.service_data.insert(
      {kFastAdvertisementServiceUuid1, kDeviceServiceData1ByteArray});
  auto advertising_session = ble_v2_medium_->StartAdvertising(
      advertising_data,
      {.tx_power_level = api::ble_v2::TxPowerLevel::kLow,
       .is_connectable = true},
      api::ble_v2::BleMedium::AdvertisingCallback{
          .start_advertising_result =
              [this](absl::Status status) {
                EXPECT_FALSE(status.ok());
                histogram_tester_.ExpectBucketCount(
                    "Nearby.Connections.BleV2.StartAdvertising.Result",
                    /*bucket: Failure=*/0, 1);
                histogram_tester_.ExpectBucketCount(
                    "Nearby.Connections.BleV2.StartAdvertising.Result."
                    "RegularAdvertisement",
                    /*bucket: Failure=*/0, 1);
                histogram_tester_.ExpectBucketCount(
                    "Nearby.Connections.BleV2.StartAdvertising.FailureReason",
                    metrics::StartAdvertisingFailureReason::
                        kAdapterRegisterAdvertisementFailed,
                    1);
                histogram_tester_.ExpectBucketCount(
                    "Nearby.Connections.BleV2.StartAdvertising.FailureReason."
                    "RegularAdvertisement",
                    metrics::StartAdvertisingFailureReason::
                        kAdapterRegisterAdvertisementFailed,
                    1);
              },
      });
  EXPECT_EQ(advertising_session, nullptr);
}

TEST_F(BleV2MediumTest, TestAdvertising_FastAdvertisementSuccess) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{::features::kEnableNearbyBleV2},
      /*disabled_features=*/{});

  fake_adapter_->SetShouldAdvertisementRegistrationSucceed(true);
  api::ble_v2::BleAdvertisementData advertising_data;
  advertising_data.is_extended_advertisement = false;
  advertising_data.service_data.insert(
      {kFastAdvertisementServiceUuid1, kDeviceServiceData1ByteArray});
  EXPECT_TRUE(ble_v2_medium_->StartAdvertising(
      advertising_data, {.tx_power_level = api::ble_v2::TxPowerLevel::kLow,
                         .is_connectable = true}));
  histogram_tester_.ExpectBucketCount(
      "Nearby.Connections.BleV2.StartAdvertising.Result",
      /*bucket: Success=*/1, 1);
  histogram_tester_.ExpectBucketCount(
      "Nearby.Connections.BleV2.StartAdvertising.Result.RegularAdvertisement",
      /*bucket: Success=*/1, 1);
}

TEST_F(BleV2MediumTest, TestAdvertising_ExtendedAdvertisementNotSupported) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{::features::kEnableNearbyBleV2},
      /*disabled_features=*/{
          ::features::kEnableNearbyBleV2ExtendedAdvertising});
  EXPECT_FALSE(ble_v2_medium_->IsExtendedAdvertisementsAvailable());

  fake_adapter_->SetShouldAdvertisementRegistrationSucceed(true);
  api::ble_v2::BleAdvertisementData advertising_data;
  advertising_data.is_extended_advertisement = true;
  advertising_data.service_data.insert(
      {kFastAdvertisementServiceUuid1, kDeviceServiceData1ByteArray});
  EXPECT_FALSE(ble_v2_medium_->StartAdvertising(
      advertising_data, {.tx_power_level = api::ble_v2::TxPowerLevel::kHigh,
                         .is_connectable = true}));
  histogram_tester_.ExpectBucketCount(
      "Nearby.Connections.BleV2.StartAdvertising.Result",
      /*bucket: Failure=*/0, 1);
  histogram_tester_.ExpectBucketCount(
      "Nearby.Connections.BleV2.StartAdvertising.Result.ExtendedAdvertisement",
      /*bucket: Failure=*/0, 1);
  histogram_tester_.ExpectBucketCount(
      "Nearby.Connections.BleV2.StartAdvertising.FailureReason",
      metrics::StartAdvertisingFailureReason::kNoExtendedAdvertisementSupport,
      1);
  histogram_tester_.ExpectBucketCount(
      "Nearby.Connections.BleV2.StartAdvertising.FailureReason."
      "ExtendedAdvertisement",
      metrics::StartAdvertisingFailureReason::kNoExtendedAdvertisementSupport,
      1);
}

TEST_F(BleV2MediumTest, TestAdvertising_ExtendedAdvertisementSupported) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{::features::kEnableNearbyBleV2,
                            ::features::kEnableNearbyBleV2ExtendedAdvertising},
      /*disabled_features=*/{});
  EXPECT_TRUE(ble_v2_medium_->IsExtendedAdvertisementsAvailable());

  fake_adapter_->SetShouldAdvertisementRegistrationSucceed(true);
  api::ble_v2::BleAdvertisementData advertising_data;
  advertising_data.is_extended_advertisement = true;
  advertising_data.service_data.insert(
      {kFastAdvertisementServiceUuid1, kDeviceServiceData1ByteArray});
  EXPECT_TRUE(ble_v2_medium_->StartAdvertising(
      advertising_data, {.tx_power_level = api::ble_v2::TxPowerLevel::kHigh,
                         .is_connectable = true}));
  histogram_tester_.ExpectBucketCount(
      "Nearby.Connections.BleV2.StartAdvertising.Result",
      /*bucket: Success=*/1, 1);
  histogram_tester_.ExpectBucketCount(
      "Nearby.Connections.BleV2.StartAdvertising.Result.ExtendedAdvertisement",
      /*bucket: Success=*/1, 1);
}

TEST_F(BleV2MediumTest, TestAdvertising_EmptyAdvertisingData) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{::features::kEnableNearbyBleV2},
      /*disabled_features=*/{});

  fake_adapter_->SetShouldAdvertisementRegistrationSucceed(true);
  api::ble_v2::BleAdvertisementData advertising_data = {};
  // Passing in empty advertisement data is unexpected, but is still
  // expected to pass.
  EXPECT_TRUE(ble_v2_medium_->StartAdvertising(advertising_data, {}));
  histogram_tester_.ExpectBucketCount(
      "Nearby.Connections.BleV2.StartAdvertising.Result",
      /*bucket: Success=*/1, 1);
  histogram_tester_.ExpectBucketCount(
      "Nearby.Connections.BleV2.StartAdvertising.Result.RegularAdvertisement",
      /*bucket: Success=*/1, 1);
}

TEST_F(BleV2MediumTest, TestAdvertising_MultipleStartAdvertisingSuccess) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{::features::kEnableNearbyBleV2,
                            ::features::kEnableNearbyBleV2ExtendedAdvertising},
      /*disabled_features=*/{});
  EXPECT_TRUE(ble_v2_medium_->IsExtendedAdvertisementsAvailable());
  EXPECT_FALSE(fake_adapter_->GetRegisteredAdvertisementServiceData(
      kService1BluetoothUuid));

  fake_adapter_->SetShouldAdvertisementRegistrationSucceed(true);
  api::ble_v2::BleAdvertisementData advertising_data1;
  advertising_data1.is_extended_advertisement = false;
  advertising_data1.service_data.insert(
      {kFastAdvertisementServiceUuid1, kDeviceServiceData1ByteArray});
  EXPECT_TRUE(ble_v2_medium_->StartAdvertising(
      advertising_data1, {.tx_power_level = api::ble_v2::TxPowerLevel::kHigh,
                          .is_connectable = true}));
  EXPECT_TRUE(fake_adapter_->GetRegisteredAdvertisementServiceData(
      kService1BluetoothUuid));
  // TODO(b/330759317): Refactor FakeAdapter to hold multiple advertisements per
  // Bluetooth UUID, and remove private variable access here.
  EXPECT_EQ(1u, ble_v2_medium_->registered_advertisements_map_
                    .at(kService1BluetoothUuid)
                    .size());
  histogram_tester_.ExpectBucketCount(
      "Nearby.Connections.BleV2.StartAdvertising.Result",
      /*bucket: Success=*/1, 1);
  histogram_tester_.ExpectBucketCount(
      "Nearby.Connections.BleV2.StartAdvertising.Result.RegularAdvertisement",
      /*bucket: Success=*/1, 1);

  // We are expected to be able to concurrently advertise multiple
  // advertisements registered to the same service UUID.
  api::ble_v2::BleAdvertisementData advertising_data2;
  advertising_data2.is_extended_advertisement = true;
  advertising_data2.service_data.insert(
      {kFastAdvertisementServiceUuid1, kDeviceServiceData1ByteArray});
  EXPECT_TRUE(ble_v2_medium_->StartAdvertising(
      advertising_data2, {.tx_power_level = api::ble_v2::TxPowerLevel::kHigh,
                          .is_connectable = true}));
  EXPECT_TRUE(fake_adapter_->GetRegisteredAdvertisementServiceData(
      kService1BluetoothUuid));
  // TODO(b/330759317): Refactor FakeAdapter to hold multiple advertisements per
  // Bluetooth UUID, and remove private variable access here.
  EXPECT_EQ(2u, ble_v2_medium_->registered_advertisements_map_
                    .at(kService1BluetoothUuid)
                    .size());
  histogram_tester_.ExpectBucketCount(
      "Nearby.Connections.BleV2.StartAdvertising.Result",
      /*bucket: Success=*/1, 2);
  histogram_tester_.ExpectBucketCount(
      "Nearby.Connections.BleV2.StartAdvertising.Result.ExtendedAdvertisement",
      /*bucket: Success=*/1, 1);
}

TEST_F(BleV2MediumTest, TestAdvertising_MultipleAdvertisementDataSuccess) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{::features::kEnableNearbyBleV2},
      /*disabled_features=*/{});

  fake_adapter_->SetShouldAdvertisementRegistrationSucceed(true);
  api::ble_v2::BleAdvertisementData advertising_data;
  advertising_data.is_extended_advertisement = false;
  EXPECT_FALSE(fake_adapter_->GetRegisteredAdvertisementServiceData(
      kService1BluetoothUuid));
  EXPECT_FALSE(fake_adapter_->GetRegisteredAdvertisementServiceData(
      kService2BluetoothUuid));

  // Currently, NC does not pass in multiple advertisement data per call
  // to StartAdvertising. However, we are expected to support that
  // capability and start advertising for each one. This is a map, so
  // service UUIDs will be different in this case.
  advertising_data.service_data = {
      {kFastAdvertisementServiceUuid1, kDeviceServiceData1ByteArray},
      {kTestServiceUuid2, kDeviceServiceData2ByteArray}};
  EXPECT_TRUE(ble_v2_medium_->StartAdvertising(
      advertising_data, {.tx_power_level = api::ble_v2::TxPowerLevel::kLow,
                         .is_connectable = true}));
  EXPECT_TRUE(fake_adapter_->GetRegisteredAdvertisementServiceData(
      kService1BluetoothUuid));
  EXPECT_TRUE(fake_adapter_->GetRegisteredAdvertisementServiceData(
      kService2BluetoothUuid));
  histogram_tester_.ExpectBucketCount(
      "Nearby.Connections.BleV2.StartAdvertising.Result",
      /*bucket: Success=*/1, 1);
  histogram_tester_.ExpectBucketCount(
      "Nearby.Connections.BleV2.StartAdvertising.Result.RegularAdvertisement",
      /*bucket: Success=*/1, 1);
}

TEST_F(BleV2MediumTest, TestAdvertising_StopAdvertisingClearsRegistrationMap) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{::features::kEnableNearbyBleV2},
      /*disabled_features=*/{});

  fake_adapter_->SetShouldAdvertisementRegistrationSucceed(true);
  api::ble_v2::BleAdvertisementData advertising_data;
  advertising_data.is_extended_advertisement = false;
  advertising_data.service_data.insert(
      {kFastAdvertisementServiceUuid1, kDeviceServiceData1ByteArray});
  EXPECT_FALSE(fake_adapter_->GetRegisteredAdvertisementServiceData(
      kService1BluetoothUuid));

  EXPECT_TRUE(ble_v2_medium_->StartAdvertising(
      advertising_data, {.tx_power_level = api::ble_v2::TxPowerLevel::kLow,
                         .is_connectable = true}));
  EXPECT_TRUE(fake_adapter_->GetRegisteredAdvertisementServiceData(
      kService1BluetoothUuid));
  histogram_tester_.ExpectBucketCount(
      "Nearby.Connections.BleV2.StartAdvertising.Result",
      /*bucket: Success=*/1, 1);
  histogram_tester_.ExpectBucketCount(
      "Nearby.Connections.BleV2.StartAdvertising.Result.RegularAdvertisement",
      /*bucket: Success=*/1, 1);

  {
    base::RunLoop run_loop;
    fake_adapter_->SetAdvertisementDestroyedCallback(run_loop.QuitClosure());
    EXPECT_TRUE(ble_v2_medium_->StopAdvertising());
    run_loop.Run();
  }
  EXPECT_FALSE(fake_adapter_->GetRegisteredAdvertisementServiceData(
      kService1BluetoothUuid));
}

TEST_F(BleV2MediumTest, TestAdvertising_StartAndStopAsyncAdvertising) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{::features::kEnableNearbyBleV2},
      /*disabled_features=*/{});

  fake_adapter_->SetShouldAdvertisementRegistrationSucceed(true);
  api::ble_v2::BleAdvertisementData advertising_data;
  advertising_data.is_extended_advertisement = false;
  advertising_data.service_data.insert(
      {kFastAdvertisementServiceUuid1, kDeviceServiceData1ByteArray});
  EXPECT_FALSE(fake_adapter_->GetRegisteredAdvertisementServiceData(
      kService1BluetoothUuid));

  auto advertising_session = ble_v2_medium_->StartAdvertising(
      advertising_data,
      {.tx_power_level = api::ble_v2::TxPowerLevel::kLow,
       .is_connectable = true},
      api::ble_v2::BleMedium::AdvertisingCallback{
          .start_advertising_result =
              [this](absl::Status status) {
                EXPECT_TRUE(status.ok());
                histogram_tester_.ExpectBucketCount(
                    "Nearby.Connections.BleV2.StartAdvertising.Result",
                    /*bucket: Success=*/1, 1);
                histogram_tester_.ExpectBucketCount(
                    "Nearby.Connections.BleV2.StartAdvertising.Result."
                    "RegularAdvertisement",
                    /*bucket: Success=*/1, 1);
              },
      });
  EXPECT_NE(advertising_session, nullptr);

  EXPECT_TRUE(fake_adapter_->GetRegisteredAdvertisementServiceData(
      kService1BluetoothUuid));

  {
    base::RunLoop run_loop;
    fake_adapter_->SetAdvertisementDestroyedCallback(run_loop.QuitClosure());
    auto status = advertising_session->stop_advertising();
    EXPECT_TRUE(status.ok());
    run_loop.Run();
  }
  EXPECT_FALSE(fake_adapter_->GetRegisteredAdvertisementServiceData(
      kService1BluetoothUuid));
}

TEST_F(BleV2MediumTest, IsExtendedAdvertisementsAvailable_FlagDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{::features::kEnableNearbyBleV2},
      /*disabled_features=*/{
          ::features::kEnableNearbyBleV2ExtendedAdvertising});

  // If the flag is disabled, always return false.
  fake_adapter_->SetExtendedAdvertisementSupport(true);
  EXPECT_FALSE(ble_v2_medium_->IsExtendedAdvertisementsAvailable());

  fake_adapter_->SetExtendedAdvertisementSupport(false);
  EXPECT_FALSE(ble_v2_medium_->IsExtendedAdvertisementsAvailable());
}

TEST_F(BleV2MediumTest, IsExtendedAdvertisementsAvailable_FlagEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{::features::kEnableNearbyBleV2,
                            ::features::kEnableNearbyBleV2ExtendedAdvertising},
      /*disabled_features=*/{});

  // If the flag is enabled, return whether the device has hardware support.
  fake_adapter_->SetExtendedAdvertisementSupport(true);
  EXPECT_TRUE(ble_v2_medium_->IsExtendedAdvertisementsAvailable());

  fake_adapter_->SetExtendedAdvertisementSupport(false);
  EXPECT_FALSE(ble_v2_medium_->IsExtendedAdvertisementsAvailable());
}

TEST_F(BleV2MediumTest, StartGattServer_DualRoleSupported_FlagDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{::features::kEnableNearbyBleV2},
      /*disabled_features=*/{::features::kEnableNearbyBleV2GattServer});

  fake_adapter_->is_dual_role_supported_ = true;
  auto gatt_server = ble_v2_medium_->StartGattServer({});
  EXPECT_FALSE(gatt_server);
}

TEST_F(BleV2MediumTest, StartGattServer_DualRoleSupported_FlagEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{::features::kEnableNearbyBleV2,
                            ::features::kEnableNearbyBleV2GattServer},
      /*disabled_features=*/{});

  fake_adapter_->is_dual_role_supported_ = true;
  auto gatt_server = ble_v2_medium_->StartGattServer({});
  EXPECT_TRUE(gatt_server);
  histogram_tester_.ExpectBucketCount(
      "Nearby.Connections.BleV2.ScatternetDualRoleSupported",
      /*bucket: DualRole is supported=*/1, 1);

  // Clean up ble_v2_medium_ to prevent dangling raw_ptr in unit tests.
  ble_v2_medium_.reset();
}

TEST_F(BleV2MediumTest, StartGattServer_DualRoleNotSupported) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{::features::kEnableNearbyBleV2,
                            ::features::kEnableNearbyBleV2GattServer},
      /*disabled_features=*/{});

  fake_adapter_->is_dual_role_supported_ = false;
  auto gatt_server = ble_v2_medium_->StartGattServer({});
  EXPECT_FALSE(gatt_server);
  histogram_tester_.ExpectBucketCount(
      "Nearby.Connections.BleV2.ScatternetDualRoleSupported",
      /*bucket: DualRole is not supported=*/0, 1);
}

TEST_F(BleV2MediumTest, StartAdvertising_RegisterGattServer_Success) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{::features::kEnableNearbyBleV2,
                            ::features::kEnableNearbyBleV2GattServer},
      /*disabled_features=*/{});

  SetUpGattServerForAdvertising(/*should_register_succeed=*/true);

  base::RunLoop run_loop;
  base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})
      ->PostTaskAndReply(
          FROM_HERE,
          base::BindOnce(&BleV2MediumTest::CallStartAdvertisingForGattService,
                         base::Unretained(this), /*expected_result=*/true),
          run_loop.QuitClosure());
  run_loop.Run();

  histogram_tester_.ExpectBucketCount(
      "Nearby.Connections.BleV2.StartAdvertising.Result",
      /*bucket: Success=*/1, 1);
  histogram_tester_.ExpectTotalCount(
      "Nearby.Connections.BleV2.StartAdvertising.FailureReason", 0);
}

TEST_F(BleV2MediumTest, StartAdvertising_RegisterGattServer_Failure) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{::features::kEnableNearbyBleV2,
                            ::features::kEnableNearbyBleV2GattServer},
      /*disabled_features=*/{});

  SetUpGattServerForAdvertising(/*should_register_succeed=*/false);

  base::RunLoop run_loop;
  base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})
      ->PostTaskAndReply(
          FROM_HERE,
          base::BindOnce(&BleV2MediumTest::CallStartAdvertisingForGattService,
                         base::Unretained(this), /*expected_result=*/false),
          run_loop.QuitClosure());
  run_loop.Run();

  histogram_tester_.ExpectBucketCount(
      "Nearby.Connections.BleV2.StartAdvertising.Result",
      /*bucket: Failure=*/0, 1);
  histogram_tester_.ExpectBucketCount(
      "Nearby.Connections.BleV2.StartAdvertising.Result."
      "RegularAdvertisement",
      /*bucket: Failure=*/0, 1);
  histogram_tester_.ExpectBucketCount(
      "Nearby.Connections.BleV2.StartAdvertising.FailureReason",
      metrics::StartAdvertisingFailureReason::kFailedToRegisterGattServices, 1);
}

TEST_F(BleV2MediumTest, ConnectToGattServer_Success) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{::features::kEnableNearbyBleV2},
      /*disabled_features=*/{});

  fake_adapter_->SetConnectToDeviceResult(
      bluetooth::mojom::ConnectResult::SUCCESS,
      std::make_unique<bluetooth::FakeDevice>());
  base::RunLoop run_loop;
  base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})
      ->PostTaskAndReply(
          FROM_HERE,
          base::BindOnce(&BleV2MediumTest::CallConnectToGattServer,
                         base::Unretained(this), /*expected_result=*/true),
          run_loop.QuitClosure());
  run_loop.Run();
  histogram_tester_.ExpectBucketCount(
      "Nearby.Connections.BleV2.ConnectToGattServer.Result",
      /*bucket: success=*/1, 1);
  histogram_tester_.ExpectTotalCount(
      "Nearby.Connections.BleV2.ConnectToGattServer.Duration", 1);
}

TEST_F(BleV2MediumTest, ConnectToGattServer_Failure) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{::features::kEnableNearbyBleV2},
      /*disabled_features=*/{});

  fake_adapter_->SetConnectToDeviceResult(
      bluetooth::mojom::ConnectResult::FAILED, /*fake_device=*/nullptr);
  base::RunLoop run_loop;
  base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})
      ->PostTaskAndReply(
          FROM_HERE,
          base::BindOnce(&BleV2MediumTest::CallConnectToGattServer,
                         base::Unretained(this), /*expected_result=*/false),
          run_loop.QuitClosure());
  run_loop.Run();
  histogram_tester_.ExpectBucketCount(
      "Nearby.Connections.BleV2.ConnectToGattServer.Result",
      /*bucket: failure=*/0, 1);
  histogram_tester_.ExpectBucketCount(
      "Nearby.Connections.BleV2.ConnectToGattServer.FailureReason",
      /*bucket: FAILED=*/5, 1);
  histogram_tester_.ExpectTotalCount(
      "Nearby.Connections.BleV2.ConnectToGattServer.Duration", 0);
}

}  // namespace nearby::chrome
