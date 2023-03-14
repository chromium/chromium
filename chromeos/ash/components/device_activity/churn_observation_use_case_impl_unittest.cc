// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/device_activity/churn_observation_use_case_impl.h"

#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "chromeos/ash/components/device_activity/churn_active_status.h"
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

// Set the current time to the following string.
const char kFakeNowTimeString[] = "2023-01-01 00:00:00 GMT";

// Set the first active week from VPD field as "2023-01".
// This value represents the UTC based activate date of the device formatted
// YYYY-WW to reduce privacy granularity.
// See
// https://crsrc.org/o/src/third_party/chromiumos-overlay/chromeos-base/chromeos-activate-date/files/activate_date;l=67
const char kFakeFirstActivateDate[] = "2023-01";

// This secret should be of exactly length 64, since it is a 256 bit string
// encoded as a hexadecimal.
constexpr char kFakePsmDeviceActiveSecret[] =
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";

constexpr ChromeDeviceMetadataParameters kFakeChromeParameters = {
    version_info::Channel::STABLE /* chromeos_channel */,
    MarketSegment::MARKET_SEGMENT_UNKNOWN /* market_segment */,
};

base::Time GetNextMonth(base::Time ts) {
  base::Time::Exploded exploded;
  ts.UTCExplode(&exploded);

  // Set new time to the first midnight of the next month.
  exploded.day_of_month = 1;
  exploded.month += 1;
  exploded.hour = 0;
  exploded.minute = 0;
  exploded.second = 0;
  exploded.millisecond = 0;

  // Handle case when month is December.
  if (exploded.month > 12) {
    exploded.year += 1;
    exploded.month = 1;
  }

  base::Time new_month_ts;
  bool success = base::Time::FromUTCExploded(exploded, &new_month_ts);

  if (!success) {
    return base::Time();
  }

  return new_month_ts;
}

// Number of months in a year.
constexpr int kMonthsInYear = 12;

base::Time GetNextYear(base::Time ts) {
  base::Time new_year_ts = ts;
  for (int i = 0; i < kMonthsInYear; i++) {
    new_year_ts = GetNextMonth(new_year_ts);
  }

  return new_year_ts;
}

}  // namespace

class ChurnObservationUseCaseImplTest : public testing::Test {
 public:
  ChurnObservationUseCaseImplTest() = default;
  ChurnObservationUseCaseImplTest(const ChurnObservationUseCaseImplTest&) =
      delete;
  ChurnObservationUseCaseImplTest& operator=(
      const ChurnObservationUseCaseImplTest&) = delete;
  ~ChurnObservationUseCaseImplTest() override = default;

 protected:
  // testing::Test:
  void SetUp() override {
    DeviceActivityController::RegisterPrefs(local_state_.registry());

    system::StatisticsProvider::SetTestProvider(&statistics_provider_);

    // Set the ActivateDate key in machine statistics as kFakeFirstActivateDate.
    statistics_provider_.SetMachineStatistic(system::kActivateDateKey,
                                             kFakeFirstActivateDate);

    // Initialize the churn active status to a default value of 0.
    churn_active_status_ = std::make_unique<ChurnActiveStatus>(0);

    const std::vector<psm_rlwe::RlwePlaintextId> plaintext_ids;
    churn_observation_use_case_impl_ =
        std::make_unique<ChurnObservationUseCaseImpl>(
            churn_active_status_.get(), kFakePsmDeviceActiveSecret,
            kFakeChromeParameters, &local_state_,
            // |FakePsmDelegate| can use any test case parameters.
            std::make_unique<FakePsmDelegate>(std::string() /* ec_cipher_key */,
                                              std::string() /* seed */,
                                              std::move(plaintext_ids)));

    // Update reporting ts for use case to kFakeNowTimeString.
    base::Time now;
    EXPECT_TRUE(base::Time::FromUTCString(kFakeNowTimeString, &now));
    churn_observation_use_case_impl_->SetWindowIdentifier(now);

    // Typically the churn cohort use case will update the
    // |churn_active_status_| object before this use case is run.
    churn_active_status_->UpdateValue(now);
  }

