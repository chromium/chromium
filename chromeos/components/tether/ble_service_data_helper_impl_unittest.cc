// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/ble_service_data_helper_impl.h"

#include <memory>

#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/chromeos_features.h"
#include "chromeos/components/tether/fake_tether_host_fetcher.h"
#include "chromeos/services/device_sync/public/cpp/fake_device_sync_client.h"
#include "chromeos/services/secure_channel/device_id_pair.h"
#include "components/cryptauth/ble/ble_advertisement_generator.h"
#include "components/cryptauth/ble/fake_ble_advertisement_generator.h"
#include "components/cryptauth/fake_background_eid_generator.h"
#include "components/cryptauth/mock_foreground_eid_generator.h"
#include "components/cryptauth/mock_local_device_data_provider.h"
#include "components/cryptauth/remote_device_cache.h"
#include "components/cryptauth/remote_device_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace tether {

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

std::unique_ptr<cryptauth::ForegroundEidGenerator::EidData>
CreateFakeBackgroundScanFilter() {
  cryptauth::DataWithTimestamp current(current_eid_data, current_eid_start_ms,
                                       current_eid_end_ms);

  std::unique_ptr<cryptauth::DataWithTimestamp> adjacent =
      std::make_unique<cryptauth::DataWithTimestamp>(
          adjacent_eid_data, adjacent_eid_start_ms, adjacent_eid_end_ms);

  return std::make_unique<cryptauth::ForegroundEidGenerator::EidData>(
      current, std::move(adjacent));
}

std::vector<cryptauth::BeaconSeed> CreateFakeBeaconSeeds() {
  cryptauth::BeaconSeed seed1;
  seed1.set_data(fake_beacon_seed1_data);
  seed1.set_start_time_millis(fake_beacon_seed1_start_ms);
  seed1.set_start_time_millis(fake_beacon_seed1_end_ms);

  cryptauth::BeaconSeed seed2;
  seed2.set_data(fake_beacon_seed2_data);
  seed2.set_start_time_millis(fake_beacon_seed2_start_ms);
  seed2.set_start_time_millis(fake_beacon_seed2_end_ms);

  std::vector<cryptauth::BeaconSeed> seeds = {seed1, seed2};
  return seeds;
}

cryptauth::RemoteDeviceRef CreateLocalDevice() {
  return cryptauth::RemoteDeviceRefBuilder()
      .SetPublicKey("local public key")
      .SetBeaconSeeds(CreateFakeBeaconSeeds())
      .Build();
}

}  // namespace

class BleServiceDataHelperImplTest : public testing::Test {
 protected:
  BleServiceDataHelperImplTest()
      : test_local_device_(CreateLocalDevice()),
        test_remote_devices_(
            cryptauth::CreateRemoteDeviceRefListForTest(kNumTestDevices)),
        fake_advertisement_(
            cryptauth::DataWithTimestamp("advertisement1", 1000L, 2000L)) {
    std::transform(test_remote_devices_.begin(), test_remote_devices_.end(),
                   std::back_inserter(test_remote_device_ids_),
                   [](const auto& device) { return device.GetDeviceId(); });
  };
  ~BleServiceDataHelperImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    fake_ble_advertisement_generator_ =
        std::make_unique<cryptauth::FakeBleAdvertisementGenerator>();
    cryptauth::BleAdvertisementGenerator::SetInstanceForTesting(
        fake_ble_advertisement_generator_.get());
    fake_ble_advertisement_generator_->set_advertisement(
        std::make_unique<cryptauth::DataWithTimestamp>(fake_advertisement_));

    fake_tether_host_fetcher_ = std::make_unique<FakeTetherHostFetcher>();
    fake_tether_host_fetcher_->set_tether_hosts({test_remote_devices_[0],
                                                 test_remote_devices_[1],
                                                 test_remote_devices_[2]});

    mock_local_device_data_provider_ =
        std::make_unique<cryptauth::MockLocalDeviceDataProvider>();
    mock_local_device_data_provider_->SetBeaconSeeds(
        std::make_unique<std::vector<cryptauth::BeaconSeed>>(
            test_local_device_.beacon_seeds()));

    fake_device_sync_client_ =
        std::make_unique<device_sync::FakeDeviceSyncClient>();

