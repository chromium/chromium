// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/foreground_eid_generator.h"

#include <memory>

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "chromeos/components/multidevice/remote_device_ref.h"
#include "chromeos/services/device_sync/proto/cryptauth_api.pb.h"
#include "chromeos/services/secure_channel/raw_eid_generator_impl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::AtLeast;
using testing::NiceMock;
using testing::Return;
using testing::SaveArg;
using testing::StrictMock;

namespace chromeos {

namespace secure_channel {

namespace {

// These constants could be made as integer amounts of milliseconds by calling
// .InMilliseconds(), but this would create a static initializer because there
// is no constexpr implementation of TimeDelta::InMilliseconds() yet. Static
// initializers are not a big problem in tests, but it is preferable to avoid
// them here for consistensy with similar definitions going into release
// binaries.
constexpr base::TimeDelta kEidPeriod = base::TimeDelta::FromHours(8);
constexpr base::TimeDelta kEidSeedPeriod = base::TimeDelta::FromDays(14);

const int32_t kNumBytesInEidValue = 2;

// Midnight on 1/1/2020.
const int64_t kDefaultCurrentPeriodStart = 1577836800000L;
// 1:43am on 1/1/2020.
const int64_t kDefaultCurrentTime = 1577843000000L;

// The Base64 encoded values of these raw data strings are, respectively:
// "Zmlyc3RTZWVk", "c2Vjb25kU2VlZA==", "dGhpcmRTZWVk","Zm91cnRoU2VlZA==".
const std::string kFirstSeed = "firstSeed";
const std::string kSecondSeed = "secondSeed";
const std::string kThirdSeed = "thirdSeed";
const std::string kFourthSeed = "fourthSeed";

const std::string kDefaultAdvertisingDevicePublicKey = "publicKey";

cryptauth::BeaconSeed CreateBeaconSeed(const std::string& data,
                                       int64_t start_timestamp_ms,
                                       int64_t end_timestamp_ms) {
  cryptauth::BeaconSeed seed;
  seed.set_data(data);
  seed.set_start_time_millis(start_timestamp_ms);
  seed.set_end_time_millis(end_timestamp_ms);
  return seed;
}

std::string GenerateFakeEidData(const std::string& eid_seed,
                                int64_t start_of_period_timestamp_ms,
                                const std::string* extra_entropy) {
  std::hash<std::string> string_hash;
  int64_t seed_hash = string_hash(eid_seed);
  int64_t extra_hash = extra_entropy ? string_hash(*extra_entropy) : 0;
  int64_t fake_data_xor = seed_hash ^ start_of_period_timestamp_ms ^ extra_hash;

  std::string fake_data(reinterpret_cast<const char*>(&fake_data_xor),
                        sizeof(fake_data_xor));
  fake_data.resize(kNumBytesInEidValue);

  return fake_data;
}

std::string GenerateFakeAdvertisement(
    const std::string& scanning_device_eid_seed,
    int64_t start_of_period_timestamp_ms,
    const std::string& advertising_device_public_key) {
  std::string fake_scanning_eid = GenerateFakeEidData(
      scanning_device_eid_seed, start_of_period_timestamp_ms, nullptr);
  std::string fake_advertising_id = GenerateFakeEidData(
      scanning_device_eid_seed, start_of_period_timestamp_ms,
      &advertising_device_public_key);
  std::string fake_advertisement;
  fake_advertisement.append(fake_scanning_eid);
  fake_advertisement.append(fake_advertising_id);
  return fake_advertisement;
}

}  //  namespace

class SecureChannelForegroundEidGeneratorTest : public testing::Test {
 protected:
  SecureChannelForegroundEidGeneratorTest() {
    scanning_device_beacon_seeds_.push_back(CreateBeaconSeed(
        kFirstSeed,
        kDefaultCurrentPeriodStart - kEidSeedPeriod.InMilliseconds(),
        kDefaultCurrentPeriodStart));
    scanning_device_beacon_seeds_.push_back(CreateBeaconSeed(
        kSecondSeed, kDefaultCurrentPeriodStart,
        kDefaultCurrentPeriodStart + kEidSeedPeriod.InMilliseconds()));
    scanning_device_beacon_seeds_.push_back(CreateBeaconSeed(
        kThirdSeed,
        kDefaultCurrentPeriodStart + kEidSeedPeriod.InMilliseconds(),
        kDefaultCurrentPeriodStart + 2 * kEidSeedPeriod.InMilliseconds()));
    scanning_device_beacon_seeds_.push_back(CreateBeaconSeed(
        kFourthSeed,
        kDefaultCurrentPeriodStart + 2 * kEidSeedPeriod.InMilliseconds(),
        kDefaultCurrentPeriodStart + 3 * kEidSeedPeriod.InMilliseconds()));
  }

