// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/device_activity/monthly_use_case_impl.h"

#include "ash/constants/ash_features.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
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

// Initialize fake value used by the FirstActiveUseCaseImpl.
// This secret should be of exactly length 64, since it is a 256 bit string
// encoded as a hexadecimal.
constexpr char kFakePsmDeviceActiveSecret[] =
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";

constexpr ChromeDeviceMetadataParameters kFakeChromeParameters = {
    version_info::Channel::STABLE /* chromeos_channel */,
    MarketSegment::MARKET_SEGMENT_UNKNOWN /* market_segment */,
};

}  // namespace

// TODO(hirthanan): Move shared tests to DeviceActiveUseCase base class.
class MonthlyUseCaseImplTest : public testing::Test {
 public:
  MonthlyUseCaseImplTest() = default;
  MonthlyUseCaseImplTest(const MonthlyUseCaseImplTest&) = delete;
  MonthlyUseCaseImplTest& operator=(const MonthlyUseCaseImplTest&) = delete;
  ~MonthlyUseCaseImplTest() override = default;

 protected:
  // testing::Test:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::
                                  kDeviceActiveClientMonthlyCheckMembership},
        /*disabled_features*/ {});

    DeviceActivityController::RegisterPrefs(local_state_.registry());

    chromeos::system::StatisticsProvider::SetTestProvider(
        &statistics_provider_);

    const std::vector<psm_rlwe::RlwePlaintextId> plaintext_ids;
    monthly_use_case_impl_ = std::make_unique<MonthlyUseCaseImpl>(
        kFakePsmDeviceActiveSecret, kFakeChromeParameters, &local_state_,
        // |FakePsmDelegate| can use any test case parameters.
        std::make_unique<FakePsmDelegate>(std::string() /* ec_cipher_key */,
                                          std::string() /* seed */,
                                          std::move(plaintext_ids)));
  }

  void TearDown() override { monthly_use_case_impl_.reset(); }

  std::unique_ptr<MonthlyUseCaseImpl> monthly_use_case_impl_;

  // Fake pref service for unit testing the local state.
  TestingPrefServiceSimple local_state_;
  base::test::ScopedFeatureList scoped_feature_list_;
  chromeos::system::FakeStatisticsProvider statistics_provider_;
};

TEST_F(MonthlyUseCaseImplTest, ValidateWindowIdFormattedCorrectly) {
  // Create fixed timestamp used to generate a fixed window identifier.
  base::Time new_monthly_ts;
  EXPECT_TRUE(
      base::Time::FromString("01 Jan 2022 23:59:59 GMT", &new_monthly_ts));

  std::string window_id =
      monthly_use_case_impl_->GenerateUTCWindowIdentifier(new_monthly_ts);

  EXPECT_EQ(window_id.size(), 6u);
  EXPECT_EQ(window_id, "202201");
}

TEST_F(MonthlyUseCaseImplTest, SameMonthTimestampsHaveSameWindowId) {
  base::Time monthly_ts_1;
  base::Time monthly_ts_2;

  EXPECT_TRUE(
      base::Time::FromString("01 Jan 2022 00:00:00 GMT", &monthly_ts_1));
  EXPECT_TRUE(
      base::Time::FromString("31 Jan 2022 23:59:59 GMT", &monthly_ts_2));

  EXPECT_EQ(monthly_use_case_impl_->GenerateUTCWindowIdentifier(monthly_ts_1),
            monthly_use_case_impl_->GenerateUTCWindowIdentifier(monthly_ts_2));
}

TEST_F(MonthlyUseCaseImplTest, DifferentMonthTimestampsHaveDifferentWindowId) {
  base::Time monthly_ts_1;
  base::Time monthly_ts_2;

  EXPECT_TRUE(
      base::Time::FromString("01 Jan 2022 00:00:00 GMT", &monthly_ts_1));
  EXPECT_TRUE(
      base::Time::FromString("01 Feb 2022 00:00:00 GMT", &monthly_ts_2));

  EXPECT_NE(monthly_use_case_impl_->GenerateUTCWindowIdentifier(monthly_ts_1),
            monthly_use_case_impl_->GenerateUTCWindowIdentifier(monthly_ts_2));
}

TEST_F(MonthlyUseCaseImplTest, ExpectedMetadataIsSet) {
  base::Time new_monthly_ts;
  EXPECT_TRUE(
      base::Time::FromString("01 Jan 2022 23:59:59 GMT", &new_monthly_ts));

  // Window identifier must be set before PSM id, and hence import request body
  // can be generated.
  monthly_use_case_impl_->SetWindowIdentifier(new_monthly_ts);

  FresnelImportDataRequest req =
      monthly_use_case_impl_->GenerateImportRequestBody();
  EXPECT_EQ(req.device_metadata().chromeos_channel(), Channel::CHANNEL_STABLE);
  EXPECT_EQ(req.device_metadata().market_segment(),
            MarketSegment::MARKET_SEGMENT_UNKNOWN);
  EXPECT_FALSE(req.device_metadata().chromeos_version().empty());
}

}  // namespace ash::device_activity