    mock_foreground_eid_generator_ =
        new cryptauth::MockForegroundEidGenerator();
    mock_foreground_eid_generator_->set_background_scan_filter(
        CreateFakeBackgroundScanFilter());
    fake_background_eid_generator_ =
        new cryptauth::FakeBackgroundEidGenerator();
  }

  void TearDown() override {
    cryptauth::BleAdvertisementGenerator::SetInstanceForTesting(nullptr);
  }

  void InitializeTest() {
    helper_ = base::WrapUnique(new BleServiceDataHelperImpl(
        fake_tether_host_fetcher_.get(),
        base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi)
            ? nullptr
            : mock_local_device_data_provider_.get(),
        base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi)
            ? fake_device_sync_client_.get()
            : nullptr));

    helper_->SetTestDoubles(base::WrapUnique(fake_background_eid_generator_),
                            base::WrapUnique(mock_foreground_eid_generator_));

    SetLocalDeviceMetadata(test_local_device_);
  }

  void SetMultiDeviceApiEnabled() {
    scoped_feature_list_.InitAndEnableFeature(features::kMultiDeviceApi);
  }

  void SetLocalDeviceMetadata(
      base::Optional<cryptauth::RemoteDeviceRef> local_device) {
    if (base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi)) {
      fake_device_sync_client_->set_local_device_metadata(local_device);
    } else {
      mock_local_device_data_provider_->SetPublicKey(
          local_device
              ? std::make_unique<std::string>(local_device->public_key())
              : nullptr);
    }
  }

  void TestIdentifyRemoteDevice_InvalidAdvertisementLength() {
    InitializeTest();

    std::string invalid_service_data = "a";
    mock_foreground_eid_generator_->set_identified_device_id(
        test_remote_devices_[0].GetDeviceId());

    auto device_with_background_bool = helper_->IdentifyRemoteDevice(
        invalid_service_data, test_remote_device_ids_);

    EXPECT_EQ(0, mock_foreground_eid_generator_->num_identify_calls());
    EXPECT_EQ(0, fake_background_eid_generator_->num_identify_calls());
    EXPECT_FALSE(device_with_background_bool);
  }

  void TestIdentifyRemoteDevice_ForegroundAdvertisement() {
    InitializeTest();

    std::string valid_service_data_for_registered_device = "abcde";
    ASSERT_TRUE(valid_service_data_for_registered_device.size() >=
                kMinNumBytesInForegroundAdvertisementServiceData);

    mock_foreground_eid_generator_->set_identified_device_id(
        test_remote_devices_[0].GetDeviceId());

    auto device_with_background_bool = helper_->IdentifyRemoteDevice(
        valid_service_data_for_registered_device, test_remote_device_ids_);

    EXPECT_EQ(1, mock_foreground_eid_generator_->num_identify_calls());
    EXPECT_EQ(0, fake_background_eid_generator_->num_identify_calls());
    EXPECT_EQ(test_remote_devices_[0].GetDeviceId(),
              device_with_background_bool->first.GetDeviceId());
    EXPECT_FALSE(device_with_background_bool->second);
  }

  void TestIdentifyRemoteDevice_ForegroundAdvertisement_NoRegisteredDevice() {
    InitializeTest();

    std::string valid_service_data = "abcde";
    ASSERT_TRUE(valid_service_data.size() >=
                kMinNumBytesInForegroundAdvertisementServiceData);

    auto device_with_background_bool = helper_->IdentifyRemoteDevice(
        valid_service_data, test_remote_device_ids_);

    EXPECT_EQ(1, mock_foreground_eid_generator_->num_identify_calls());
    EXPECT_EQ(0, fake_background_eid_generator_->num_identify_calls());
    EXPECT_FALSE(device_with_background_bool);
  }

  void TestIdentifyRemoteDevice_BackgroundAdvertisement() {
    InitializeTest();

    std::string valid_service_data_for_registered_device = "ab";
    ASSERT_TRUE(valid_service_data_for_registered_device.size() >=
                kNumBytesInBackgroundAdvertisementServiceData);

    fake_background_eid_generator_->set_identified_device_id(
        test_remote_devices_[0].GetDeviceId());

    auto device_with_background_bool = helper_->IdentifyRemoteDevice(
        valid_service_data_for_registered_device, test_remote_device_ids_);

    EXPECT_EQ(0, mock_foreground_eid_generator_->num_identify_calls());
    EXPECT_EQ(1, fake_background_eid_generator_->num_identify_calls());
    EXPECT_EQ(test_remote_devices_[0].GetDeviceId(),
              device_with_background_bool->first.GetDeviceId());
    EXPECT_TRUE(device_with_background_bool->second);
  }

  void TestIdentifyRemoteDevice_BackgroundAdvertisement_NoRegisteredDevice() {
    InitializeTest();

    std::string valid_service_data_for_registered_device = "ab";
    ASSERT_TRUE(valid_service_data_for_registered_device.size() >=
                kNumBytesInBackgroundAdvertisementServiceData);

    auto device_with_background_bool = helper_->IdentifyRemoteDevice(
        valid_service_data_for_registered_device, test_remote_device_ids_);

    EXPECT_EQ(0, mock_foreground_eid_generator_->num_identify_calls());
    EXPECT_EQ(1, fake_background_eid_generator_->num_identify_calls());
    EXPECT_FALSE(device_with_background_bool);
  }

  std::unique_ptr<cryptauth::DataWithTimestamp>
  CallGenerateForegroundAdvertisement(const std::string& remote_device_id) {
    return helper_->GenerateForegroundAdvertisement(
        secure_channel::DeviceIdPair(remote_device_id,
                                     std::string() /* local_device_id */));
  }

  std::unique_ptr<cryptauth::FakeBleAdvertisementGenerator>
      fake_ble_advertisement_generator_;
  std::unique_ptr<FakeTetherHostFetcher> fake_tether_host_fetcher_;
  std::unique_ptr<cryptauth::MockLocalDeviceDataProvider>
      mock_local_device_data_provider_;
  std::unique_ptr<device_sync::FakeDeviceSyncClient> fake_device_sync_client_;

  cryptauth::MockForegroundEidGenerator* mock_foreground_eid_generator_;
  cryptauth::FakeBackgroundEidGenerator* fake_background_eid_generator_;

  std::unique_ptr<BleServiceDataHelperImpl> helper_;

  cryptauth::RemoteDeviceRef test_local_device_;
  cryptauth::RemoteDeviceRefList test_remote_devices_;
  std::vector<std::string> test_remote_device_ids_;
  cryptauth::DataWithTimestamp fake_advertisement_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(BleServiceDataHelperImplTest);
};