  class TestRawEidGenerator : public RawEidGenerator {
   public:
    TestRawEidGenerator() {}
    ~TestRawEidGenerator() override {}

    // RawEidGenerator:
    std::string GenerateEid(const std::string& eid_seed,
                            int64_t start_of_period_timestamp_ms,
                            std::string const* extra_entropy) override {
      return GenerateFakeEidData(eid_seed, start_of_period_timestamp_ms,
                                 extra_entropy);
    }
  };

  void SetUp() override {
    SetTestTime(kDefaultCurrentTime);

    eid_generator_.reset(new ForegroundEidGenerator(
        std::make_unique<TestRawEidGenerator>(), &test_clock_));
  }

  // TODO(khorimoto): Is there an easier way to do this?
  void SetTestTime(int64_t timestamp_ms) {
    base::Time time = base::Time::UnixEpoch() +
                      base::TimeDelta::FromMilliseconds(timestamp_ms);
    test_clock_.SetNow(time);
  }

  std::unique_ptr<ForegroundEidGenerator> eid_generator_;
  base::SimpleTestClock test_clock_;
  std::vector<cryptauth::BeaconSeed> scanning_device_beacon_seeds_;
};

TEST_F(SecureChannelForegroundEidGeneratorTest,
       GenerateBackgroundScanFilter_StartOfPeriod_AnotherSeedInPreviousPeriod) {
  SetTestTime(kDefaultCurrentTime);

  std::unique_ptr<ForegroundEidGenerator::EidData> data =
      eid_generator_->GenerateBackgroundScanFilter(
          scanning_device_beacon_seeds_);
  ASSERT_TRUE(data);
  EXPECT_EQ(data->GetAdjacentDataType(),
            ForegroundEidGenerator::EidData::AdjacentDataType::PAST);

  EXPECT_EQ(kDefaultCurrentPeriodStart, data->current_data.start_timestamp_ms);
  EXPECT_EQ(kDefaultCurrentPeriodStart + kEidPeriod.InMilliseconds(),
            data->current_data.end_timestamp_ms);
  EXPECT_EQ(
      GenerateFakeEidData(kSecondSeed, kDefaultCurrentPeriodStart, nullptr),
      data->current_data.data);

  ASSERT_TRUE(data->adjacent_data);
  EXPECT_EQ(kDefaultCurrentPeriodStart - kEidPeriod.InMilliseconds(),
            data->adjacent_data->start_timestamp_ms);
  EXPECT_EQ(kDefaultCurrentPeriodStart, data->adjacent_data->end_timestamp_ms);
  EXPECT_EQ(
      GenerateFakeEidData(
          kFirstSeed, kDefaultCurrentPeriodStart - kEidPeriod.InMilliseconds(),
          nullptr),
      data->adjacent_data->data);
}

TEST_F(SecureChannelForegroundEidGeneratorTest,
       GenerateBackgroundScanFilter_StartOfPeriod_NoSeedBefore) {
  SetTestTime(kDefaultCurrentTime - kEidSeedPeriod.InMilliseconds());

  std::unique_ptr<ForegroundEidGenerator::EidData> data =
      eid_generator_->GenerateBackgroundScanFilter(
          scanning_device_beacon_seeds_);
  ASSERT_TRUE(data);
  EXPECT_EQ(data->GetAdjacentDataType(),
            ForegroundEidGenerator::EidData::AdjacentDataType::NONE);

  EXPECT_EQ(kDefaultCurrentPeriodStart - kEidSeedPeriod.InMilliseconds(),
            data->current_data.start_timestamp_ms);
  EXPECT_EQ(kDefaultCurrentPeriodStart - kEidSeedPeriod.InMilliseconds() +
                kEidPeriod.InMilliseconds(),
            data->current_data.end_timestamp_ms);
  EXPECT_EQ(GenerateFakeEidData(
                kFirstSeed,
                kDefaultCurrentPeriodStart - kEidSeedPeriod.InMilliseconds(),
                nullptr),
            data->current_data.data);

  EXPECT_FALSE(data->adjacent_data);
}

TEST_F(SecureChannelForegroundEidGeneratorTest,
       GenerateBackgroundScanFilter_PastStartOfPeriod) {
  SetTestTime(kDefaultCurrentTime +
              base::TimeDelta::FromHours(3).InMilliseconds());

  std::unique_ptr<ForegroundEidGenerator::EidData> data =
      eid_generator_->GenerateBackgroundScanFilter(
          scanning_device_beacon_seeds_);
  ASSERT_TRUE(data);
  EXPECT_EQ(data->GetAdjacentDataType(),
            ForegroundEidGenerator::EidData::AdjacentDataType::FUTURE);

  EXPECT_EQ(kDefaultCurrentPeriodStart, data->current_data.start_timestamp_ms);
  EXPECT_EQ(kDefaultCurrentPeriodStart + kEidPeriod.InMilliseconds(),
            data->current_data.end_timestamp_ms);
  EXPECT_EQ(
      GenerateFakeEidData(kSecondSeed, kDefaultCurrentPeriodStart, nullptr),
      data->current_data.data);

  ASSERT_TRUE(data->adjacent_data);
  EXPECT_EQ(kDefaultCurrentPeriodStart + kEidPeriod.InMilliseconds(),
            data->adjacent_data->start_timestamp_ms);
  EXPECT_EQ(kDefaultCurrentPeriodStart + 2 * kEidPeriod.InMilliseconds(),
            data->adjacent_data->end_timestamp_ms);
  EXPECT_EQ(
      GenerateFakeEidData(
          kSecondSeed, kDefaultCurrentPeriodStart + kEidPeriod.InMilliseconds(),
          nullptr),
      data->adjacent_data->data);
}

TEST_F(SecureChannelForegroundEidGeneratorTest,
       GenerateBackgroundScanFilter_EndOfPeriod) {
  SetTestTime(kDefaultCurrentPeriodStart + kEidSeedPeriod.InMilliseconds() - 1);

  std::unique_ptr<ForegroundEidGenerator::EidData> data =
      eid_generator_->GenerateBackgroundScanFilter(
          scanning_device_beacon_seeds_);
  ASSERT_TRUE(data);
  EXPECT_EQ(data->GetAdjacentDataType(),
            ForegroundEidGenerator::EidData::AdjacentDataType::FUTURE);

  EXPECT_EQ(kDefaultCurrentPeriodStart + kEidSeedPeriod.InMilliseconds() -
                kEidPeriod.InMilliseconds(),
            data->current_data.start_timestamp_ms);
  EXPECT_EQ(kDefaultCurrentPeriodStart + kEidSeedPeriod.InMilliseconds(),
            data->current_data.end_timestamp_ms);
  EXPECT_EQ(GenerateFakeEidData(kSecondSeed,
                                kDefaultCurrentPeriodStart +
                                    kEidSeedPeriod.InMilliseconds() -
                                    kEidPeriod.InMilliseconds(),
                                nullptr),
            data->current_data.data);

  ASSERT_TRUE(data->adjacent_data);
  EXPECT_EQ(kDefaultCurrentPeriodStart + kEidSeedPeriod.InMilliseconds(),
            data->adjacent_data->start_timestamp_ms);
  EXPECT_EQ(kDefaultCurrentPeriodStart + kEidSeedPeriod.InMilliseconds() +
                kEidPeriod.InMilliseconds(),
            data->adjacent_data->end_timestamp_ms);
  EXPECT_EQ(GenerateFakeEidData(
                kThirdSeed,
                kDefaultCurrentPeriodStart + kEidSeedPeriod.InMilliseconds(),
                nullptr),
            data->adjacent_data->data);
}

TEST_F(SecureChannelForegroundEidGeneratorTest,
       GenerateBackgroundScanFilter_EndOfPeriod_NoSeedAfter) {
  SetTestTime(kDefaultCurrentPeriodStart + 3 * kEidSeedPeriod.InMilliseconds() -
              1);

  std::unique_ptr<ForegroundEidGenerator::EidData> data =
      eid_generator_->GenerateBackgroundScanFilter(
          scanning_device_beacon_seeds_);
  ASSERT_TRUE(data);
  EXPECT_EQ(data->GetAdjacentDataType(),
            ForegroundEidGenerator::EidData::AdjacentDataType::NONE);

  EXPECT_EQ(kDefaultCurrentPeriodStart + 3 * kEidSeedPeriod.InMilliseconds() -
                kEidPeriod.InMilliseconds(),
            data->current_data.start_timestamp_ms);
  EXPECT_EQ(kDefaultCurrentPeriodStart + 3 * kEidSeedPeriod.InMilliseconds(),
            data->current_data.end_timestamp_ms);
  EXPECT_EQ(GenerateFakeEidData(kFourthSeed,
                                kDefaultCurrentPeriodStart +
                                    3 * kEidSeedPeriod.InMilliseconds() -
                                    kEidPeriod.InMilliseconds(),
                                nullptr),
            data->current_data.data);

  EXPECT_FALSE(data->adjacent_data);
}

TEST_F(SecureChannelForegroundEidGeneratorTest,
       GenerateBackgroundScanFilter_NoCurrentPeriodSeed) {
  SetTestTime(kDefaultCurrentPeriodStart + 4 * kEidSeedPeriod.InMilliseconds() -
              1);

  std::unique_ptr<ForegroundEidGenerator::EidData> data =
      eid_generator_->GenerateBackgroundScanFilter(
          scanning_device_beacon_seeds_);
  EXPECT_FALSE(data);
}

TEST_F(SecureChannelForegroundEidGeneratorTest,
       GenerateBackgroundScanFilter_EmptySeeds) {
  SetTestTime(kDefaultCurrentTime);

  std::vector<cryptauth::BeaconSeed> empty;
  std::unique_ptr<ForegroundEidGenerator::EidData> data =
      eid_generator_->GenerateBackgroundScanFilter(empty);
  EXPECT_FALSE(data);
}

TEST_F(SecureChannelForegroundEidGeneratorTest,
       GenerateBackgroundScanFilter_InvalidSeed_PeriodNotMultipleOf8Hours) {
  SetTestTime(kDefaultCurrentTime);

  // Seed has a period of 1ms, but it should have a period of 8 hours.
  std::vector<cryptauth::BeaconSeed> invalid_seed_vector = {CreateBeaconSeed(
      kFirstSeed, kDefaultCurrentPeriodStart, kDefaultCurrentPeriodStart + 1)};
  std::unique_ptr<ForegroundEidGenerator::EidData> data =
      eid_generator_->GenerateBackgroundScanFilter(invalid_seed_vector);
  EXPECT_FALSE(data);
}

TEST_F(SecureChannelForegroundEidGeneratorTest,
       GenerateBackgroundScanFilter_UsingRealEids) {
  SetTestTime(kDefaultCurrentTime);

  // Use real RawEidGenerator implementation instead of test version.
  eid_generator_.reset(new ForegroundEidGenerator(
      std::make_unique<RawEidGeneratorImpl>(), &test_clock_));

  std::unique_ptr<ForegroundEidGenerator::EidData> data =
      eid_generator_->GenerateBackgroundScanFilter(
          scanning_device_beacon_seeds_);
  ASSERT_TRUE(data);
  EXPECT_EQ(data->GetAdjacentDataType(),
            ForegroundEidGenerator::EidData::AdjacentDataType::PAST);

  EXPECT_EQ(kDefaultCurrentPeriodStart, data->current_data.start_timestamp_ms);
  EXPECT_EQ(kDefaultCurrentPeriodStart + kEidPeriod.InMilliseconds(),
            data->current_data.end_timestamp_ms);
  // Since this uses the real RawEidGenerator, just make sure the data
  // exists and has the proper length.
  EXPECT_EQ((size_t)kNumBytesInEidValue, data->current_data.data.length());

  ASSERT_TRUE(data->adjacent_data);
  EXPECT_EQ(kDefaultCurrentPeriodStart - kEidPeriod.InMilliseconds(),
            data->adjacent_data->start_timestamp_ms);
  EXPECT_EQ(kDefaultCurrentPeriodStart, data->adjacent_data->end_timestamp_ms);
  // Since this uses the real RawEidGenerator, just make sure the data
  // exists and has the proper length.
  EXPECT_EQ((size_t)kNumBytesInEidValue, data->adjacent_data->data.length());
}

TEST_F(SecureChannelForegroundEidGeneratorTest, GenerateAdvertisementData) {
  SetTestTime(kDefaultCurrentTime);

  std::unique_ptr<DataWithTimestamp> data =
      eid_generator_->GenerateAdvertisement(kDefaultAdvertisingDevicePublicKey,
                                            scanning_device_beacon_seeds_);
  ASSERT_TRUE(data);

  EXPECT_EQ(kDefaultCurrentPeriodStart, data->start_timestamp_ms);
  EXPECT_EQ(kDefaultCurrentPeriodStart + kEidPeriod.InMilliseconds(),
            data->end_timestamp_ms);
  EXPECT_EQ(GenerateFakeAdvertisement(kSecondSeed, kDefaultCurrentPeriodStart,
                                      kDefaultAdvertisingDevicePublicKey),
            data->data);
}

TEST_F(SecureChannelForegroundEidGeneratorTest,
       GenerateAdvertisementData_NoSeedForPeriod) {
  SetTestTime(kDefaultCurrentTime + 4 * kEidSeedPeriod.InMilliseconds());

  std::unique_ptr<DataWithTimestamp> data =
      eid_generator_->GenerateAdvertisement(kDefaultAdvertisingDevicePublicKey,
                                            scanning_device_beacon_seeds_);
  EXPECT_FALSE(data);
}

TEST_F(SecureChannelForegroundEidGeneratorTest,
       GenerateAdvertisementData_EmptySeeds) {
  SetTestTime(kDefaultCurrentTime + 4 * kEidSeedPeriod.InMilliseconds());

  std::vector<cryptauth::BeaconSeed> empty;
  std::unique_ptr<DataWithTimestamp> data =
      eid_generator_->GenerateAdvertisement(kDefaultAdvertisingDevicePublicKey,
                                            empty);
  EXPECT_FALSE(data);
}

TEST_F(SecureChannelForegroundEidGeneratorTest,
       GeneratePossibleAdvertisements_CurrentAndPastAdjacentPeriods) {
  SetTestTime(kDefaultCurrentPeriodStart);

  std::vector<std::string> possible_advertisements =
      eid_generator_->GeneratePossibleAdvertisements(
          kDefaultAdvertisingDevicePublicKey, scanning_device_beacon_seeds_);

  EXPECT_EQ((size_t)2, possible_advertisements.size());
  EXPECT_EQ(GenerateFakeAdvertisement(kSecondSeed, kDefaultCurrentPeriodStart,
                                      kDefaultAdvertisingDevicePublicKey),
            possible_advertisements[0]);
  EXPECT_EQ(
      GenerateFakeAdvertisement(
          kFirstSeed, kDefaultCurrentPeriodStart - kEidPeriod.InMilliseconds(),
          kDefaultAdvertisingDevicePublicKey),
      possible_advertisements[1]);
}

TEST_F(SecureChannelForegroundEidGeneratorTest,
       testGeneratePossibleAdvertisements_CurrentAndFutureAdjacentPeriods) {
  SetTestTime(kDefaultCurrentPeriodStart +
              base::TimeDelta::FromHours(3).InMilliseconds());

  std::vector<std::string> possible_advertisements =
      eid_generator_->GeneratePossibleAdvertisements(
          kDefaultAdvertisingDevicePublicKey, scanning_device_beacon_seeds_);

  EXPECT_EQ((size_t)2, possible_advertisements.size());
  EXPECT_EQ(GenerateFakeAdvertisement(kSecondSeed, kDefaultCurrentPeriodStart,
                                      kDefaultAdvertisingDevicePublicKey),
            possible_advertisements[0]);
  EXPECT_EQ(
      GenerateFakeAdvertisement(
          kSecondSeed, kDefaultCurrentPeriodStart + kEidPeriod.InMilliseconds(),
          kDefaultAdvertisingDevicePublicKey),
      possible_advertisements[1]);
}

TEST_F(SecureChannelForegroundEidGeneratorTest,
       GeneratePossibleAdvertisements_OnlyCurrentPeriod) {
  SetTestTime(kDefaultCurrentPeriodStart - kEidSeedPeriod.InMilliseconds());

  std::vector<std::string> possible_advertisements =
      eid_generator_->GeneratePossibleAdvertisements(
          kDefaultAdvertisingDevicePublicKey, scanning_device_beacon_seeds_);

  EXPECT_EQ((size_t)1, possible_advertisements.size());
  EXPECT_EQ(GenerateFakeAdvertisement(
                kFirstSeed,
                kDefaultCurrentPeriodStart - kEidSeedPeriod.InMilliseconds(),
                kDefaultAdvertisingDevicePublicKey),
            possible_advertisements[0]);
}

TEST_F(SecureChannelForegroundEidGeneratorTest,
       GeneratePossibleAdvertisements_OnlyFuturePeriod) {
  SetTestTime(kDefaultCurrentPeriodStart - kEidSeedPeriod.InMilliseconds() -
              kEidPeriod.InMilliseconds());

  std::vector<std::string> possible_advertisements =
      eid_generator_->GeneratePossibleAdvertisements(
          kDefaultAdvertisingDevicePublicKey, scanning_device_beacon_seeds_);

  EXPECT_EQ((size_t)1, possible_advertisements.size());
  EXPECT_EQ(GenerateFakeAdvertisement(
                kFirstSeed,
                kDefaultCurrentPeriodStart - kEidSeedPeriod.InMilliseconds(),
                kDefaultAdvertisingDevicePublicKey),
            possible_advertisements[0]);
}

TEST_F(SecureChannelForegroundEidGeneratorTest,
       GeneratePossibleAdvertisements_NoAdvertisements_SeedsTooFarInFuture) {
  SetTestTime(kDefaultCurrentPeriodStart - kEidSeedPeriod.InMilliseconds() -
              kEidPeriod.InMilliseconds() - 1);

  std::vector<std::string> possible_advertisements =
      eid_generator_->GeneratePossibleAdvertisements(
          kDefaultAdvertisingDevicePublicKey, scanning_device_beacon_seeds_);
  EXPECT_TRUE(possible_advertisements.empty());
}

TEST_F(SecureChannelForegroundEidGeneratorTest,
       GeneratePossibleAdvertisements_OnlyPastPeriod) {
  SetTestTime(kDefaultCurrentPeriodStart + 3 * kEidSeedPeriod.InMilliseconds() +
              kEidPeriod.InMilliseconds());

  std::vector<std::string> possible_advertisements =
      eid_generator_->GeneratePossibleAdvertisements(
          kDefaultAdvertisingDevicePublicKey, scanning_device_beacon_seeds_);

  EXPECT_EQ((size_t)1, possible_advertisements.size());
  EXPECT_EQ(GenerateFakeAdvertisement(kFourthSeed,
                                      kDefaultCurrentPeriodStart +
                                          3 * kEidSeedPeriod.InMilliseconds() -
                                          kEidPeriod.InMilliseconds(),
                                      kDefaultAdvertisingDevicePublicKey),
            possible_advertisements[0]);
}

TEST_F(SecureChannelForegroundEidGeneratorTest,
       GeneratePossibleAdvertisements_NoAdvertisements_SeedsTooFarInPast) {
  SetTestTime(kDefaultCurrentPeriodStart + 3 * kEidSeedPeriod.InMilliseconds() +
              kEidPeriod.InMilliseconds() + 1);

  std::vector<std::string> possible_advertisements =
      eid_generator_->GeneratePossibleAdvertisements(
          kDefaultAdvertisingDevicePublicKey, scanning_device_beacon_seeds_);
  EXPECT_TRUE(possible_advertisements.empty());
}

TEST_F(SecureChannelForegroundEidGeneratorTest,
       GeneratePossibleAdvertisements_NoAdvertisements_EmptySeeds) {
  SetTestTime(kDefaultCurrentPeriodStart);

  std::vector<cryptauth::BeaconSeed> empty;
  std::vector<std::string> possible_advertisements =
      eid_generator_->GeneratePossibleAdvertisements(
          kDefaultAdvertisingDevicePublicKey, empty);
  EXPECT_TRUE(possible_advertisements.empty());
}

TEST_F(SecureChannelForegroundEidGeneratorTest,
       IdentifyRemoteDevice_NoDevices) {
  SetTestTime(kDefaultCurrentPeriodStart);

  std::string service_data =
      GenerateFakeAdvertisement(kSecondSeed, kDefaultCurrentPeriodStart,
                                kDefaultAdvertisingDevicePublicKey);

  std::vector<std::string> device_id_list;
  const std::string identified_device_id =
      eid_generator_->IdentifyRemoteDeviceByAdvertisement(
          service_data, device_id_list, scanning_device_beacon_seeds_);
  EXPECT_TRUE(identified_device_id.empty());
}

TEST_F(SecureChannelForegroundEidGeneratorTest,
       IdentifyRemoteDevice_OneDevice_Success) {
  SetTestTime(kDefaultCurrentPeriodStart);

  std::string service_data =
      GenerateFakeAdvertisement(kSecondSeed, kDefaultCurrentPeriodStart,
                                kDefaultAdvertisingDevicePublicKey);

  std::string device_id = multidevice::RemoteDevice::GenerateDeviceId(
      kDefaultAdvertisingDevicePublicKey);
  std::vector<std::string> device_id_list = {device_id};
  std::string identified_device_id =
      eid_generator_->IdentifyRemoteDeviceByAdvertisement(
          service_data, device_id_list, scanning_device_beacon_seeds_);
  EXPECT_EQ(device_id, identified_device_id);
}

TEST_F(SecureChannelForegroundEidGeneratorTest,
       IdentifyRemoteDevice_OneDevice_ServiceDataWithOneByteFlag_Success) {
  SetTestTime(kDefaultCurrentPeriodStart);

  std::string service_data =
      GenerateFakeAdvertisement(kSecondSeed, kDefaultCurrentPeriodStart,
                                kDefaultAdvertisingDevicePublicKey);

  // Identifying device should still succeed if there is an extra "flag" byte
  // after the first 4 bytes.
  service_data.append(
      1, static_cast<char>(ForegroundEidGenerator::kBluetooth4Flag));

  std::string device_id = multidevice::RemoteDevice::GenerateDeviceId(
      kDefaultAdvertisingDevicePublicKey);
  std::vector<std::string> device_id_list = {device_id};
  std::string identified_device_id =
      eid_generator_->IdentifyRemoteDeviceByAdvertisement(
          service_data, device_id_list, scanning_device_beacon_seeds_);
  EXPECT_EQ(device_id, identified_device_id);
}

TEST_F(SecureChannelForegroundEidGeneratorTest,
       IdentifyRemoteDevice_OneDevice_ServiceDataWithLongerFlag_Success) {
  SetTestTime(kDefaultCurrentPeriodStart);

  std::string service_data =
      GenerateFakeAdvertisement(kSecondSeed, kDefaultCurrentPeriodStart,
                                kDefaultAdvertisingDevicePublicKey);

  // Identifying device should still succeed if there are extra "flag" bytes
  // after the first 4 bytes.
  service_data.append("extra_flag_bytes");

  std::string device_id = multidevice::RemoteDevice::GenerateDeviceId(
      kDefaultAdvertisingDevicePublicKey);
  std::vector<std::string> device_id_list = {device_id};
  std::string identified_device_id =
      eid_generator_->IdentifyRemoteDeviceByAdvertisement(
          service_data, device_id_list, scanning_device_beacon_seeds_);
  EXPECT_EQ(device_id, identified_device_id);
}

TEST_F(SecureChannelForegroundEidGeneratorTest,
       IdentifyRemoteDevice_OneDevice_Failure) {
  SetTestTime(kDefaultCurrentPeriodStart);

  std::string service_data =
      GenerateFakeAdvertisement(kSecondSeed, kDefaultCurrentPeriodStart,
                                kDefaultAdvertisingDevicePublicKey);

  std::vector<std::string> device_id_list = {"wrongDeviceId"};
  std::string identified_device_id =
      eid_generator_->IdentifyRemoteDeviceByAdvertisement(
          service_data, device_id_list, scanning_device_beacon_seeds_);
  EXPECT_TRUE(identified_device_id.empty());
}

TEST_F(SecureChannelForegroundEidGeneratorTest,
       IdentifyRemoteDevice_MultipleDevices_Success) {
  SetTestTime(kDefaultCurrentPeriodStart);

  std::string service_data =
      GenerateFakeAdvertisement(kSecondSeed, kDefaultCurrentPeriodStart,
                                kDefaultAdvertisingDevicePublicKey);

  std::string device_id = multidevice::RemoteDevice::GenerateDeviceId(
      kDefaultAdvertisingDevicePublicKey);
  std::vector<std::string> device_id_list = {device_id, "wrongDeviceId"};
  std::string identified_device_id =
      eid_generator_->IdentifyRemoteDeviceByAdvertisement(
          service_data, device_id_list, scanning_device_beacon_seeds_);
  EXPECT_EQ(device_id, identified_device_id);
}

TEST_F(SecureChannelForegroundEidGeneratorTest,
       IdentifyRemoteDevice_MultipleDevices_Failure) {
  SetTestTime(kDefaultCurrentPeriodStart);

  std::string service_data =
      GenerateFakeAdvertisement(kSecondSeed, kDefaultCurrentPeriodStart,
                                kDefaultAdvertisingDevicePublicKey);

  std::vector<std::string> device_id_list = {"wrongDeviceId", "wrongDeviceId"};
  std::string identified_device_id =
      eid_generator_->IdentifyRemoteDeviceByAdvertisement(
          service_data, device_id_list, scanning_device_beacon_seeds_);
  EXPECT_TRUE(identified_device_id.empty());
}

TEST_F(SecureChannelForegroundEidGeneratorTest,
       DataWithTimestamp_ContainsTime) {
  DataWithTimestamp data_with_timestamp("data", /* start */ 1000L,
                                        /* end */ 2000L);
  EXPECT_FALSE(data_with_timestamp.ContainsTime(999L));
  EXPECT_TRUE(data_with_timestamp.ContainsTime(1000L));
  EXPECT_TRUE(data_with_timestamp.ContainsTime(1500L));
  EXPECT_TRUE(data_with_timestamp.ContainsTime(1999L));
  EXPECT_FALSE(data_with_timestamp.ContainsTime(2000L));
}

}  // namespace secure_channel

}  // namespace chromeos
