// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/ble_medium.h"

#include <memory>
#include <optional>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chrome/services/sharing/nearby/platform/ble_peripheral.h"
#include "chrome/services/sharing/nearby/test_support/fake_adapter.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nearby::chrome {

namespace {

const char kServiceId1[] = "NearbySharing";
const char kServiceId2[] = "PhoneHub";
const char kFastAdvertisementServiceId1[] =
    "00000000-0000-0000-0000-000000000001";
const char kFastAdvertisementServiceId2[] =
    "00000000-0000-0000-0000-000000000002";
const char kDeviceAddress[] = "DeviceAddress";
const char kDeviceServiceData1Str[] = "Device_Advertisement1";
const char kDeviceServiceData2Str[] = "Device_Advertisement2";

std::vector<uint8_t> GetByteVector(const std::string& str) {
  return std::vector<uint8_t>(str.begin(), str.end());
}

}  // namespace

class BleMediumTest : public testing::Test {
 public:
  BleMediumTest() = default;
  ~BleMediumTest() override = default;
  BleMediumTest(const BleMediumTest&) = delete;
  BleMediumTest& operator=(const BleMediumTest&) = delete;

  void SetUp() override {
    auto fake_adapter = std::make_unique<bluetooth::FakeAdapter>();
    fake_adapter_ = fake_adapter.get();

    mojo::PendingRemote<bluetooth::mojom::Adapter> pending_adapter;

    mojo::MakeSelfOwnedReceiver(
        std::move(fake_adapter),
        pending_adapter.InitWithNewPipeAndPassReceiver());

    remote_adapter_.Bind(std::move(pending_adapter),
                         /*bind_task_runner=*/nullptr);

    ble_medium_ = std::make_unique<BleMedium>(remote_adapter_);

    discovered_peripheral_callback_ = {
        .peripheral_discovered_cb =
            [this](api::BlePeripheral& peripheral,
                   const std::string& service_id, bool fast_advertisement) {
              EXPECT_TRUE(fast_advertisement);
              OnPeripheralDiscovered(peripheral, service_id);
            },
        .peripheral_lost_cb =
            [this](api::BlePeripheral& peripheral,
                   const std::string& service_id) {
              OnPeripheralLost(peripheral, service_id);
            }};
  }

 protected:
  void StartScanning(const std::string& service_id,
                     const std::string& fast_advertisement_service_uuid) {
    EXPECT_EQ(!scanning_service_ids_set_.empty(),
              fake_adapter_->IsDiscoverySessionActive());
    scanning_service_ids_set_.insert(service_id);
    EXPECT_TRUE(ble_medium_->StartScanning(
        service_id, fast_advertisement_service_uuid,
        {.peripheral_discovered_cb =
             [this](api::BlePeripheral& peripheral,
                    const std::string& service_id, bool fast_advertisement) {
               EXPECT_TRUE(fast_advertisement);
               OnPeripheralDiscovered(peripheral, service_id);
             },
         .peripheral_lost_cb =
             [this](api::BlePeripheral& peripheral,
                    const std::string& service_id) {
               OnPeripheralLost(peripheral, service_id);
             }}));
    EXPECT_TRUE(fake_adapter_->IsDiscoverySessionActive());
  }

  void StopScanning(const std::string& service_id) {
    base::RunLoop run_loop;

    bool is_last_service = scanning_service_ids_set_.size() == 1u;
    if (is_last_service) {
      fake_adapter_->SetDiscoverySessionDestroyedCallback(
          run_loop.QuitClosure());
    }

    ble_medium_->StopScanning(service_id);

    if (is_last_service)
      run_loop.Run();

    EXPECT_EQ(!is_last_service, fake_adapter_->IsDiscoverySessionActive());
    scanning_service_ids_set_.erase(service_id);
  }

