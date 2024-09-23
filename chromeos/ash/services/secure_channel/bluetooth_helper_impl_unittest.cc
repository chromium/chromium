// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/bluetooth_helper_impl.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "chromeos/ash/components/multidevice/remote_device_cache.h"
#include "chromeos/ash/components/multidevice/remote_device_test_util.h"
#include "chromeos/ash/services/secure_channel/ble_advertisement_generator.h"
#include "chromeos/ash/services/secure_channel/device_id_pair.h"
#include "chromeos/ash/services/secure_channel/fake_background_eid_generator.h"
#include "chromeos/ash/services/secure_channel/fake_ble_advertisement_generator.h"
#include "chromeos/ash/services/secure_channel/mock_foreground_eid_generator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::secure_channel {

namespace {

const size_t kNumTestDevices = 3;

const size_t kNumBytesInBackgroundAdvertisementServiceData = 2;
const size_t kMinNumBytesInForegroundAdvertisementServiceData = 4;

const char current_eid_data[] = "currentEidData";
const int64_t current_eid_start_ms = 1000L;
const int64_t current_eid_end_ms = 2000L;

const char adjacent_eid_data[] = "adjacentEidData";
const int64_t adjacent_eid_start_ms = 2000L;
const int64_t adjacent_eid_end_ms = 3000L;

const char fake_beacon_seed1_data[] = "fakeBeaconSeed1Data";
const int64_t fake_beacon_seed1_start_ms = current_eid_start_ms;
const int64_t fake_beacon_seed1_end_ms = current_eid_end_ms;

const char fake_beacon_seed2_data[] = "fakeBeaconSeed2Data";
const int64_t fake_beacon_seed2_start_ms = adjacent_eid_start_ms;
const int64_t fake_beacon_seed2_end_ms = adjacent_eid_end_ms;

std::unique_ptr<ForegroundEidGenerator::EidData>
CreateFakeBackgroundScanFilter() {
  DataWithTimestamp current(current_eid_data, current_eid_start_ms,
                            current_eid_end_ms);

  std::unique_ptr<DataWithTimestamp> adjacent =
      std::make_unique<DataWithTimestamp>(
          adjacent_eid_data, adjacent_eid_start_ms, adjacent_eid_end_ms);

  return std::make_unique<ForegroundEidGenerator::EidData>(current,
                                                           std::move(adjacent));
}

std::vector<multidevice::BeaconSeed> CreateFakeBeaconSeeds(int id) {
  std::string id_str = base::NumberToString(id);

  multidevice::BeaconSeed seed1(
      fake_beacon_seed1_data + id_str /* data */,
      base::Time::FromMillisecondsSinceUnixEpoch(fake_beacon_seed1_start_ms *
                                                 id) /* start_time */,
      base::Time::FromMillisecondsSinceUnixEpoch(fake_beacon_seed1_end_ms *
                                                 id) /* end_time */);

  multidevice::BeaconSeed seed2(
      fake_beacon_seed2_data + id_str /* data */,
      base::Time::FromMillisecondsSinceUnixEpoch(fake_beacon_seed2_start_ms *
                                                 id) /* start_time */,
      base::Time::FromMillisecondsSinceUnixEpoch(fake_beacon_seed2_end_ms *
                                                 id) /* end_time */);

  std::vector<multidevice::BeaconSeed> seeds = {seed1, seed2};
  return seeds;
}

multidevice::RemoteDeviceRef CreateLocalDevice(int id) {
  return multidevice::RemoteDeviceRefBuilder()
      .SetInstanceId("local instance id " + base::NumberToString(id))
      .SetPublicKey("local public key " + base::NumberToString(id))
      .SetBeaconSeeds(CreateFakeBeaconSeeds(id))
      .SetBluetoothPublicAddress(base::NumberToString(id))
      .Build();
}

}  // namespace

class SecureChannelBluetoothHelperImplTest : public testing::Test {
 public:
  SecureChannelBluetoothHelperImplTest(
      const SecureChannelBluetoothHelperImplTest&) = delete;
  SecureChannelBluetoothHelperImplTest& operator=(
      const SecureChannelBluetoothHelperImplTest&) = delete;

 protected:
  SecureChannelBluetoothHelperImplTest()
      : test_local_device_1_(CreateLocalDevice(1)),
        test_local_device_2_(CreateLocalDevice(2)),
        test_remote_devices_(
            multidevice::CreateRemoteDeviceRefListForTest(kNumTestDevices)),
        fake_advertisement_(DataWithTimestamp("advertisement1", 1000L, 2000L)) {
    device_id_pair_set_.emplace(test_remote_devices_[0].GetDeviceId(),
                                test_local_device_1_.GetDeviceId());
    device_id_pair_set_.emplace(test_remote_devices_[1].GetDeviceId(),
                                test_local_device_1_.GetDeviceId());
    device_id_pair_set_.emplace(test_remote_devices_[2].GetDeviceId(),
                                test_local_device_2_.GetDeviceId());
  }

  ~SecureChannelBluetoothHelperImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    fake_ble_advertisement_generator_ =
        std::make_unique<FakeBleAdvertisementGenerator>();
    BleAdvertisementGenerator::SetInstanceForTesting(
        fake_ble_advertisement_generator_.get());
    fake_ble_advertisement_generator_->set_advertisement(
        std::make_unique<DataWithTimestamp>(fake_advertisement_));

    auto mock_foreground_eid_generator =
        std::make_unique<MockForegroundEidGenerator>();
    mock_foreground_eid_generator_ = mock_foreground_eid_generator.get();
    mock_foreground_eid_generator_->set_background_scan_filter(
        CreateFakeBackgroundScanFilter());

    auto fake_background_eid_generator =
        std::make_unique<FakeBackgroundEidGenerator>();
    fake_background_eid_generator_ = fake_background_eid_generator.get();

    remote_device_cache_ = multidevice::RemoteDeviceCache::Factory::Create();

    multidevice::RemoteDeviceList devices;
    devices.push_back(
        *multidevice::GetMutableRemoteDevice(test_local_device_1_));
    devices.push_back(
        *multidevice::GetMutableRemoteDevice(test_local_device_2_));
    base::ranges::transform(
        test_remote_devices_, std::back_inserter(devices),
        [](auto remote_device_ref) {
          return *multidevice::GetMutableRemoteDevice(remote_device_ref);
        });
    remote_device_cache_->SetRemoteDevices(devices);

    helper_ = BluetoothHelperImpl::Factory::Create(remote_device_cache_.get());

    static_cast<BluetoothHelperImpl*>(helper_.get())
        ->SetTestDoubles(std::move(fake_background_eid_generator),
                         std::move(mock_foreground_eid_generator));
  }

  void TearDown() override {
    BleAdvertisementGenerator::SetInstanceForTesting(nullptr);
  }

  std::unique_ptr<FakeBleAdvertisementGenerator>
      fake_ble_advertisement_generator_;
  raw_ptr<MockForegroundEidGenerator, DanglingUntriaged>
      mock_foreground_eid_generator_;
  raw_ptr<FakeBackgroundEidGenerator, DanglingUntriaged>
      fake_background_eid_generator_;

  std::unique_ptr<multidevice::RemoteDeviceCache> remote_device_cache_;

  std::unique_ptr<BluetoothHelper> helper_;

  multidevice::RemoteDeviceRef test_local_device_1_;
  multidevice::RemoteDeviceRef test_local_device_2_;
  multidevice::RemoteDeviceRefList test_remote_devices_;
  DeviceIdPairSet device_id_pair_set_;

  DataWithTimestamp fake_advertisement_;
};

TEST_F(SecureChannelBluetoothHelperImplTest,
       TestGenerateForegroundAdvertisement_CannotGenerateAdvertisement) {
  fake_ble_advertisement_generator_->set_advertisement(nullptr);
  EXPECT_FALSE(helper_->GenerateForegroundAdvertisement(
      DeviceIdPair(test_remote_devices_[0].GetDeviceId() /* remote_device_id */,
                   test_local_device_1_.GetDeviceId() /* local_device_id */)));
}

TEST_F(SecureChannelBluetoothHelperImplTest,
       TestGenerateForegroundAdvertisement) {
  auto data_with_timestamp = helper_->GenerateForegroundAdvertisement(
      DeviceIdPair(test_remote_devices_[0].GetDeviceId() /* remote_device_id */,
                   test_local_device_1_.GetDeviceId() /* local_device_id */));
  EXPECT_EQ(fake_advertisement_, *data_with_timestamp);
}

TEST_F(SecureChannelBluetoothHelperImplTest,
       TestGenerateForegroundAdvertisement_InvalidLocalDevice) {
  EXPECT_FALSE(helper_->GenerateForegroundAdvertisement(
      DeviceIdPair(test_remote_devices_[0].GetDeviceId() /* remote_device_id */,
                   "invalid local device id" /* local_device_id */)));
}

TEST_F(SecureChannelBluetoothHelperImplTest,
       TestGenerateForegroundAdvertisement_InvalidRemoteDevice) {
  EXPECT_FALSE(helper_->GenerateForegroundAdvertisement(
      DeviceIdPair("invalid remote device id" /* remote_device_id */,
                   test_local_device_1_.GetDeviceId() /* local_device_id */)));
}

TEST_F(SecureChannelBluetoothHelperImplTest,
       TestIdentifyRemoteDevice_InvalidAdvertisementLength) {
  std::string invalid_service_data = "a";
  mock_foreground_eid_generator_->set_identified_device_id(
      test_remote_devices_[0].GetDeviceId());

  auto device_with_background_bool =
      helper_->IdentifyRemoteDevice(invalid_service_data, device_id_pair_set_);

  EXPECT_EQ(0, mock_foreground_eid_generator_->num_identify_calls());
  EXPECT_EQ(0, fake_background_eid_generator_->num_identify_calls());
  EXPECT_FALSE(device_with_background_bool);
}