  void TearDown() override {
    DCHECK(churn_observation_use_case_impl_);
    DCHECK(churn_active_status_);

    // Safely destruct unique pointers.
    churn_observation_use_case_impl_.reset();
    churn_active_status_.reset();
  }

  std::unique_ptr<ChurnActiveStatus> churn_active_status_;
  std::unique_ptr<ChurnObservationUseCaseImpl> churn_observation_use_case_impl_;

  // Fake pref service for unit testing the local state.
  TestingPrefServiceSimple local_state_;
  system::FakeStatisticsProvider statistics_provider_;
};

TEST_F(ChurnObservationUseCaseImplTest, ValidateWindowIdFormattedCorrectly) {
  absl::optional<std::string> window_id =
      churn_observation_use_case_impl_->GetWindowIdentifier();

  EXPECT_TRUE(window_id.value() != std::string());
  EXPECT_EQ(static_cast<int>(window_id.value().size()), 6);

  EXPECT_EQ(window_id.value(), "202301");
}

TEST_F(ChurnObservationUseCaseImplTest, GetFirstActiveDuringCohortStatus) {
  // Initialize active status object with fake value.
  std::bitset<ChurnActiveStatus::kChurnBitSize> max_28_bits(
      "1111111111111111111111111111");
  churn_active_status_->SetValueForTesting(max_28_bits);
  churn_observation_use_case_impl_->GenerateImportRequestBody();
}

TEST_F(ChurnObservationUseCaseImplTest,
       ValidateObservationWindowsMiddleOfYear) {
  // Update reporting date to middle of the year.
  base::Time now;
  EXPECT_TRUE(base::Time::FromUTCString("2023-05-01 00:00:00 GMT", &now));
  churn_observation_use_case_impl_->SetWindowIdentifier(now);

  // Cohort churn use case should have updated the churn_active_status object
  // prior to observation check in.
  churn_active_status_->UpdateValue(now);

  // Generate observation windows.
  churn_observation_use_case_impl_->GenerateImportRequestBody();

  // Validate observation windows are expected.
  EXPECT_EQ(churn_observation_use_case_impl_->GetObservationPeriod(0),
            "202305-202307");
  EXPECT_EQ(churn_observation_use_case_impl_->GetObservationPeriod(1),
            "202304-202306");
  EXPECT_EQ(churn_observation_use_case_impl_->GetObservationPeriod(2),
            "202303-202305");
}

TEST_F(ChurnObservationUseCaseImplTest, ValidateObservationWindowsEndOfYear) {
  // Update reporting date to end of the year.
  base::Time now;
  EXPECT_TRUE(base::Time::FromUTCString("2023-12-01 00:00:00 GMT", &now));
  churn_observation_use_case_impl_->SetWindowIdentifier(now);

  // Cohort churn use case should have updated the churn_active_status object
  // prior to observation check in.
  churn_active_status_->UpdateValue(now);

  // Generate observation windows.
  churn_observation_use_case_impl_->GenerateImportRequestBody();

  // Validate observation windows are expected.
  EXPECT_EQ(churn_observation_use_case_impl_->GetObservationPeriod(0),
            "202312-202402");
  EXPECT_EQ(churn_observation_use_case_impl_->GetObservationPeriod(1),
            "202311-202401");
  EXPECT_EQ(churn_observation_use_case_impl_->GetObservationPeriod(2),
            "202310-202312");
}

TEST_F(ChurnObservationUseCaseImplTest, ValidateObservationWindowsStartOfYear) {
  // Update reporting date to start of the year.
  base::Time now;
  EXPECT_TRUE(base::Time::FromUTCString("2023-01-01 00:00:00 GMT", &now));
  churn_observation_use_case_impl_->SetWindowIdentifier(now);

  // Cohort churn use case should have updated the churn_active_status object
  // prior to observation check in.
  churn_active_status_->UpdateValue(now);

  // Generate observation windows.
  churn_observation_use_case_impl_->GenerateImportRequestBody();

  // Validate observation windows are expected.
  EXPECT_EQ(churn_observation_use_case_impl_->GetObservationPeriod(0),
            "202301-202303");
  EXPECT_EQ(churn_observation_use_case_impl_->GetObservationPeriod(1),
            "202212-202302");
  EXPECT_EQ(churn_observation_use_case_impl_->GetObservationPeriod(2),
            "202211-202301");
}

