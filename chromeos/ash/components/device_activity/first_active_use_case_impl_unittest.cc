// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/device_activity/first_active_use_case_impl.h"

#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "chromeos/ash/components/device_activity/device_activity_controller.h"
#include "chromeos/ash/components/device_activity/fresnel_pref_names.h"
#include "chromeos/system/fake_statistics_provider.h"
#include "components/prefs/testing_pref_service.h"
#include "components/version_info/channel.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/private_membership/src/private_membership_rlwe_client.h"

namespace ash {
namespace device_activity {

namespace psm_rlwe = private_membership::rlwe;

namespace {

// Initialize fake values used by the FirstActiveUseCaseImpl.
constexpr char kFakePsmDeviceActiveSecret[] = "FAKE_PSM_DEVICE_ACTIVE_SECRET";

// TODO(hirthanan): Enable when rolling out check membership requests for the
// first active use case.
// constexpr char kHardwareClassKeyNotFound[] = "HARDWARE_CLASS_KEY_NOT_FOUND";

constexpr ChromeDeviceMetadataParameters kFakeChromeParameters = {
    version_info::Channel::STABLE /* chromeos_channel */,
    MarketSegment::MARKET_SEGMENT_UNKNOWN /* market_segment */,
};

}  // namespace

class FirstActiveUseCaseImplTest : public testing::Test {
 public:
  FirstActiveUseCaseImplTest() = default;
  FirstActiveUseCaseImplTest(const FirstActiveUseCaseImplTest&) = delete;
  FirstActiveUseCaseImplTest& operator=(const FirstActiveUseCaseImplTest&) =
      delete;
  ~FirstActiveUseCaseImplTest() override = default;

 protected:
  // testing::Test:
  void SetUp() override {
    DeviceActivityController::RegisterPrefs(local_state_.registry());

    chromeos::system::StatisticsProvider::SetTestProvider(
        &statistics_provider_);

    first_active_use_case_impl_ = std::make_unique<FirstActiveUseCaseImpl>(
        kFakePsmDeviceActiveSecret, kFakeChromeParameters, &local_state_);
  }

  void TearDown() override { first_active_use_case_impl_.reset(); }

  std::unique_ptr<FirstActiveUseCaseImpl> first_active_use_case_impl_;