TEST_F(BleServiceDataHelperImplTest,
       TestGenerateForegroundAdvertisement_CannotGenerateAdvertisement) {
  InitializeTest();
  fake_ble_advertisement_generator_->set_advertisement(nullptr);
  EXPECT_FALSE(CallGenerateForegroundAdvertisement(
      test_remote_devices_[1].GetDeviceId()));
}

TEST_F(
    BleServiceDataHelperImplTest,
    TestGenerateForegroundAdvertisement_CannotGenerateAdvertisement_MultideviceApiEnabled) {
  SetMultiDeviceApiEnabled();

  InitializeTest();
  fake_ble_advertisement_generator_->set_advertisement(nullptr);
  EXPECT_FALSE(CallGenerateForegroundAdvertisement(
      test_remote_devices_[1].GetDeviceId()));
}

TEST_F(BleServiceDataHelperImplTest, TestGenerateForegroundAdvertisement) {
  InitializeTest();
  auto data_with_timestamp = CallGenerateForegroundAdvertisement(
      test_remote_devices_[1].GetDeviceId());
  EXPECT_EQ(fake_advertisement_, *data_with_timestamp);
}

TEST_F(BleServiceDataHelperImplTest,
       TestGenerateForegroundAdvertisement_MultideviceApiEnabled) {
  SetMultiDeviceApiEnabled();

  InitializeTest();
  auto data_with_timestamp = CallGenerateForegroundAdvertisement(
      test_remote_devices_[1].GetDeviceId());
  EXPECT_EQ(fake_advertisement_, *data_with_timestamp);
}

TEST_F(BleServiceDataHelperImplTest,
       TestGenerateForegroundAdvertisement_NoLocalDeviceMetadata) {
  InitializeTest();
  SetLocalDeviceMetadata(base::nullopt);
  EXPECT_FALSE(CallGenerateForegroundAdvertisement(
      test_remote_devices_[0].GetDeviceId()));
}

TEST_F(
    BleServiceDataHelperImplTest,
    TestGenerateForegroundAdvertisement_NoLocalDeviceMetadata_MultideviceApiEnabled) {
  SetMultiDeviceApiEnabled();

  InitializeTest();
  SetLocalDeviceMetadata(base::nullopt);
  EXPECT_FALSE(CallGenerateForegroundAdvertisement(
      test_remote_devices_[0].GetDeviceId()));
}

