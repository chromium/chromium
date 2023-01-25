// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/device_activity/twenty_eight_day_active_use_case_impl.h"

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

// Used to configure the last N days to send plaintext id check membership
// requests.
constexpr size_t kRollingWindowSize = 28;

// Initialize fake value used by the TwentyEightDayActiveUseCaseImpl.
// This secret should be of exactly length 64, since it is a 256 bit string
// encoded as a hexadecimal.
constexpr char kFakePsmDeviceActiveSecret[] =
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";

// TODO(hirthanan): Enable when rolling out check membership requests.
// constexpr char kHardwareClassKeyNotFound[] = "HARDWARE_CLASS_KEY_NOT_FOUND";

constexpr ChromeDeviceMetadataParameters kFakeChromeParameters = {
    version_info::Channel::STABLE /* chromeos_channel */,
    MarketSegment::MARKET_SEGMENT_UNKNOWN /* market_segment */,
};

}  // namespace

class TwentyEightDayActiveUseCaseImplTest : public testing::Test {
 public:
  TwentyEightDayActiveUseCaseImplTest() = default;
  TwentyEightDayActiveUseCaseImplTest(
      const TwentyEightDayActiveUseCaseImplTest&) = delete;
  TwentyEightDayActiveUseCaseImplTest& operator=(
      const TwentyEightDayActiveUseCaseImplTest&) = delete;
  ~TwentyEightDayActiveUseCaseImplTest() override = default;

 protected:
  // testing::Test:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {features::kDeviceActiveClient28DayActiveCheckMembership},
        /*disabled_features*/ {});

    DeviceActivityController::RegisterPrefs(local_state_.registry());

    system::StatisticsProvider::SetTestProvider(&statistics_provider_);

    const std::vector<psm_rlwe::RlwePlaintextId> plaintext_ids;
    twenty_eight_day_active_use_case_impl_ =
        std::make_unique<TwentyEightDayActiveUseCaseImpl>(
            kFakePsmDeviceActiveSecret, kFakeChromeParameters, &local_state_,
            // |FakePsmDelegate| can use any test case parameters.
            std::make_unique<FakePsmDelegate>(std::string() /* ec_cipher_key */,
                                              std::string() /* seed */,
                                              std::move(plaintext_ids)));
  }

  void TearDown() override { twenty_eight_day_active_use_case_impl_.reset(); }

  std::unique_ptr<TwentyEightDayActiveUseCaseImpl>
      twenty_eight_day_active_use_case_impl_;

  // Fake pref service for unit testing the local state.
  TestingPrefServiceSimple local_state_;
  base::test::ScopedFeatureList scoped_feature_list_;
  system::FakeStatisticsProvider statistics_provider_;
};

TEST_F(TwentyEightDayActiveUseCaseImplTest,
       ValidateWindowIdFormattedCorrectly) {
  // Create fixed timestamp used to generate a fixed window identifier.
  base::Time new_daily_ts;
  EXPECT_TRUE(
      base::Time::FromString("01 Jan 2022 23:59:59 GMT", &new_daily_ts));

  std::string window_id =
      twenty_eight_day_active_use_case_impl_->GenerateWindowIdentifier(
          new_daily_ts);

  EXPECT_EQ(static_cast<int>(window_id.size()), 8);
  EXPECT_EQ(window_id, "20220101");
}

TEST_F(TwentyEightDayActiveUseCaseImplTest, SameDayTimestampsHaveSameWindowId) {
  base::Time daily_ts_1;
  base::Time daily_ts_2;

  EXPECT_TRUE(base::Time::FromString("01 Jan 2022 00:00:00 GMT", &daily_ts_1));
  EXPECT_TRUE(base::Time::FromString("01 Jan 2022 23:59:59 GMT", &daily_ts_2));

  EXPECT_EQ(twenty_eight_day_active_use_case_impl_->GenerateWindowIdentifier(
                daily_ts_1),
            twenty_eight_day_active_use_case_impl_->GenerateWindowIdentifier(
                daily_ts_2));
}

