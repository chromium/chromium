// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/device_activity/daily_use_case_impl.h"

#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "chromeos/ash/components/device_activity/device_activity_controller.h"
#include "chromeos/ash/components/device_activity/fake_psm_delegate.h"
#include "chromeos/ash/components/device_activity/fresnel_pref_names.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "components/prefs/testing_pref_service.h"
#include "components/version_info/channel.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/private_membership/src/private_membership_rlwe_client.h"

namespace ash::device_activity {

namespace psm_rlwe = private_membership::rlwe;

namespace {

// This secret should be of exactly length 64, since it is a 256 bit string
// encoded as a hexadecimal.
constexpr char kFakePsmDeviceActiveSecret[] =
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";

constexpr char kHardwareClassKeyNotFound[] = "HARDWARE_CLASS_KEY_NOT_FOUND";

constexpr ChromeDeviceMetadataParameters kFakeChromeParameters = {
    version_info::Channel::STABLE /* chromeos_channel */,
    MarketSegment::MARKET_SEGMENT_UNKNOWN /* market_segment */,
};

}  // namespace

class DailyUseCaseImplTest : public testing::Test {
 public:
  DailyUseCaseImplTest() = default;
  DailyUseCaseImplTest(const DailyUseCaseImplTest&) = delete;
  DailyUseCaseImplTest& operator=(const DailyUseCaseImplTest&) = delete;
  ~DailyUseCaseImplTest() override = default;

 protected:
  // testing::Test:
  void SetUp() override {
    DeviceActivityController::RegisterPrefs(local_state_.registry());

    system::StatisticsProvider::SetTestProvider(&statistics_provider_);

    const std::vector<psm_rlwe::RlwePlaintextId> plaintext_ids;
    daily_use_case_impl_ = std::make_unique<DailyUseCaseImpl>(
        kFakePsmDeviceActiveSecret, kFakeChromeParameters, &local_state_,
        // |FakePsmDelegate| can use any test case parameters.
        std::make_unique<FakePsmDelegate>(std::string() /* ec_cipher_key */,
                                          std::string() /* seed */,
                                          std::move(plaintext_ids)));
  }

  void TearDown() override { daily_use_case_impl_.reset(); }

  std::unique_ptr<DailyUseCaseImpl> daily_use_case_impl_;

  // Fake pref service for unit testing the local state.
  TestingPrefServiceSimple local_state_;
  system::FakeStatisticsProvider statistics_provider_;
};

TEST_F(DailyUseCaseImplTest, ValidateWindowIdFormattedCorrectly) {
  // Create fixed timestamp used to generate a fixed window identifier.
  base::Time new_daily_ts;
  EXPECT_TRUE(
      base::Time::FromString("01 Jan 2022 23:59:59 GMT", &new_daily_ts));

  std::string window_id =
      daily_use_case_impl_->GenerateWindowIdentifier(new_daily_ts);

  EXPECT_EQ(window_id.size(), 8u);
  EXPECT_EQ(window_id, "20220101");
}

TEST_F(DailyUseCaseImplTest, SameDayTimestampsHaveSameWindowId) {
  base::Time daily_ts_1;
  base::Time daily_ts_2;

  EXPECT_TRUE(base::Time::FromString("01 Jan 2022 00:00:00 GMT", &daily_ts_1));
  EXPECT_TRUE(base::Time::FromString("01 Jan 2022 23:59:59 GMT", &daily_ts_2));

  EXPECT_EQ(daily_use_case_impl_->GenerateWindowIdentifier(daily_ts_1),
            daily_use_case_impl_->GenerateWindowIdentifier(daily_ts_2));
}

TEST_F(DailyUseCaseImplTest, DifferentDayTimestampsHaveDifferentWindowId) {
  base::Time daily_ts_1;
  base::Time daily_ts_2;

  EXPECT_TRUE(base::Time::FromString("01 Jan 2022 00:00:00 GMT", &daily_ts_1));
  EXPECT_TRUE(base::Time::FromString("02 Jan 2022 00:00:00 GMT", &daily_ts_2));

  EXPECT_NE(daily_use_case_impl_->GenerateWindowIdentifier(daily_ts_1),
            daily_use_case_impl_->GenerateWindowIdentifier(daily_ts_2));
}

TEST_F(DailyUseCaseImplTest, ExpectedMetadataIsSet) {
  base::Time new_daily_ts;
  EXPECT_TRUE(
      base::Time::FromString("01 Jan 2022 23:59:59 GMT", &new_daily_ts));

  // Window identifier must be set before PSM id, and hence import request body
  // can be generated.
  daily_use_case_impl_->SetWindowIdentifier(new_daily_ts);

  FresnelImportDataRequest req =
      daily_use_case_impl_->GenerateImportRequestBody().value();
  EXPECT_EQ(req.device_metadata().hardware_id(), kHardwareClassKeyNotFound);
  EXPECT_EQ(req.device_metadata().chromeos_channel(), Channel::CHANNEL_STABLE);
  EXPECT_EQ(req.device_metadata().market_segment(),
            MarketSegment::MARKET_SEGMENT_UNKNOWN);
  EXPECT_FALSE(req.device_metadata().chromeos_version().empty());
}

}  // namespace ash::device_activity