  // Fake pref service for unit testing the local state.
  TestingPrefServiceSimple local_state_;
  chromeos::system::FakeStatisticsProvider statistics_provider_;
};

TEST_F(FirstActiveUseCaseImplTest, CheckIfLastKnownPingTimestampNotSet) {
  EXPECT_FALSE(first_active_use_case_impl_->IsLastKnownPingTimestampSet());
}

TEST_F(FirstActiveUseCaseImplTest, CheckIfLastKnownPingTimestampSet) {
  // Create fixed timestamp to see if local state updates value correctly.
  base::Time new_first_active_ts;
  EXPECT_TRUE(
      base::Time::FromString("01 Jan 2022 23:59:59 GMT", &new_first_active_ts));

  // Update local state with fixed timestamp.
  first_active_use_case_impl_->SetLastKnownPingTimestamp(new_first_active_ts);

  EXPECT_EQ(first_active_use_case_impl_->GetLastKnownPingTimestamp(),
            new_first_active_ts);
  EXPECT_TRUE(first_active_use_case_impl_->IsLastKnownPingTimestampSet());
}

TEST_F(FirstActiveUseCaseImplTest,
       CheckGenerateUTCWindowIdentifierHasValidFormat) {
  // Create fixed timestamp used to generate a fixed window identifier.
  base::Time new_first_active_ts;
  EXPECT_TRUE(
      base::Time::FromString("01 Jan 2022 23:59:59 GMT", &new_first_active_ts));

  std::string window_id =
      first_active_use_case_impl_->GenerateUTCWindowIdentifier(
          new_first_active_ts);

  EXPECT_EQ(window_id.size(), 8);
  EXPECT_EQ(window_id, "20220101");
}

TEST_F(FirstActiveUseCaseImplTest, CheckPsmIdEmptyIfWindowIdIsNotSet) {
  // |first_active_use_case_impl_| must set the window id before generating the
  // psm id.
  EXPECT_THAT(first_active_use_case_impl_->GetPsmIdentifier(),
              testing::Eq(absl::nullopt));
}

TEST_F(FirstActiveUseCaseImplTest, CheckPsmIdGeneratedCorrectly) {
  // Create fixed timestamp used to generate a fixed window identifier.
  // The window id must be set before generating the psm id.
  base::Time new_first_active_ts;
  EXPECT_TRUE(
      base::Time::FromString("01 Jan 2022 23:59:59 GMT", &new_first_active_ts));

  std::string window_id =
      first_active_use_case_impl_->GenerateUTCWindowIdentifier(
          new_first_active_ts);

  first_active_use_case_impl_->SetWindowIdentifier(window_id);

  absl::optional<psm_rlwe::RlwePlaintextId> psm_id =
      first_active_use_case_impl_->GetPsmIdentifier();

  EXPECT_TRUE(psm_id.has_value());

  // Verify the PSM value is correct for parameters supplied by the unit tests.
  std::string unhashed_psm_id = base::JoinString(
      {psm_rlwe::RlweUseCase_Name(first_active_use_case_impl_->GetPsmUseCase()),
       window_id},
      "|");
  std::string expected_psm_id_hex =
      first_active_use_case_impl_->GetDigestString(kFakePsmDeviceActiveSecret,
                                                   unhashed_psm_id);
  EXPECT_EQ(psm_id.value().sensitive_id(), expected_psm_id_hex);
}

TEST_F(FirstActiveUseCaseImplTest, PingRequiredInNonOverlappingUTCWindows) {
  base::Time last_first_active_ts;
  base::Time current_first_active_ts;

  EXPECT_TRUE(base::Time::FromString("01 Jan 2022 00:00:00 GMT",
                                     &last_first_active_ts));
  first_active_use_case_impl_->SetLastKnownPingTimestamp(last_first_active_ts);

  EXPECT_TRUE(base::Time::FromString("05 Jan 2022 00:00:00 GMT",
                                     &current_first_active_ts));

  EXPECT_TRUE(first_active_use_case_impl_->IsDevicePingRequired(
      current_first_active_ts));
}

TEST_F(FirstActiveUseCaseImplTest, PingNotRequiredInOverlappingUTCWindows) {
  base::Time last_first_active_ts;
  base::Time current_first_active_ts;

  EXPECT_TRUE(base::Time::FromString("01 Jan 2022 12:59:59 GMT",
                                     &last_first_active_ts));
  first_active_use_case_impl_->SetLastKnownPingTimestamp(last_first_active_ts);

  EXPECT_TRUE(base::Time::FromString("01 Jan 2022 15:59:59 GMT",
                                     &current_first_active_ts));

  EXPECT_FALSE(first_active_use_case_impl_->IsDevicePingRequired(
      current_first_active_ts));
}

TEST_F(FirstActiveUseCaseImplTest, CheckIfPingRequiredInUTCBoundaryCases) {
  base::Time last_first_active_ts;
  base::Time current_first_active_ts;

  EXPECT_TRUE(base::Time::FromString("01 Jan 2022 23:59:59 GMT",
                                     &last_first_active_ts));
  first_active_use_case_impl_->SetLastKnownPingTimestamp(last_first_active_ts);

  EXPECT_TRUE(base::Time::FromString("02 Jan 2022 00:00:00 GMT",
                                     &current_first_active_ts));

  EXPECT_TRUE(first_active_use_case_impl_->IsDevicePingRequired(
      current_first_active_ts));

  // Set last_first_active_ts as a date after current_first_active_ts.
  EXPECT_TRUE(base::Time::FromString("02 Jan 2022 00:00:00 GMT",
                                     &last_first_active_ts));
  first_active_use_case_impl_->SetLastKnownPingTimestamp(last_first_active_ts);

  EXPECT_TRUE(base::Time::FromString("01 Jan 2022 23:59:59 GMT",
                                     &current_first_active_ts));

  // Since the current_first_active_ts is prior to the last_first_active_ts, the
  // function should return false.
  EXPECT_FALSE(first_active_use_case_impl_->IsDevicePingRequired(
      current_first_active_ts));
}

TEST_F(FirstActiveUseCaseImplTest, SameDayTimestampsHaveSameWindowId) {
  base::Time first_active_ts_1;
  base::Time first_active_ts_2;

  EXPECT_TRUE(
      base::Time::FromString("01 Jan 2022 00:00:00 GMT", &first_active_ts_1));
  EXPECT_TRUE(
      base::Time::FromString("01 Jan 2022 23:59:59 GMT", &first_active_ts_2));

  EXPECT_EQ(first_active_use_case_impl_->GenerateUTCWindowIdentifier(
                first_active_ts_1),
            first_active_use_case_impl_->GenerateUTCWindowIdentifier(
                first_active_ts_2));
}

TEST_F(FirstActiveUseCaseImplTest, DifferentWindowIdGeneratesDifferentPsmId) {
  base::Time first_active_ts_1;
  base::Time first_active_ts_2;

  EXPECT_TRUE(
      base::Time::FromString("01 Jan 2022 00:00:00 GMT", &first_active_ts_1));
  EXPECT_TRUE(
      base::Time::FromString("02 Jan 2022 00:00:00 GMT", &first_active_ts_2));

  std::string window_id_1 =
      first_active_use_case_impl_->GenerateUTCWindowIdentifier(
          first_active_ts_1);
  std::string window_id_2 =
      first_active_use_case_impl_->GenerateUTCWindowIdentifier(
          first_active_ts_2);

  first_active_use_case_impl_->SetWindowIdentifier(window_id_1);
  absl::optional<psm_rlwe::RlwePlaintextId> psm_id_1 =
      first_active_use_case_impl_->GetPsmIdentifier();

  first_active_use_case_impl_->SetWindowIdentifier(window_id_2);
  absl::optional<psm_rlwe::RlwePlaintextId> psm_id_2 =
      first_active_use_case_impl_->GetPsmIdentifier();

  EXPECT_NE(psm_id_1.value().sensitive_id(), psm_id_2.value().sensitive_id());
}

TEST_F(FirstActiveUseCaseImplTest, ValidateWindowIdFormattedCorrectly) {
  base::Time new_first_active_ts;
  EXPECT_TRUE(
      base::Time::FromString("01 Jan 2022 23:59:59 GMT", &new_first_active_ts));

  // Validate UTC window identifier follows format YYYYMMDD.
  EXPECT_EQ(first_active_use_case_impl_->GenerateUTCWindowIdentifier(
                new_first_active_ts),
            "20220101");
}

TEST_F(FirstActiveUseCaseImplTest, ExpectedMetadataIsSet) {
  base::Time new_first_active_ts;
  EXPECT_TRUE(
      base::Time::FromString("01 Jan 2022 23:59:59 GMT", &new_first_active_ts));

  // Window identifier must be set before PSM id, and hence import request body
  // can be generated.
  first_active_use_case_impl_->SetWindowIdentifier(
      first_active_use_case_impl_->GenerateUTCWindowIdentifier(
          new_first_active_ts));

  ImportDataRequest req =
      first_active_use_case_impl_->GenerateImportRequestBody();
  EXPECT_EQ(req.device_metadata().chromeos_channel(), Channel::CHANNEL_STABLE);
  EXPECT_FALSE(req.device_metadata().chromeos_version().empty());

  // TODO(hirthanan): Enable when rolling out check membership requests for the
  // first active use case.
  // EXPECT_EQ(req.device_metadata().hardware_id(), kHardwareClassKeyNotFound);
  // EXPECT_EQ(req.device_metadata().market_segment(),
  //          MarketSegment::MARKET_SEGMENT_UNKNOWN);
}

}  // namespace device_activity
}  // namespace ash