TEST_F(ChurnObservationUseCaseImplTest,
       FailedToSetObservationPeriodActiveTsInvalid) {
  // Attempt to generate import request body without setting the window id or
  // active ts.
  churn_observation_use_case_impl_->ClearSavedState();
  EXPECT_EQ(churn_observation_use_case_impl_->GenerateImportRequestBody(),
            absl::nullopt);
}

TEST_F(ChurnObservationUseCaseImplTest, ObservationPeriodZeroIsMonthlyActive) {
  // Update reporting date to 1 month after current active ts.
  base::Time ts = GetNextMonth(churn_observation_use_case_impl_->GetActiveTs());
  churn_observation_use_case_impl_->SetWindowIdentifier(ts);

  // Update 28 bit churn active status to have been active last month.
  // In the constructor we already initialize the active status value to reflect
  // the device being active starting Jan 2023.
  // We can simply update the value to reflect active in Feb 2023.
  churn_active_status_->UpdateValue(ts);

  auto observation_req =
      churn_observation_use_case_impl_->GenerateImportRequestBody();

  // Validate observation period 0 has monthly active.
  EXPECT_TRUE(observation_req.value()
                  .import_data(0)
                  .churn_observation_metadata()
                  .monthly_active_status());
  EXPECT_FALSE(observation_req.value()
                   .import_data(0)
                   .churn_observation_metadata()
                   .yearly_active_status());
  EXPECT_EQ(
      observation_req.value()
          .import_data(0)
          .churn_observation_metadata()
          .first_active_during_cohort(),
      ChurnObservationMetadata_FirstActiveDuringCohort_FIRST_ACTIVE_IN_MONTHLY_COHORT);

  // Validate observation period 1.
  EXPECT_FALSE(observation_req.value()
                   .import_data(1)
                   .churn_observation_metadata()
                   .monthly_active_status());
  EXPECT_FALSE(observation_req.value()
                   .import_data(1)
                   .churn_observation_metadata()
                   .yearly_active_status());
  EXPECT_EQ(
      observation_req.value()
          .import_data(1)
          .churn_observation_metadata()
          .first_active_during_cohort(),
      ChurnObservationMetadata_FirstActiveDuringCohort_EXISTED_OR_NOT_ACTIVE_YET);

  // Validate observation period 2.
  EXPECT_FALSE(observation_req.value()
                   .import_data(2)
                   .churn_observation_metadata()
                   .monthly_active_status());
  EXPECT_FALSE(observation_req.value()
                   .import_data(2)
                   .churn_observation_metadata()
                   .yearly_active_status());
  EXPECT_EQ(
      observation_req.value()
          .import_data(2)
          .churn_observation_metadata()
          .first_active_during_cohort(),
      ChurnObservationMetadata_FirstActiveDuringCohort_EXISTED_OR_NOT_ACTIVE_YET);
}

