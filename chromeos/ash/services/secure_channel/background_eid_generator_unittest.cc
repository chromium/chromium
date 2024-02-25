// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/background_eid_generator.h"

#include <memory>
#include <string>

#include "base/strings/string_util.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "chromeos/ash/components/multidevice/beacon_seed.h"
#include "chromeos/ash/components/multidevice/remote_device_ref.h"
#include "chromeos/ash/components/multidevice/remote_device_test_util.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_api.pb.h"
#include "chromeos/ash/services/secure_channel/data_with_timestamp.h"
#include "chromeos/ash/services/secure_channel/raw_eid_generator_impl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::secure_channel {

namespace {

const int64_t kEidPeriodMs = base::Minutes(15).InMilliseconds();
const int64_t kBeaconSeedDurationMs = base::Days(14).InMilliseconds();

// The number of nearest EIDs returned by GenerateNearestEids().
const size_t kEidCount = 5;

// Midnight on 1/1/2020.
const int64_t kStartPeriodMs = 1577836800000L;
// 1:43am on 1/1/2020.
const int64_t kCurrentTimeMs = 1577843000000L;

const std::string kFirstSeed = "firstSeed";
const std::string kSecondSeed = "secondSeed";
const std::string kThirdSeed = "thirdSeed";
const std::string kFourthSeed = "fourthSeed";

cryptauth::BeaconSeed CreateBeaconSeed(const std::string& data,
                                       const int64_t start_timestamp_ms,
                                       const int64_t end_timestamp_ms) {
  cryptauth::BeaconSeed seed;
  seed.set_data(data);
  seed.set_start_time_millis(start_timestamp_ms);
  seed.set_end_time_millis(end_timestamp_ms);
  return seed;
}

DataWithTimestamp CreateDataWithTimestamp(
    const std::string& eid_seed,
    int64_t start_of_period_timestamp_ms) {
  std::unique_ptr<RawEidGenerator> raw_eid_generator =
      std::make_unique<RawEidGeneratorImpl>();
  std::string data = raw_eid_generator->GenerateEid(
      eid_seed, start_of_period_timestamp_ms, nullptr /* extra_entropy */);
  return DataWithTimestamp(data, start_of_period_timestamp_ms,
                           start_of_period_timestamp_ms + kEidPeriodMs);
}

class TestRawEidGenerator : public RawEidGeneratorImpl {
 public:
  TestRawEidGenerator() {}

  TestRawEidGenerator(const TestRawEidGenerator&) = delete;
  TestRawEidGenerator& operator=(const TestRawEidGenerator&) = delete;

  ~TestRawEidGenerator() override {}

  // RawEidGenerator:
  std::string GenerateEid(const std::string& eid_seed,
                          int64_t start_of_period_timestamp_ms,
                          std::string const* extra_entropy) override {
    EXPECT_FALSE(extra_entropy);
    return RawEidGeneratorImpl::GenerateEid(
        eid_seed, start_of_period_timestamp_ms, extra_entropy);
  }
};

}  //  namespace

class SecureChannelBackgroundEidGeneratorTest : public testing::Test {
 protected:
  SecureChannelBackgroundEidGeneratorTest() {
    beacon_seeds_.push_back(CreateBeaconSeed(
        kFirstSeed, kStartPeriodMs - kBeaconSeedDurationMs, kStartPeriodMs));
    beacon_seeds_.push_back(CreateBeaconSeed(
        kSecondSeed, kStartPeriodMs, kStartPeriodMs + kBeaconSeedDurationMs));
    beacon_seeds_.push_back(
        CreateBeaconSeed(kThirdSeed, kStartPeriodMs + kBeaconSeedDurationMs,
                         kStartPeriodMs + 2 * kBeaconSeedDurationMs));
    beacon_seeds_.push_back(CreateBeaconSeed(
        kFourthSeed, kStartPeriodMs + 2 * kBeaconSeedDurationMs,
        kStartPeriodMs + 3 * kBeaconSeedDurationMs));

    multidevice::RemoteDeviceRef device_1 =
        multidevice::RemoteDeviceRefBuilder()
            .SetPublicKey("publicKey1")
            .SetBeaconSeeds(multidevice::FromCryptAuthSeedList(beacon_seeds_))
            .Build();
    multidevice::RemoteDeviceRef device_2 =
        multidevice::RemoteDeviceRefBuilder()
            .SetPublicKey("publicKey2")
            .Build();
    test_remote_devices_ = {device_1, device_2};
  }