TEST_F(BleServiceDataHelperImplTest,
       TestGenerateForegroundAdvertisement_EmptyLocalPublicKey) {
  InitializeTest();

  SetLocalDeviceMetadata(
      cryptauth::RemoteDeviceRefBuilder().SetPublicKey(std::string()).Build());
  EXPECT_FALSE(CallGenerateForegroundAdvertisement(
      test_remote_devices_[0].GetDeviceId()));
}

TEST_F(
    BleServiceDataHelperImplTest,
    TestGenerateForegroundAdvertisement_EmptyLocalPublicKey_MultideviceApiEnabled) {
  SetMultiDeviceApiEnabled();

  InitializeTest();
  SetLocalDeviceMetadata(
      cryptauth::RemoteDeviceRefBuilder().SetPublicKey(std::string()).Build());
  EXPECT_FALSE(CallGenerateForegroundAdvertisement(
      test_remote_devices_[0].GetDeviceId()));
}

TEST_F(BleServiceDataHelperImplTest,
       TestGenerateForegroundAdvertisement_RemoteDeviceNotInTetherHostList) {
  InitializeTest();
  EXPECT_FALSE(CallGenerateForegroundAdvertisement("invalid device id"));
}

TEST_F(
    BleServiceDataHelperImplTest,
    TestGenerateForegroundAdvertisement_RemoteDeviceNotInTetherHostList_MultideviceApiEnabled) {
  SetMultiDeviceApiEnabled();

  InitializeTest();
  EXPECT_FALSE(CallGenerateForegroundAdvertisement("invalid device id"));
}

TEST_F(BleServiceDataHelperImplTest,
       TestIdentifyRemoteDevice_InvalidAdvertisementLength) {
  TestIdentifyRemoteDevice_InvalidAdvertisementLength();
}

TEST_F(
    BleServiceDataHelperImplTest,
    TestIdentifyRemoteDevice_InvalidAdvertisementLength_MultideviceApiEnabled) {
  SetMultiDeviceApiEnabled();
  TestIdentifyRemoteDevice_InvalidAdvertisementLength();
}

TEST_F(BleServiceDataHelperImplTest,
       TestIdentifyRemoteDevice_ForegroundAdvertisement) {
  TestIdentifyRemoteDevice_ForegroundAdvertisement();
}

TEST_F(BleServiceDataHelperImplTest,
       TestIdentifyRemoteDevice_ForegroundAdvertisement_MultideviceApiEnabled) {
  SetMultiDeviceApiEnabled();
  TestIdentifyRemoteDevice_ForegroundAdvertisement();
}

TEST_F(BleServiceDataHelperImplTest,
       TestIdentifyRemoteDevice_ForegroundAdvertisement_NoRegisteredDevice) {
  TestIdentifyRemoteDevice_ForegroundAdvertisement_NoRegisteredDevice();
}

TEST_F(
    BleServiceDataHelperImplTest,
    TestIdentifyRemoteDevice_ForegroundAdvertisement_NoRegisteredDevice_MultideviceApiEnabled) {
  SetMultiDeviceApiEnabled();
  TestIdentifyRemoteDevice_ForegroundAdvertisement_NoRegisteredDevice();
}

TEST_F(BleServiceDataHelperImplTest,
       TestIdentifyRemoteDevice_BackgroundAdvertisement) {
  TestIdentifyRemoteDevice_BackgroundAdvertisement();
}

TEST_F(BleServiceDataHelperImplTest,
       TestIdentifyRemoteDevice_BackgroundAdvertisement_MultideviceApiEnabled) {
  SetMultiDeviceApiEnabled();
  TestIdentifyRemoteDevice_BackgroundAdvertisement();
}

TEST_F(BleServiceDataHelperImplTest,
       TestIdentifyRemoteDevice_BackgroundAdvertisement_NoRegisteredDevice) {
  TestIdentifyRemoteDevice_BackgroundAdvertisement_NoRegisteredDevice();
}

TEST_F(
    BleServiceDataHelperImplTest,
    TestIdentifyRemoteDevice_BackgroundAdvertisement_NoRegisteredDevice_MultideviceApiEnabled) {
  SetMultiDeviceApiEnabled();
  TestIdentifyRemoteDevice_BackgroundAdvertisement_NoRegisteredDevice();
}

}  // namespace tether

}  // namespace chromeos