TEST_F(SecureChannelBluetoothHelperImplTest,
       TestIdentifyRemoteDevice_ForegroundAdvertisement) {
  std::string valid_service_data_for_registered_device = "abcde";
  ASSERT_TRUE(valid_service_data_for_registered_device.size() >=
              kMinNumBytesInForegroundAdvertisementServiceData);

  mock_foreground_eid_generator_->set_identified_device_id(
      test_remote_devices_[0].GetDeviceId());

  auto device_with_background_bool = helper_->IdentifyRemoteDevice(
      valid_service_data_for_registered_device, device_id_pair_set_);

  EXPECT_EQ(1, mock_foreground_eid_generator_->num_identify_calls());
  EXPECT_EQ(0, fake_background_eid_generator_->num_identify_calls());
  EXPECT_EQ(test_remote_devices_[0].GetDeviceId(),
            device_with_background_bool->first.GetDeviceId());
  EXPECT_FALSE(device_with_background_bool->second);

  // Ensure that other local device IDs in device_id_pair_set_ are considered.
  mock_foreground_eid_generator_->set_identified_device_id(
      test_remote_devices_[2].GetDeviceId());

  device_with_background_bool = helper_->IdentifyRemoteDevice(
      valid_service_data_for_registered_device, device_id_pair_set_);

  EXPECT_EQ(0, fake_background_eid_generator_->num_identify_calls());
  EXPECT_EQ(test_remote_devices_[2].GetDeviceId(),
            device_with_background_bool->first.GetDeviceId());
  EXPECT_FALSE(device_with_background_bool->second);
}

TEST_F(SecureChannelBluetoothHelperImplTest,
       TestIdentifyRemoteDevice_ForegroundAdvertisement_NoRegisteredDevice) {
  std::string valid_service_data = "abcde";
  ASSERT_TRUE(valid_service_data.size() >=
              kMinNumBytesInForegroundAdvertisementServiceData);

  auto device_with_background_bool =
      helper_->IdentifyRemoteDevice(valid_service_data, device_id_pair_set_);

  EXPECT_EQ(2, mock_foreground_eid_generator_->num_identify_calls());
  EXPECT_EQ(0, fake_background_eid_generator_->num_identify_calls());
  EXPECT_FALSE(device_with_background_bool);
}

TEST_F(SecureChannelBluetoothHelperImplTest,
       TestIdentifyRemoteDevice_BackgroundAdvertisement) {
  std::string valid_service_data_for_registered_device = "ab";
  ASSERT_TRUE(valid_service_data_for_registered_device.size() >=
              kNumBytesInBackgroundAdvertisementServiceData);

  fake_background_eid_generator_->set_identified_device_id(
      test_remote_devices_[0].GetDeviceId());

  auto device_with_background_bool = helper_->IdentifyRemoteDevice(
      valid_service_data_for_registered_device, device_id_pair_set_);

  EXPECT_EQ(0, mock_foreground_eid_generator_->num_identify_calls());
  EXPECT_EQ(1, fake_background_eid_generator_->num_identify_calls());
  EXPECT_EQ(test_remote_devices_[0].GetDeviceId(),
            device_with_background_bool->first.GetDeviceId());
  EXPECT_TRUE(device_with_background_bool->second);

  // Ensure that other local device IDs in device_id_pair_set_ are considered.
  fake_background_eid_generator_->set_identified_device_id(
      test_remote_devices_[2].GetDeviceId());

  device_with_background_bool = helper_->IdentifyRemoteDevice(
      valid_service_data_for_registered_device, device_id_pair_set_);

  EXPECT_EQ(0, mock_foreground_eid_generator_->num_identify_calls());
  EXPECT_EQ(test_remote_devices_[2].GetDeviceId(),
            device_with_background_bool->first.GetDeviceId());
  EXPECT_TRUE(device_with_background_bool->second);
}

TEST_F(SecureChannelBluetoothHelperImplTest,
       TestIdentifyRemoteDevice_BackgroundAdvertisement_NoRegisteredDevice) {
  std::string valid_service_data_for_registered_device = "ab";
  ASSERT_TRUE(valid_service_data_for_registered_device.size() >=
              kNumBytesInBackgroundAdvertisementServiceData);

  auto device_with_background_bool = helper_->IdentifyRemoteDevice(
      valid_service_data_for_registered_device, device_id_pair_set_);

  EXPECT_EQ(0, mock_foreground_eid_generator_->num_identify_calls());
  EXPECT_EQ(2, fake_background_eid_generator_->num_identify_calls());
  EXPECT_FALSE(device_with_background_bool);
}

TEST_F(SecureChannelBluetoothHelperImplTest, BluetoothPublicAddress) {
  EXPECT_EQ("1", test_local_device_1_.bluetooth_public_address());
  EXPECT_EQ("1", helper_->GetBluetoothPublicAddress(
                     test_local_device_1_.GetDeviceId()));
}

}  // namespace ash::secure_channel