  void NotifyDeviceAdded(
      const std::string& address,
      const base::flat_map<device::BluetoothUUID, std::vector<uint8_t>>&
          service_data_map,
      uint32_t num_expected_peripherals_discovered) {
    base::RunLoop run_loop;
    SetOnExpectedPeripheralsDiscoveredCallback(
        run_loop.QuitClosure(), num_expected_peripherals_discovered);
    fake_adapter_->NotifyDeviceAdded(
        CreateDeviceInfo(address, service_data_map));
    run_loop.Run();
  }

  void NotifyDeviceChanged(
      const std::string& address,
      const base::flat_map<device::BluetoothUUID, std::vector<uint8_t>>&
          service_data_map,
      uint32_t num_expected_peripherals_discovered) {
    base::RunLoop run_loop;
    SetOnExpectedPeripheralsDiscoveredCallback(
        run_loop.QuitClosure(), num_expected_peripherals_discovered);
    fake_adapter_->NotifyDeviceChanged(
        CreateDeviceInfo(address, service_data_map));
    run_loop.Run();
  }

  void NotifyDeviceRemoved(const std::string& address,
                           uint32_t num_expected_peripherals_lost) {
    base::RunLoop run_loop;
    SetOnExpectedPeripheralsLostCallback(run_loop.QuitClosure(),
                                         num_expected_peripherals_lost);
    fake_adapter_->NotifyDeviceRemoved(
        CreateDeviceInfo(address, /*service_data_map=*/{}));
    run_loop.Run();
  }

  void VerifyByteArrayEquals(const ByteArray& byte_array,
                             const std::string& expected_value) {
    EXPECT_EQ(expected_value,
              std::string(byte_array.data(), byte_array.size()));
  }

  raw_ptr<bluetooth::FakeAdapter> fake_adapter_;
  mojo::SharedRemote<bluetooth::mojom::Adapter> remote_adapter_;
  std::unique_ptr<BleMedium> ble_medium_;

  BleMedium::DiscoveredPeripheralCallback discovered_peripheral_callback_;

  std::vector<std::pair<api::BlePeripheral*, std::string>>
      last_peripheral_discovered_args_;
  std::vector<std::pair<api::BlePeripheral*, std::string>>
      last_peripheral_lost_args_;

 private:
  void SetOnExpectedPeripheralsDiscoveredCallback(
      base::OnceClosure callback,
      uint32_t num_expected_peripherals_discovered) {
    on_expected_peripherals_discovered_callback_ = std::move(callback);
    num_expected_peripherals_discovered_ = num_expected_peripherals_discovered;
    last_peripheral_discovered_args_.clear();
  }

  void SetOnExpectedPeripheralsLostCallback(
      base::OnceClosure callback,
      uint32_t num_expected_peripherals_lost) {
    on_expected_peripherals_lost_callback_ = std::move(callback);
    num_expected_peripherals_lost_ = num_expected_peripherals_lost;
    last_peripheral_lost_args_.clear();
  }

  void OnPeripheralDiscovered(api::BlePeripheral& ble_peripheral,
                              const std::string& service_id) {
    last_peripheral_discovered_args_.emplace_back(
        std::make_pair(&ble_peripheral, service_id));

    if (last_peripheral_discovered_args_.size() ==
        num_expected_peripherals_discovered_) {
      std::move(on_expected_peripherals_discovered_callback_).Run();
      num_expected_peripherals_discovered_ = 0;
    }
  }

  void OnPeripheralLost(api::BlePeripheral& ble_peripheral,
                        const std::string& service_id) {
    last_peripheral_lost_args_.emplace_back(
        std::make_pair(&ble_peripheral, service_id));

    if (last_peripheral_lost_args_.size() == num_expected_peripherals_lost_) {
      std::move(on_expected_peripherals_lost_callback_).Run();
      num_expected_peripherals_lost_ = 0;
    }
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

    return device_info;
  }

  std::set<std::string> scanning_service_ids_set_;