TEST_F(ChurnObservationUseCaseImplTest, ObservationPeriodOneIsMonthlyActive) {
  // Update reporting date to 2 month after current active ts.
  base::Time ts = GetNextMonth(
      GetNextMonth(churn_observation_use_case_impl_->GetActiveTs()));
  churn_observation_use_case_impl_->SetWindowIdentifier(ts);

  // Update 28 bit churn active status to have been active 2 months ago.
  // In the constructor we already initialize the active status value to reflect
  // the device being active starting Jan 2023.
  // We can simply update the value to reflect active in Mar 2023.
  churn_active_status_->UpdateValue(ts);

  auto observation_req =
      churn_observation_use_case_impl_->GenerateImportRequestBody();

  // Validate observation period 0.
  EXPECT_FALSE(observation_req.value()
                   .import_data(0)
                   .churn_observation_metadata()
                   .monthly_active_status());
  EXPECT_FALSE(observation_req.value()
                   .import_data(0)
                   .churn_observation_metadata()
                   .yearly_active_status());
  EXPECT_EQ(
      observation_req.value()
          .import_data(0)
          .churn_observation_metadata()
          .first_active_during_cohort(),
      ChurnObservationMetadata_FirstActiveDuringCohort_EXISTED_OR_NOT_ACTIVE_YET);

  // Validate observation period 1 has monthly active.
  EXPECT_TRUE(observation_req.value()
                  .import_data(1)
                  .churn_observation_metadata()
                  .monthly_active_status());
  EXPECT_FALSE(observation_req.value()
                   .import_data(1)
                   .churn_observation_metadata()
                   .yearly_active_status());
  EXPECT_EQ(
      observation_req.value()
          .import_data(1)
          .churn_observation_metadata()
          .first_active_during_cohort(),
      ChurnObservationMetadata_FirstActiveDuringCohort_FIRST_ACTIVE_IN_MONTHLY_COHORT);

  // Validate observation period 2.
  EXPECT_FALSE(observation_req.value()
                   .import_data(2)
                   .churn_observation_metadata()
                   .monthly_active_status());
  EXPECT_FALSE(observation_req.value()
                   .import_data(2)
                   .churn_observation_metadata()
                   .yearly_active_status());
  EXPECT_EQ(
      observation_req.value()
          .import_data(2)
          .churn_observation_metadata()
          .first_active_during_cohort(),
      ChurnObservationMetadata_FirstActiveDuringCohort_EXISTED_OR_NOT_ACTIVE_YET);
}

TEST_F(ChurnObservationUseCaseImplTest, ObservationPeriodTwoIsMonthlyActive) {
  // Update reporting date to 3 month after current active ts.
  base::Time ts = GetNextMonth(GetNextMonth(
      GetNextMonth(churn_observation_use_case_impl_->GetActiveTs())));
  churn_observation_use_case_impl_->SetWindowIdentifier(ts);

  // Update 28 bit churn active status to have been active 3 months ago.
  // In the constructor we already initialize the active status value to reflect
  // the device being active starting Jan 2023.
  // We can simply update the value to reflect active in Apr 2023.
  churn_active_status_->UpdateValue(ts);

  auto observation_req =
      churn_observation_use_case_impl_->GenerateImportRequestBody();

  // Validate observation period 0.
  EXPECT_FALSE(observation_req.value()
                   .import_data(0)
                   .churn_observation_metadata()
                   .monthly_active_status());
  EXPECT_FALSE(observation_req.value()
                   .import_data(0)
                   .churn_observation_metadata()
                   .yearly_active_status());
  EXPECT_EQ(
      observation_req.value()
          .import_data(0)
          .churn_observation_metadata()
          .first_active_during_cohort(),
      ChurnObservationMetadata_FirstActiveDuringCohort_EXISTED_OR_NOT_ACTIVE_YET);

  // Validate observation period 1.
  EXPECT_FALSE(observation_req.value()
                   .import_data(1)
                   .churn_observation_metadata()
                   .monthly_active_status());
  EXPECT_FALSE(observation_req.value()
                   .import_data(1)
                   .churn_observation_metadata()
                   .yearly_active_status());
  EXPECT_EQ(
      observation_req.value()
          .import_data(1)
          .churn_observation_metadata()
          .first_active_during_cohort(),
      ChurnObservationMetadata_FirstActiveDuringCohort_EXISTED_OR_NOT_ACTIVE_YET);

  // Validate observation period 2 has monthly active.
  EXPECT_TRUE(observation_req.value()
                  .import_data(2)
                  .churn_observation_metadata()
                  .monthly_active_status());
  EXPECT_FALSE(observation_req.value()
                   .import_data(2)
                   .churn_observation_metadata()
                   .yearly_active_status());
  EXPECT_EQ(
      observation_req.value()
          .import_data(2)
          .churn_observation_metadata()
          .first_active_during_cohort(),
      ChurnObservationMetadata_FirstActiveDuringCohort_FIRST_ACTIVE_IN_MONTHLY_COHORT);
}

