// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/device_activity/daily_use_case_impl.h"

#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "chromeos/ash/components/device_activity/device_activity_controller.h"
#include "chromeos/ash/components/device_activity/fresnel_pref_names.h"
#include "components/prefs/testing_pref_service.h"
#include "components/version_info/channel.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/private_membership/src/private_membership_rlwe_client.h"

namespace ash {
namespace device_activity {

namespace psm_rlwe = private_membership::rlwe;

namespace {

// Initialize fake values used by the |DailyUseCaseImpl|.
constexpr char kFakePsmDeviceActiveSecret[] = "FAKE_PSM_DEVICE_ACTIVE_SECRET";

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

    daily_use_case_impl_ = std::make_unique<DailyUseCaseImpl>(
        kFakePsmDeviceActiveSecret, kFakeChromeParameters, &local_state_);
  }

  void TearDown() override { daily_use_case_impl_.reset(); }

  std::unique_ptr<DailyUseCaseImpl> daily_use_case_impl_;

  // Fake pref service for unit testing the local state.
  TestingPrefServiceSimple local_state_;
};

TEST_F(DailyUseCaseImplTest, CheckIfLastKnownPingTimestampNotSet) {
  EXPECT_FALSE(daily_use_case_impl_->IsLastKnownPingTimestampSet());
}

TEST_F(DailyUseCaseImplTest, CheckIfLastKnownPingTimestampSet) {
  // Create fixed timestamp to see if local state updates value correctly.
  base::Time new_daily_ts;
  EXPECT_TRUE(
      base::Time::FromString("01 Jan 2022 23:59:59 GMT", &new_daily_ts));

  // Update local state with fixed timestamp.
  daily_use_case_impl_->SetLastKnownPingTimestamp(new_daily_ts);

  EXPECT_EQ(daily_use_case_impl_->GetLastKnownPingTimestamp(), new_daily_ts);
  EXPECT_TRUE(daily_use_case_impl_->IsLastKnownPingTimestampSet());
}

TEST_F(DailyUseCaseImplTest, CheckGenerateUTCWindowIdentifierHasValidFormat) {
  // Create fixed timestamp used to generate a fixed window identifier.
  base::Time new_daily_ts;
  EXPECT_TRUE(
      base::Time::FromString("01 Jan 2022 23:59:59 GMT", &new_daily_ts));

  std::string window_id =
      daily_use_case_impl_->GenerateUTCWindowIdentifier(new_daily_ts);

  EXPECT_EQ(window_id.size(), 8);
  EXPECT_EQ(window_id, "20220101");
}

TEST_F(DailyUseCaseImplTest, CheckPsmIdEmptyIfWindowIdIsNotSet) {
  // |daily_use_case_impl_| must set the window id before generating the psm id.
  EXPECT_THAT(daily_use_case_impl_->GetPsmIdentifier(),
              testing::Eq(absl::nullopt));
}

TEST_F(DailyUseCaseImplTest, CheckPsmIdGeneratedCorrectly) {
  // Create fixed timestamp used to generate a fixed window identifier.
  // The window id must be set before generating the psm id.
  base::Time new_daily_ts;
  EXPECT_TRUE(
      base::Time::FromString("01 Jan 2022 23:59:59 GMT", &new_daily_ts));

  std::string window_id =
      daily_use_case_impl_->GenerateUTCWindowIdentifier(new_daily_ts);

  daily_use_case_impl_->SetWindowIdentifier(window_id);

  absl::optional<psm_rlwe::RlwePlaintextId> psm_id =
      daily_use_case_impl_->GetPsmIdentifier();

  EXPECT_TRUE(psm_id.has_value());

  // Verify the PSM value is correct for parameters supplied by the unit tests.
  std::string unhashed_psm_id = base::JoinString(
      {psm_rlwe::RlweUseCase_Name(daily_use_case_impl_->GetPsmUseCase()),
       window_id},
      "|");
  std::string expected_psm_id_hex = daily_use_case_impl_->GetDigestString(
      kFakePsmDeviceActiveSecret, unhashed_psm_id);
  EXPECT_EQ(psm_id.value().sensitive_id(), expected_psm_id_hex);
}

TEST_F(DailyUseCaseImplTest, PingRequiredInNonOverlappingUTCWindows) {
  base::Time last_daily_ts;
  base::Time current_daily_ts;

  EXPECT_TRUE(
      base::Time::FromString("01 Jan 2022 00:00:00 GMT", &last_daily_ts));
  daily_use_case_impl_->SetLastKnownPingTimestamp(last_daily_ts);

  EXPECT_TRUE(
      base::Time::FromString("05 Jan 2022 00:00:00 GMT", &current_daily_ts));

  EXPECT_TRUE(daily_use_case_impl_->IsDevicePingRequired(current_daily_ts));
}

TEST_F(DailyUseCaseImplTest, PingNotRequiredInOverlappingUTCWindows) {
  base::Time last_daily_ts;
  base::Time current_daily_ts;

  EXPECT_TRUE(
      base::Time::FromString("01 Jan 2022 12:59:59 GMT", &last_daily_ts));
  daily_use_case_impl_->SetLastKnownPingTimestamp(last_daily_ts);

  EXPECT_TRUE(
      base::Time::FromString("01 Jan 2022 15:59:59 GMT", &current_daily_ts));

  EXPECT_FALSE(daily_use_case_impl_->IsDevicePingRequired(current_daily_ts));
}

TEST_F(DailyUseCaseImplTest, CheckIfPingRequiredInUTCBoundaryCases) {
  base::Time last_daily_ts;
  base::Time current_daily_ts;

  EXPECT_TRUE(
      base::Time::FromString("01 Jan 2022 23:59:59 GMT", &last_daily_ts));
  daily_use_case_impl_->SetLastKnownPingTimestamp(last_daily_ts);

  EXPECT_TRUE(
      base::Time::FromString("02 Jan 2022 00:00:00 GMT", &current_daily_ts));

  EXPECT_TRUE(daily_use_case_impl_->IsDevicePingRequired(current_daily_ts));

  // Set last_daily_ts as a date after current_daily_ts.
  EXPECT_TRUE(
      base::Time::FromString("02 Jan 2022 00:00:00 GMT", &last_daily_ts));
  daily_use_case_impl_->SetLastKnownPingTimestamp(last_daily_ts);

  EXPECT_TRUE(
      base::Time::FromString("01 Jan 2022 23:59:59 GMT", &current_daily_ts));

  // Since the current_daily_ts is prior to the last_daily_ts, the function
  // should return false.
  EXPECT_FALSE(daily_use_case_impl_->IsDevicePingRequired(current_daily_ts));
}

TEST_F(DailyUseCaseImplTest, SameDayTimestampsHaveSameWindowId) {
  base::Time daily_ts_1;
  base::Time daily_ts_2;

  EXPECT_TRUE(base::Time::FromString("01 Jan 2022 00:00:00 GMT", &daily_ts_1));
  EXPECT_TRUE(base::Time::FromString("01 Jan 2022 23:59:59 GMT", &daily_ts_2));

  EXPECT_EQ(daily_use_case_impl_->GenerateUTCWindowIdentifier(daily_ts_1),
            daily_use_case_impl_->GenerateUTCWindowIdentifier(daily_ts_2));
}

TEST_F(DailyUseCaseImplTest, DifferentWindowIdGeneratesDifferentPsmId) {
  base::Time daily_ts_1;
  base::Time daily_ts_2;

  EXPECT_TRUE(base::Time::FromString("01 Jan 2022 00:00:00 GMT", &daily_ts_1));
  EXPECT_TRUE(base::Time::FromString("02 Jan 2022 00:00:00 GMT", &daily_ts_2));

  std::string window_id_1 =
      daily_use_case_impl_->GenerateUTCWindowIdentifier(daily_ts_1);
  std::string window_id_2 =
      daily_use_case_impl_->GenerateUTCWindowIdentifier(daily_ts_2);

  daily_use_case_impl_->SetWindowIdentifier(window_id_1);
  absl::optional<psm_rlwe::RlwePlaintextId> psm_id_1 =
      daily_use_case_impl_->GetPsmIdentifier();

  daily_use_case_impl_->SetWindowIdentifier(window_id_2);
  absl::optional<psm_rlwe::RlwePlaintextId> psm_id_2 =
      daily_use_case_impl_->GetPsmIdentifier();

  EXPECT_NE(psm_id_1.value().sensitive_id(), psm_id_2.value().sensitive_id());
}

}  // namespace device_activity
}  // namespace ash