  void SetUp() override {
    SetTestTime(kCurrentTimeMs);

    eid_generator_.reset(new BackgroundEidGenerator(
        std::make_unique<TestRawEidGenerator>(), &test_clock_));
  }

  void SetTestTime(int64_t timestamp_ms) {
    base::Time time =
        base::Time::UnixEpoch() + base::Milliseconds(timestamp_ms);
    test_clock_.SetNow(time);
  }

  std::unique_ptr<BackgroundEidGenerator> eid_generator_;
  base::SimpleTestClock test_clock_;
  std::vector<cryptauth::BeaconSeed> beacon_seeds_;
  multidevice::RemoteDeviceRefList test_remote_devices_;
};

TEST_F(SecureChannelBackgroundEidGeneratorTest,
       GenerateNearestEids_BeaconSeedsExpired) {
  SetTestTime(beacon_seeds_[beacon_seeds_.size() - 1].end_time_millis() +
              kEidCount * kEidPeriodMs);
  std::vector<DataWithTimestamp> eids =
      eid_generator_->GenerateNearestEids(beacon_seeds_);
  EXPECT_EQ(0u, eids.size());
}

TEST_F(SecureChannelBackgroundEidGeneratorTest,
       GenerateNearestEids_BeaconSeedsValidInFuture) {
  SetTestTime(beacon_seeds_[0].start_time_millis() - kEidCount * kEidPeriodMs);
  std::vector<DataWithTimestamp> eids =
      eid_generator_->GenerateNearestEids(beacon_seeds_);
  EXPECT_EQ(0u, eids.size());
}

TEST_F(SecureChannelBackgroundEidGeneratorTest,
       GenerateNearestEids_EidsUseSameBeaconSeed) {
  int64_t start_period_ms =
      beacon_seeds_[0].start_time_millis() + kEidCount * kEidPeriodMs;
  SetTestTime(start_period_ms + kEidPeriodMs / 2);

  std::vector<DataWithTimestamp> eids =
      eid_generator_->GenerateNearestEids(beacon_seeds_);

  std::string seed = beacon_seeds_[0].data();
  EXPECT_EQ(kEidCount, eids.size());
  EXPECT_EQ(CreateDataWithTimestamp(seed, start_period_ms - 2 * kEidPeriodMs),
            eids[0]);
  EXPECT_EQ(CreateDataWithTimestamp(seed, start_period_ms - 1 * kEidPeriodMs),
            eids[1]);
  EXPECT_EQ(CreateDataWithTimestamp(seed, start_period_ms + 0 * kEidPeriodMs),
            eids[2]);
  EXPECT_EQ(CreateDataWithTimestamp(seed, start_period_ms + 1 * kEidPeriodMs),
            eids[3]);
  EXPECT_EQ(CreateDataWithTimestamp(seed, start_period_ms + 2 * kEidPeriodMs),
            eids[4]);
}

TEST_F(SecureChannelBackgroundEidGeneratorTest,
       GenerateNearestEids_EidsAcrossBeaconSeeds) {
  int64_t end_period_ms = beacon_seeds_[0].end_time_millis();
  int64_t start_period_ms = beacon_seeds_[1].start_time_millis();
  SetTestTime(start_period_ms + kEidPeriodMs / 2);

  std::vector<DataWithTimestamp> eids =
      eid_generator_->GenerateNearestEids(beacon_seeds_);

  std::string seed0 = beacon_seeds_[0].data();
  std::string seed1 = beacon_seeds_[1].data();
  EXPECT_EQ(kEidCount, eids.size());
  EXPECT_EQ(CreateDataWithTimestamp(seed0, end_period_ms - 2 * kEidPeriodMs),
            eids[0]);
  EXPECT_EQ(CreateDataWithTimestamp(seed0, end_period_ms - 1 * kEidPeriodMs),
            eids[1]);
  EXPECT_EQ(CreateDataWithTimestamp(seed1, start_period_ms + 0 * kEidPeriodMs),
            eids[2]);
  EXPECT_EQ(CreateDataWithTimestamp(seed1, start_period_ms + 1 * kEidPeriodMs),
            eids[3]);
  EXPECT_EQ(CreateDataWithTimestamp(seed1, start_period_ms + 2 * kEidPeriodMs),
            eids[4]);
}