TEST_F(ChurnObservationUseCaseImplTest,
       AllOberservationPeriodsHaveFalseMonthlyActive) {
  // Set up function only updates the churn active status once.
  // This should mean the past 17 months are all inactive, set to 0 bit.
  auto observation_req =
      churn_observation_use_case_impl_->GenerateImportRequestBody();

  // Validate observation period 0 has false monthly active.
  EXPECT_FALSE(observation_req.value()
                   .import_data(0)
                   .churn_observation_metadata()
                   .monthly_active_status());
  EXPECT_FALSE(observation_req.value()
                   .import_data(0)
                   .churn_observation_metadata()
                   .yearly_active_status());
  EXPECT_EQ(
      observation_req.value()
          .import_data(0)
          .churn_observation_metadata()
          .first_active_during_cohort(),
      ChurnObservationMetadata_FirstActiveDuringCohort_EXISTED_OR_NOT_ACTIVE_YET);

  // Validate observation period 1 has false monthly active.
  EXPECT_FALSE(observation_req.value()
                   .import_data(1)
                   .churn_observation_metadata()
                   .monthly_active_status());
  EXPECT_FALSE(observation_req.value()
                   .import_data(1)
                   .churn_observation_metadata()
                   .yearly_active_status());
  EXPECT_EQ(
      observation_req.value()
          .import_data(1)
          .churn_observation_metadata()
          .first_active_during_cohort(),
      ChurnObservationMetadata_FirstActiveDuringCohort_EXISTED_OR_NOT_ACTIVE_YET);

  // Validate observation period 2 has false monthly active.
  EXPECT_FALSE(observation_req.value()
                   .import_data(2)
                   .churn_observation_metadata()
                   .monthly_active_status());
  EXPECT_FALSE(observation_req.value()
                   .import_data(2)
                   .churn_observation_metadata()
                   .yearly_active_status());
  EXPECT_EQ(
      observation_req.value()
          .import_data(2)
          .churn_observation_metadata()
          .first_active_during_cohort(),
      ChurnObservationMetadata_FirstActiveDuringCohort_EXISTED_OR_NOT_ACTIVE_YET);
}

TEST_F(ChurnObservationUseCaseImplTest, ObservationPeriodZeroIsYearlyActive) {
  // Update reporting date to 12 + 1 months after current active month.
  // It sets it 13 months ahead because the first active on cohort year
  // has a 13 month look back from the current month.
  base::Time ts = GetNextYear(
      GetNextMonth(churn_observation_use_case_impl_->GetActiveTs()));
  churn_observation_use_case_impl_->SetWindowIdentifier(ts);

  // Update 28 bit churn active status to be active 13 months into the future.
  // In the constructor we already initialize the active status value to reflect
  // the device being active starting Jan 2023.
  // We can simply update the value to reflect active in Feb 2024.
  churn_active_status_->UpdateValue(ts);

  auto observation_req =
      churn_observation_use_case_impl_->GenerateImportRequestBody();

  // Validate observation period 0 has yearly first active.
  EXPECT_FALSE(observation_req.value()
                   .import_data(0)
                   .churn_observation_metadata()
                   .monthly_active_status());
  EXPECT_TRUE(observation_req.value()
                  .import_data(0)
                  .churn_observation_metadata()
                  .yearly_active_status());
  EXPECT_EQ(
      observation_req.value()
          .import_data(0)
          .churn_observation_metadata()
          .first_active_during_cohort(),
      ChurnObservationMetadata_FirstActiveDuringCohort_FIRST_ACTIVE_IN_YEARLY_COHORT);

  // Validate observation period 1 has no actives.
  EXPECT_FALSE(observation_req.value()
                   .import_data(1)
                   .churn_observation_metadata()
                   .monthly_active_status());
  EXPECT_FALSE(observation_req.value()
                   .import_data(1)
                   .churn_observation_metadata()
                   .yearly_active_status());
  EXPECT_EQ(
      observation_req.value()
          .import_data(1)
          .churn_observation_metadata()
          .first_active_during_cohort(),
      ChurnObservationMetadata_FirstActiveDuringCohort_EXISTED_OR_NOT_ACTIVE_YET);

  // Validate observation period 2 has no actives.
  EXPECT_FALSE(observation_req.value()
                   .import_data(2)
                   .churn_observation_metadata()
                   .monthly_active_status());
  EXPECT_FALSE(observation_req.value()
                   .import_data(2)
                   .churn_observation_metadata()
                   .yearly_active_status());
  EXPECT_EQ(
      observation_req.value()
          .import_data(2)
          .churn_observation_metadata()
          .first_active_during_cohort(),
      ChurnObservationMetadata_FirstActiveDuringCohort_EXISTED_OR_NOT_ACTIVE_YET);
}