TEST_F(TwentyEightDayActiveUseCaseImplTest,
       DifferentDayTimestampsHaveDifferentWindowId) {
  base::Time daily_ts_1;
  base::Time daily_ts_2;

  EXPECT_TRUE(base::Time::FromString("01 Jan 2022 00:00:00 GMT", &daily_ts_1));
  EXPECT_TRUE(base::Time::FromString("02 Jan 2022 00:00:00 GMT", &daily_ts_2));

  EXPECT_NE(twenty_eight_day_active_use_case_impl_->GenerateWindowIdentifier(
                daily_ts_1),
            twenty_eight_day_active_use_case_impl_->GenerateWindowIdentifier(
                daily_ts_2));
}

TEST_F(TwentyEightDayActiveUseCaseImplTest, ExpectedMetadataIsSet) {
  base::Time new_daily_ts;
  EXPECT_TRUE(
      base::Time::FromString("01 Jan 2022 23:59:59 GMT", &new_daily_ts));

  // Window identifier must be set before PSM id, and hence import request body
  // can be generated.
  twenty_eight_day_active_use_case_impl_->SetWindowIdentifier(new_daily_ts);

  FresnelImportDataRequest req =
      twenty_eight_day_active_use_case_impl_->GenerateImportRequestBody()
          .value();
  EXPECT_EQ(req.device_metadata().chromeos_channel(), Channel::CHANNEL_STABLE);
  EXPECT_FALSE(req.device_metadata().chromeos_version().empty());

  // TODO(hirthanan): Enable when rolling out check membership requests.
  // EXPECT_EQ(req.device_metadata().hardware_id(), kHardwareClassKeyNotFound);
  // EXPECT_EQ(req.device_metadata().market_segment(),
  //          MarketSegment::MARKET_SEGMENT_UNKNOWN);
}

TEST_F(TwentyEightDayActiveUseCaseImplTest,
       VerifyNumberOfPsmIdsInQueryRequest) {
  // PSM identifiers to query is unknown since there is no window id set yet.
  std::vector<psm_rlwe::RlwePlaintextId> psm_ids =
      twenty_eight_day_active_use_case_impl_->GetPsmIdentifiersToQuery();

  EXPECT_EQ(static_cast<int>(twenty_eight_day_active_use_case_impl_
                                 ->GetPsmIdentifiersToQuery()
                                 .size()),
            0);

  base::Time ts;
  EXPECT_TRUE(base::Time::FromString("01 Jan 2022 23:59:59 GMT", &ts));

  // Set the window id to some active date.
  EXPECT_TRUE(twenty_eight_day_active_use_case_impl_->SetWindowIdentifier(ts));

  EXPECT_EQ(
      twenty_eight_day_active_use_case_impl_->GetPsmIdentifiersToQuery().size(),
      kRollingWindowSize);
}

TEST_F(TwentyEightDayActiveUseCaseImplTest,
       PsmIdToDatesIsInitializedCorrectly) {
  base::Time ts;
  EXPECT_TRUE(base::Time::FromString("01 Jan 2022 23:59:59 GMT", &ts));

  EXPECT_TRUE(twenty_eight_day_active_use_case_impl_->SetWindowIdentifier(ts));

  std::vector<std::string> expected_psm_window_ids = {
      "20220101", "20211231", "20211230", "20211229", "20211228", "20211227",
      "20211226", "20211225", "20211224", "20211223", "20211222", "20211221",
      "20211220", "20211219", "20211218", "20211217", "20211216", "20211215",
      "20211214", "20211213", "20211212", "20211211", "20211210", "20211209",
      "20211208", "20211207", "20211206", "20211205",
  };

  EXPECT_EQ(expected_psm_window_ids.size(), kRollingWindowSize);

  // Verify that each of the expected psm window ids are contained in the
  // |psm_id_to_date_| map.
  for (auto& window_id : expected_psm_window_ids) {
    psm_rlwe::RlwePlaintextId expected_psm_id =
        twenty_eight_day_active_use_case_impl_->GeneratePsmIdentifier(window_id)
            .value();

    // Method should return a non Unix Epoch date if found.
    EXPECT_NE(twenty_eight_day_active_use_case_impl_->RetrievePsmIdDate(
                  expected_psm_id),
              base::Time::UnixEpoch());
  }
}