  base::OnceClosure on_expected_peripherals_discovered_callback_;
  uint32_t num_expected_peripherals_discovered_ = 0;

  base::OnceClosure on_expected_peripherals_lost_callback_;
  uint32_t num_expected_peripherals_lost_ = 0;

  base::test::TaskEnvironment task_environment_;
};

TEST_F(BleMediumTest, TestAdvertising) {
  ASSERT_FALSE(fake_adapter_->GetRegisteredAdvertisementServiceData(
      device::BluetoothUUID(kFastAdvertisementServiceId1)));
  ASSERT_FALSE(fake_adapter_->GetRegisteredAdvertisementServiceData(
      device::BluetoothUUID(kFastAdvertisementServiceId2)));

  ble_medium_->StartAdvertising(kServiceId1, ByteArray(kDeviceServiceData1Str),
                                kFastAdvertisementServiceId1);
  EXPECT_EQ(GetByteVector(kDeviceServiceData1Str),
            *fake_adapter_->GetRegisteredAdvertisementServiceData(
                device::BluetoothUUID(kFastAdvertisementServiceId1)));
  EXPECT_FALSE(fake_adapter_->GetRegisteredAdvertisementServiceData(
      device::BluetoothUUID(kFastAdvertisementServiceId2)));

  ble_medium_->StartAdvertising(kServiceId2, ByteArray(kDeviceServiceData2Str),
                                kFastAdvertisementServiceId2);
  EXPECT_TRUE(fake_adapter_->GetRegisteredAdvertisementServiceData(
      device::BluetoothUUID(kFastAdvertisementServiceId1)));
  EXPECT_EQ(GetByteVector(kDeviceServiceData2Str),
            *fake_adapter_->GetRegisteredAdvertisementServiceData(
                device::BluetoothUUID(kFastAdvertisementServiceId2)));

  {
    base::RunLoop run_loop;
    fake_adapter_->SetAdvertisementDestroyedCallback(run_loop.QuitClosure());
    ble_medium_->StopAdvertising(kServiceId1);
    run_loop.Run();
  }

  EXPECT_FALSE(fake_adapter_->GetRegisteredAdvertisementServiceData(
      device::BluetoothUUID(kFastAdvertisementServiceId1)));
  EXPECT_TRUE(fake_adapter_->GetRegisteredAdvertisementServiceData(
      device::BluetoothUUID(kFastAdvertisementServiceId2)));

  {
    base::RunLoop run_loop;
    fake_adapter_->SetAdvertisementDestroyedCallback(run_loop.QuitClosure());
    ble_medium_->StopAdvertising(kServiceId2);
    run_loop.Run();
  }

  EXPECT_FALSE(fake_adapter_->GetRegisteredAdvertisementServiceData(
      device::BluetoothUUID(kFastAdvertisementServiceId1)));
  EXPECT_FALSE(fake_adapter_->GetRegisteredAdvertisementServiceData(
      device::BluetoothUUID(kFastAdvertisementServiceId2)));
}