TEST_F(ChurnObservationUseCaseImplTest, ObservationPeriodOneHasYearlyActive) {
  // Update reporting date to 12 + 2 months after current active month.
  // It sets the date 14 months ahead because the first active for the previous
  // month observation window has a 14 month look back from the current month.
  base::Time ts = GetNextYear(GetNextMonth(
      GetNextMonth(churn_observation_use_case_impl_->GetActiveTs())));
  churn_observation_use_case_impl_->SetWindowIdentifier(ts);

  // Update 28 bit churn active status to be active 14 months into the future.
  // In the constructor we already initialize the active status value to reflect
  // the device being active starting Jan 2023.
  // We can simply update the value to reflect active in Mar 2024.
  churn_active_status_->UpdateValue(ts);

  auto observation_req =
      churn_observation_use_case_impl_->GenerateImportRequestBody();

  // Validate observation period 0 has noactive.
  EXPECT_FALSE(observation_req.value()
                   .import_data(0)
                   .churn_observation_metadata()
                   .monthly_active_status());
  EXPECT_FALSE(observation_req.value()
                   .import_data(0)
                   .churn_observation_metadata()
                   .yearly_active_status());
  EXPECT_EQ(
      observation_req.value()
          .import_data(0)
          .churn_observation_metadata()
          .first_active_during_cohort(),
      ChurnObservationMetadata_FirstActiveDuringCohort_EXISTED_OR_NOT_ACTIVE_YET);

  // Validate observation period 1 has yearly first active.
  EXPECT_FALSE(observation_req.value()
                   .import_data(1)
                   .churn_observation_metadata()
                   .monthly_active_status());
  EXPECT_TRUE(observation_req.value()
                  .import_data(1)
                  .churn_observation_metadata()
                  .yearly_active_status());
  EXPECT_EQ(
      observation_req.value()
          .import_data(1)
          .churn_observation_metadata()
          .first_active_during_cohort(),
      ChurnObservationMetadata_FirstActiveDuringCohort_FIRST_ACTIVE_IN_YEARLY_COHORT);

  // Validate observation period 2 has no actives.
  EXPECT_FALSE(observation_req.value()
                   .import_data(2)
                   .churn_observation_metadata()
                   .monthly_active_status());
  EXPECT_FALSE(observation_req.value()
                   .import_data(2)
                   .churn_observation_metadata()
                   .yearly_active_status());
  EXPECT_EQ(
      observation_req.value()
          .import_data(2)
          .churn_observation_metadata()
          .first_active_during_cohort(),
      ChurnObservationMetadata_FirstActiveDuringCohort_EXISTED_OR_NOT_ACTIVE_YET);
}