TEST_F(TwentyEightDayActiveUseCaseImplTest,
       SearchForLastTwentyEightIdentifiersFromRegularDate) {
  base::Time ts;
  EXPECT_TRUE(base::Time::FromString("01 Jan 2022 23:59:59 GMT", &ts));

  EXPECT_TRUE(twenty_eight_day_active_use_case_impl_->SetWindowIdentifier(ts));

  std::vector<std::string> expected_psm_window_ids = {
      "20220101", "20211231", "20211230", "20211229",
      "20211228", "20211227", "20211226"};

  for (auto& expected_window_id : expected_psm_window_ids) {
    psm_rlwe::RlwePlaintextId expected_psm_id =
        twenty_eight_day_active_use_case_impl_
            ->GeneratePsmIdentifier(expected_window_id)
            .value();

    // Validate that all query PSM ids are correctly generated from the reported
    // ts date. Note that the only input to the
    // |twenty_eight_day_active_use_case_impl_| is where the window identifier
    // is set. This generates the expected psm id to window dates that is
    // verified here.
    std::string actual_window_id =
        twenty_eight_day_active_use_case_impl_->GenerateWindowIdentifier(
            twenty_eight_day_active_use_case_impl_->RetrievePsmIdDate(
                expected_psm_id));
    EXPECT_EQ(actual_window_id, expected_window_id);
  }
}

TEST_F(TwentyEightDayActiveUseCaseImplTest,
       SearchForLastTwentyEightIdentifiersFromLeapYearDate) {
  base::Time ts;
  EXPECT_TRUE(base::Time::FromString("01 Mar 2020 23:59:59 GMT", &ts));

  EXPECT_TRUE(twenty_eight_day_active_use_case_impl_->SetWindowIdentifier(ts));

  std::vector<psm_rlwe::RlwePlaintextId> psm_ids =
      twenty_eight_day_active_use_case_impl_->GetPsmIdentifiersToQuery();

  EXPECT_EQ(psm_ids.size(), kRollingWindowSize);

  std::vector<std::string> expected_psm_window_ids = {
      "20200301", "20200229", "20200228", "20200227",
      "20200226", "20200225", "20200224"};

  for (auto& expected_window_id : expected_psm_window_ids) {
    psm_rlwe::RlwePlaintextId expected_psm_id =
        twenty_eight_day_active_use_case_impl_
            ->GeneratePsmIdentifier(expected_window_id)
            .value();

    // Validate that all query PSM ids are correctly generated from the reported
    // ts date. Note that the only input to the
    // |twenty_eight_day_active_use_case_impl_| is where the window identifier
    // is set. This generates the expected psm id to window dates that is
    // verified here.
    std::string actual_window_id =
        twenty_eight_day_active_use_case_impl_->GenerateWindowIdentifier(
            twenty_eight_day_active_use_case_impl_->RetrievePsmIdDate(
                expected_psm_id));
    EXPECT_EQ(actual_window_id, expected_window_id);
  }
}

TEST_F(TwentyEightDayActiveUseCaseImplTest,
       SearchForLastTwentyEightIdentifiersFromNonLeapYearDate) {
  base::Time ts;
  EXPECT_TRUE(base::Time::FromString("01 Mar 2021 23:59:59 GMT", &ts));

  EXPECT_TRUE(twenty_eight_day_active_use_case_impl_->SetWindowIdentifier(ts));

  std::vector<psm_rlwe::RlwePlaintextId> psm_ids =
      twenty_eight_day_active_use_case_impl_->GetPsmIdentifiersToQuery();

  EXPECT_EQ(psm_ids.size(), kRollingWindowSize);

  std::vector<std::string> expected_psm_window_ids = {
      "20210301", "20210228", "20210227", "20210226", "20210225", "20210224",
      "20210223", "20210222", "20210221", "20210223", "20210222", "20210221",
      "20210220", "20210219", "20210218", "20210217", "20210216", "20210215",
      "20210214", "20210213", "20210212", "20210211", "20210210", "20210209",
      "20210208", "20210207", "20210206", "20210205",
  };

  for (auto& expected_window_id : expected_psm_window_ids) {
    psm_rlwe::RlwePlaintextId expected_psm_id =
        twenty_eight_day_active_use_case_impl_
            ->GeneratePsmIdentifier(expected_window_id)
            .value();

    // Validate that all query PSM ids are correctly generated from the reported
    // ts date. Note that the only input to the
    // |twenty_eight_day_active_use_case_impl_| is where the window identifier
    // is set. This generates the expected psm id to window dates that is
    // verified here.
    std::string actual_window_id =
        twenty_eight_day_active_use_case_impl_->GenerateWindowIdentifier(
            twenty_eight_day_active_use_case_impl_->RetrievePsmIdDate(
                expected_psm_id));
    EXPECT_EQ(actual_window_id, expected_window_id);
  }
}