TEST_F(SecureChannelBackgroundEidGeneratorTest,
       GenerateNearestEids_CurrentTimeAtStartOfRange) {
  int64_t start_period_ms = beacon_seeds_[0].start_time_millis();
  SetTestTime(start_period_ms + kEidPeriodMs / 2);

  std::vector<DataWithTimestamp> eids =
      eid_generator_->GenerateNearestEids(beacon_seeds_);

  std::string seed = beacon_seeds_[0].data();
  EXPECT_EQ(3u, eids.size());
  EXPECT_EQ(CreateDataWithTimestamp(seed, start_period_ms + 0 * kEidPeriodMs),
            eids[0]);
  EXPECT_EQ(CreateDataWithTimestamp(seed, start_period_ms + 1 * kEidPeriodMs),
            eids[1]);
  EXPECT_EQ(CreateDataWithTimestamp(seed, start_period_ms + 2 * kEidPeriodMs),
            eids[2]);
}

TEST_F(SecureChannelBackgroundEidGeneratorTest,
       GenerateNearestEids_CurrentTimeAtEndOfRange) {
  int64_t start_period_ms = beacon_seeds_[3].end_time_millis() - kEidPeriodMs;
  SetTestTime(start_period_ms + kEidPeriodMs / 2);

  std::vector<DataWithTimestamp> eids =
      eid_generator_->GenerateNearestEids(beacon_seeds_);

  std::string seed = beacon_seeds_[3].data();
  EXPECT_EQ(3u, eids.size());
  EXPECT_EQ(CreateDataWithTimestamp(seed, start_period_ms - 2 * kEidPeriodMs),
            eids[0]);
  EXPECT_EQ(CreateDataWithTimestamp(seed, start_period_ms - 1 * kEidPeriodMs),
            eids[1]);
  EXPECT_EQ(CreateDataWithTimestamp(seed, start_period_ms - 0 * kEidPeriodMs),
            eids[2]);
}

// Test the case where the account has other devices, but their beacon seeds
// don't match the incoming advertisement. |beacon_seeds_[0]| corresponds to
// |test_remote_devices_[1]|. Since |test_remote_devices_[1]| is not present in
// the device ids passed to IdentifyRemoteDeviceByAdvertisement(), no match is
// expected to be found.
TEST_F(SecureChannelBackgroundEidGeneratorTest,
       IdentifyRemoteDeviceByAdvertisement_NoMatchingRemoteDevices) {
  SetTestTime(kStartPeriodMs + kEidPeriodMs / 2);
  DataWithTimestamp advertisement_eid = CreateDataWithTimestamp(
      beacon_seeds_[0].data(), kStartPeriodMs - kEidPeriodMs);

  EXPECT_EQ(std::string(),
            eid_generator_->IdentifyRemoteDeviceByAdvertisement(
                advertisement_eid.data, {test_remote_devices_[1]}));
}

TEST_F(SecureChannelBackgroundEidGeneratorTest,
       IdentifyRemoteDeviceByAdvertisement_Success) {
  SetTestTime(kStartPeriodMs + kEidPeriodMs / 2);
  DataWithTimestamp advertisement_eid = CreateDataWithTimestamp(
      beacon_seeds_[0].data(), kStartPeriodMs - kEidPeriodMs);

  EXPECT_EQ(test_remote_devices_[0].GetDeviceId(),
            eid_generator_->IdentifyRemoteDeviceByAdvertisement(
                advertisement_eid.data, test_remote_devices_));
}

}  // namespace ash::secure_channel