TEST_F(ChurnObservationUseCaseImplTest, ObservationPeriodTwoHasYearlyActive) {
  // Update reporting date to 12 + 3 months after current active month.
  // It sets the date 15 months ahead because the first active for the previous
  // month observation window has a 15 month look back from the current month.
  base::Time ts = GetNextYear(GetNextMonth(GetNextMonth(
      GetNextMonth(churn_observation_use_case_impl_->GetActiveTs()))));
  churn_observation_use_case_impl_->SetWindowIdentifier(ts);

  // Update 28 bit churn active status to be active 15 months into the future.
  // In the constructor we already initialize the active status value to reflect
  // the device being active starting Jan 2023.
  // We can simply update the value to reflect active in Apr 2024.
  churn_active_status_->UpdateValue(ts);

  auto observation_req =
      churn_observation_use_case_impl_->GenerateImportRequestBody();

  // Validate observation period 0 has no actives.
  EXPECT_FALSE(observation_req.value()
                   .import_data(0)
                   .churn_observation_metadata()
                   .monthly_active_status());
  EXPECT_FALSE(observation_req.value()
                   .import_data(0)
                   .churn_observation_metadata()
                   .yearly_active_status());
  EXPECT_EQ(
      observation_req.value()
          .import_data(0)
          .churn_observation_metadata()
          .first_active_during_cohort(),
      ChurnObservationMetadata_FirstActiveDuringCohort_EXISTED_OR_NOT_ACTIVE_YET);

  // Validate observation period 1 has no actives.
  EXPECT_FALSE(observation_req.value()
                   .import_data(1)
                   .churn_observation_metadata()
                   .monthly_active_status());
  EXPECT_FALSE(observation_req.value()
                   .import_data(1)
                   .churn_observation_metadata()
                   .yearly_active_status());
  EXPECT_EQ(
      observation_req.value()
          .import_data(1)
          .churn_observation_metadata()
          .first_active_during_cohort(),
      ChurnObservationMetadata_FirstActiveDuringCohort_EXISTED_OR_NOT_ACTIVE_YET);

  // Validate observation period 2 has yearly first active.
  EXPECT_FALSE(observation_req.value()
                   .import_data(2)
                   .churn_observation_metadata()
                   .monthly_active_status());
  EXPECT_TRUE(observation_req.value()
                  .import_data(2)
                  .churn_observation_metadata()
                  .yearly_active_status());
  EXPECT_EQ(
      observation_req.value()
          .import_data(2)
          .churn_observation_metadata()
          .first_active_during_cohort(),
      ChurnObservationMetadata_FirstActiveDuringCohort_FIRST_ACTIVE_IN_YEARLY_COHORT);
}

TEST_F(ChurnObservationUseCaseImplTest,
       AllObservationPeriodsHaveFalseYearlyActive) {
  // Update reporting date to 12 months after the current active month.
  base::Time ts = GetNextYear(churn_observation_use_case_impl_->GetActiveTs());
  churn_observation_use_case_impl_->SetWindowIdentifier(ts);

  // Update 28 bit churn active status to be active 12 months from the last
  // active ts.
  churn_active_status_->UpdateValue(ts);

  auto observation_req =
      churn_observation_use_case_impl_->GenerateImportRequestBody();

  // Validate observation period 0 has false yearly active.
  EXPECT_FALSE(observation_req.value()
                   .import_data(0)
                   .churn_observation_metadata()
                   .monthly_active_status());
  EXPECT_FALSE(observation_req.value()
                   .import_data(0)
                   .churn_observation_metadata()
                   .yearly_active_status());
  EXPECT_EQ(
      observation_req.value()
          .import_data(0)
          .churn_observation_metadata()
          .first_active_during_cohort(),
      ChurnObservationMetadata_FirstActiveDuringCohort_EXISTED_OR_NOT_ACTIVE_YET);

  // Validate observation period 1 has false yearly active.
  EXPECT_FALSE(observation_req.value()
                   .import_data(1)
                   .churn_observation_metadata()
                   .monthly_active_status());
  EXPECT_FALSE(observation_req.value()
                   .import_data(1)
                   .churn_observation_metadata()
                   .yearly_active_status());
  EXPECT_EQ(
      observation_req.value()
          .import_data(1)
          .churn_observation_metadata()
          .first_active_during_cohort(),
      ChurnObservationMetadata_FirstActiveDuringCohort_EXISTED_OR_NOT_ACTIVE_YET);

  // Validate observation period 2 has false yearly active.
  EXPECT_FALSE(observation_req.value()
                   .import_data(2)
                   .churn_observation_metadata()
                   .monthly_active_status());
  EXPECT_FALSE(observation_req.value()
                   .import_data(2)
                   .churn_observation_metadata()
                   .yearly_active_status());
  EXPECT_EQ(
      observation_req.value()
          .import_data(2)
          .churn_observation_metadata()
          .first_active_during_cohort(),
      ChurnObservationMetadata_FirstActiveDuringCohort_EXISTED_OR_NOT_ACTIVE_YET);
}