TEST_F(TwentyEightDayActiveUseCaseImplTest,
       CheckInEntireRollingWindowOnEmptyLocalState) {
  // Set local state to unix epoch.
  twenty_eight_day_active_use_case_impl_->SetLastKnownPingTimestamp(
      base::Time::UnixEpoch());

  // Update current ts to 20220102
  base::Time ts;
  EXPECT_TRUE(base::Time::FromString("01 Jan 2022 00:00:00 GMT", &ts));
  EXPECT_TRUE(twenty_eight_day_active_use_case_impl_->SetWindowIdentifier(ts));

  std::vector<FresnelImportData> import_data =
      twenty_eight_day_active_use_case_impl_->GetImportData();

  EXPECT_EQ(import_data.size(), kRollingWindowSize);

  std::vector<std::string> expected_window_ids = {
      "20220101", "20220102", "20220103", "20220104", "20220105", "20220106",
      "20220107", "20220108", "20220109", "20220110", "20220111", "20220112",
      "20220113", "20220114", "20220115", "20220116", "20220117", "20220118",
      "20220119", "20220120", "20220121", "20220122", "20220123", "20220124",
      "20220125", "20220126", "20220127", "20220128",
  };

  for (auto& v : import_data) {
    EXPECT_TRUE(std::find(expected_window_ids.begin(),
                          expected_window_ids.end(),
                          v.window_identifier()) != expected_window_ids.end());
  }
}

TEST_F(TwentyEightDayActiveUseCaseImplTest,
       CheckInPartialRollingWindowOnDeviceThatReportedYesterday) {
  base::Time ts;
  EXPECT_TRUE(base::Time::FromString("01 Jan 2022 00:00:00 GMT", &ts));
  twenty_eight_day_active_use_case_impl_->SetLastKnownPingTimestamp(ts);

  // Update current ts to 20220102
  EXPECT_TRUE(base::Time::FromString("02 Jan 2022 00:00:00 GMT", &ts));
  EXPECT_TRUE(twenty_eight_day_active_use_case_impl_->SetWindowIdentifier(ts));

  std::vector<FresnelImportData> import_data =
      twenty_eight_day_active_use_case_impl_->GetImportData();

  // Client should only import for the new day 20220108.
  EXPECT_EQ(import_data.size(), 1u);

  std::vector<std::string> expected_window_ids = {"20220129"};

  for (auto& v : import_data) {
    EXPECT_TRUE(std::find(expected_window_ids.begin(),
                          expected_window_ids.end(),
                          v.window_identifier()) != expected_window_ids.end());
  }
}

TEST_F(TwentyEightDayActiveUseCaseImplTest,
       CheckInEmptyOnDeviceThatReportedFuture) {
  base::Time ts;
  EXPECT_TRUE(base::Time::FromString("03 Jan 2022 00:00:00 GMT", &ts));
  twenty_eight_day_active_use_case_impl_->SetLastKnownPingTimestamp(ts);

  // Update current ts to a previous date 20220102.
  // This may happen if the system clock goes back in time due to NTP sync
  // error.
  EXPECT_TRUE(base::Time::FromString("02 Jan 2022 00:00:00 GMT", &ts));
  EXPECT_TRUE(twenty_eight_day_active_use_case_impl_->SetWindowIdentifier(ts));

  std::vector<FresnelImportData> import_data =
      twenty_eight_day_active_use_case_impl_->GetImportData();

  // Client should not import any data.
  EXPECT_EQ(import_data.size(), 0u);
}

}  // namespace ash::device_activity