TEST_F(BleMediumTest, TestScanning_OneService) {
  StartScanning(kServiceId1, kFastAdvertisementServiceId1);

  base::flat_map<device::BluetoothUUID, std::vector<uint8_t>> service_data_map;
  service_data_map.insert_or_assign(
      device::BluetoothUUID(kFastAdvertisementServiceId1),
      GetByteVector(kDeviceServiceData1Str));

  NotifyDeviceAdded(kDeviceAddress, service_data_map,
                    /*num_expected_peripherals_discovered=*/1u);
  ASSERT_EQ(1u, last_peripheral_discovered_args_.size());
  auto& last_peripheral_discovered_args = last_peripheral_discovered_args_[0];
  const auto* first_discovered_ble_peripheral =
      last_peripheral_discovered_args.first;
  EXPECT_EQ(kServiceId1, last_peripheral_discovered_args.second);
  VerifyByteArrayEquals(
      first_discovered_ble_peripheral->GetAdvertisementBytes(kServiceId1),
      kDeviceServiceData1Str);

  // The same information should be returned on a DeviceChanged event, with
  // the same BlePeripheral reference.
  NotifyDeviceChanged(kDeviceAddress, service_data_map,
                      /*num_expected_peripherals_discovered=*/1u);
  ASSERT_EQ(1u, last_peripheral_discovered_args_.size());
  last_peripheral_discovered_args = last_peripheral_discovered_args_[0];
  EXPECT_EQ(first_discovered_ble_peripheral,
            last_peripheral_discovered_args.first);
  EXPECT_EQ(kServiceId1, last_peripheral_discovered_args.second);
  VerifyByteArrayEquals(
      last_peripheral_discovered_args.first->GetAdvertisementBytes(kServiceId1),
      kDeviceServiceData1Str);

  // Again, the same BlePeripheral reference should be marked as lost.
  NotifyDeviceRemoved(kDeviceAddress, /*num_expected_peripherals_lost=*/1u);
  ASSERT_EQ(1u, last_peripheral_lost_args_.size());
  const auto& last_peripheral_lost_args = last_peripheral_lost_args_[0];
  const auto* lost_ble_peripheral = last_peripheral_lost_args.first;
  EXPECT_EQ(first_discovered_ble_peripheral, lost_ble_peripheral);
  EXPECT_EQ(kServiceId1, last_peripheral_lost_args.second);

  StopScanning(kServiceId1);
}

TEST_F(BleMediumTest, TestScanning_MultipleServices) {
  StartScanning(kServiceId1, kFastAdvertisementServiceId1);
  StartScanning(kServiceId2, kFastAdvertisementServiceId2);

  base::flat_map<device::BluetoothUUID, std::vector<uint8_t>> service_data_map;
  service_data_map.insert_or_assign(
      device::BluetoothUUID(kFastAdvertisementServiceId1),
      GetByteVector(kDeviceServiceData1Str));
  service_data_map.insert_or_assign(
      device::BluetoothUUID(kFastAdvertisementServiceId2),
      GetByteVector(kDeviceServiceData2Str));

  // Discovering a device with 2 desired service ids should trigger discovery
  // callbacks for both.
  NotifyDeviceAdded(kDeviceAddress, service_data_map,
                    /*num_expected_peripherals_discovered=*/2u);
  ASSERT_EQ(2u, last_peripheral_discovered_args_.size());
  VerifyByteArrayEquals(
      last_peripheral_discovered_args_[0].first->GetAdvertisementBytes(
          kServiceId1),
      kDeviceServiceData1Str);
  VerifyByteArrayEquals(
      last_peripheral_discovered_args_[1].first->GetAdvertisementBytes(
          kServiceId2),
      kDeviceServiceData2Str);

  NotifyDeviceRemoved(kDeviceAddress, /*num_expected_peripherals_lost=*/2u);
  ASSERT_EQ(2u, last_peripheral_lost_args_.size());

  StopScanning(kServiceId1);
  StopScanning(kServiceId2);
}

TEST_F(BleMediumTest, TestStartAcceptingConnections) {
  // StartAcceptingConnections() should do nothing but still return true.
  EXPECT_TRUE(
      ble_medium_->StartAcceptingConnections(kServiceId1, /*callback=*/{}));
}

TEST_F(BleMediumTest, TestConnect) {
  chrome::BlePeripheral ble_peripheral(
      bluetooth::mojom::DeviceInfo::New(),
      /*service_id_to_fast_advertisement_service_uuid_map=*/std::map<
          std::string, device::BluetoothUUID>());

  // Connect() should do nothing and not return a valid api::BleSocket.
  EXPECT_FALSE(ble_medium_->Connect(ble_peripheral, kServiceId1, nullptr));
}

}  // namespace nearby::chrome