TEST_F(ChurnObservationUseCaseImplTest,
       ObservationPeriodHasMonthlyActiveButNotMonthlyFirstActive) {
  // Update the churn active status for 2 months in a row.
  // The first active week is set in the constructor as Jan 2023.
  // This section of code updates the active status object to be
  // active in Feb 2023, and March 2023.
  base::Time ts = churn_observation_use_case_impl_->GetActiveTs();

  base::Time month_1 = GetNextMonth(ts);
  base::Time month_2 = GetNextMonth(month_1);

  churn_active_status_->UpdateValue(month_1);
  churn_active_status_->UpdateValue(month_2);

  // Update the current date to reflect the current active status in March 2023.
  churn_observation_use_case_impl_->SetWindowIdentifier(month_2);

  auto observation_req =
      churn_observation_use_case_impl_->GenerateImportRequestBody();

  // Validate observation period 0 is monthly active but is not monthly first
  // active. This is because monthly active from March 2023, checks whether the
  // device was active in Feb 2023, which is true.
  // The device was first active in Jan 2023, which does not align with March
  // 2023 monthly cohort period of Feb 2023.
  EXPECT_TRUE(observation_req.value()
                  .import_data(0)
                  .churn_observation_metadata()
                  .monthly_active_status());
  EXPECT_FALSE(observation_req.value()
                   .import_data(0)
                   .churn_observation_metadata()
                   .yearly_active_status());
  EXPECT_EQ(
      observation_req.value()
          .import_data(0)
          .churn_observation_metadata()
          .first_active_during_cohort(),
      ChurnObservationMetadata_FirstActiveDuringCohort_EXISTED_OR_NOT_ACTIVE_YET);
}

TEST_F(ChurnObservationUseCaseImplTest,
       ObservationPeriodHasYearlyActiveButNotYearlyFirstActive) {
  // Update the churn active status for 2 months in a row, followed by being
  // active 12 months later. The first active week is set in the constructor as
  // Jan 2023. This section of code updates the active status object to be
  // active in Feb 2023, March 2023, and then March 2024.
  base::Time ts = churn_observation_use_case_impl_->GetActiveTs();

  base::Time month_1 = GetNextMonth(ts);
  base::Time month_2 = GetNextMonth(month_1);
  base::Time month_14 = GetNextYear(month_2);

  churn_active_status_->UpdateValue(month_1);
  churn_active_status_->UpdateValue(month_2);
  churn_active_status_->UpdateValue(month_14);

  // Update the current date to reflect the current active status in March 2024.
  churn_observation_use_case_impl_->SetWindowIdentifier(month_14);

  auto observation_req =
      churn_observation_use_case_impl_->GenerateImportRequestBody();

  // Validate observation period 0 is yearly active but is not yearly first
  // active. This is because yearly active from March 2024, checks whether the
  // device was active in Feb 2023, which is true.
  // The device was first active in Jan 2023, which does not align with March
  // 2024 yearly cohort period of Feb 2023.
  EXPECT_FALSE(observation_req.value()
                   .import_data(0)
                   .churn_observation_metadata()
                   .monthly_active_status());
  EXPECT_TRUE(observation_req.value()
                  .import_data(0)
                  .churn_observation_metadata()
                  .yearly_active_status());
  EXPECT_EQ(
      observation_req.value()
          .import_data(0)
          .churn_observation_metadata()
          .first_active_during_cohort(),
      ChurnObservationMetadata_FirstActiveDuringCohort_EXISTED_OR_NOT_ACTIVE_YET);
}

TEST_F(ChurnObservationUseCaseImplTest, EmptyActiveStatusIsInvalid) {
  churn_active_status_->SetValueForTesting(0);

  auto observation_req =
      churn_observation_use_case_impl_->GenerateImportRequestBody();

  EXPECT_FALSE(observation_req.has_value());
}

TEST_F(ChurnObservationUseCaseImplTest, ValueGreaterThanCurrentActiveMonth) {
  // Initialize active status object with fake value.
  std::bitset<ChurnActiveStatus::kChurnBitSize> max_28_bits(
      "1111111111111111111111111111");
  churn_active_status_->SetValueForTesting(max_28_bits);

  auto observation_req =
      churn_observation_use_case_impl_->GenerateImportRequestBody();

  EXPECT_FALSE(observation_req.has_value());
}

// TODO(hirthanan): Add parameterized tests first active week at start of month,
// end of month, start of year, end of year. Test active status object is all
// actives, all not active, some active. Test other inputs and boundary cases.

}  // namespace ash::device_activity
