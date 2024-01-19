// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/report/device_metrics/churn/active_status.h"

#include "base/check.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chromeos/ash/components/report/proto/fresnel_service.pb.h"
#include "chromeos/ash/components/report/report_controller.h"
#include "chromeos/ash/components/report/utils/test_utils.h"
#include "chromeos/ash/components/report/utils/time_utils.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::report::device_metrics {

class ActiveStatusTest : public testing::Test {
 protected:
  void SetUp() override {
    // Set the mock time to |kFakeTimeNow|.
    base::Time ts;
    ASSERT_TRUE(base::Time::FromUTCString(utils::kFakeTimeNowString, &ts));
    task_environment_.AdvanceClock(ts - base::Time::Now());

    // Register all related local state prefs.
    ReportController::RegisterPrefs(local_state_.registry());

    system::StatisticsProvider::SetTestProvider(&statistics_provider_);

    active_status_ = std::make_unique<ActiveStatus>(&local_state_);
  }

  void TearDown() override { active_status_.reset(); }

  ActiveStatus* GetActiveStatus() { return active_status_.get(); }

  base::Time GetFakeTimeNow() { return base::Time::Now(); }

 protected:
  void SetActivateDate(const std::string& activate_date) {
    statistics_provider_.SetMachineStatistic(system::kActivateDateKey,
                                             activate_date);
  }

  std::optional<base::Time> GetFirstActiveWeekForTest() {
    return utils::GetFirstActiveWeek();
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 private:
  TestingPrefServiceSimple local_state_;
  system::FakeStatisticsProvider statistics_provider_;
  std::unique_ptr<ActiveStatus> active_status_;
};

TEST_F(ActiveStatusTest, SetAndGetValue) {
  // Set a value using SetValue method.
  GetActiveStatus()->SetValue(42);

  // Verify that GetValue returns the same value.
  EXPECT_EQ(GetActiveStatus()->GetValue(), 42);
}

TEST_F(ActiveStatusTest, CalculateValue) {
  // Verify initial active status value updates to current month timestamp.
  base::Time ts = GetFakeTimeNow();
  std::optional<int> value = GetActiveStatus()->CalculateNewValue(ts);
  EXPECT_TRUE(value.has_value());
  EXPECT_EQ(value.value(), 72351745);

  // Verify uninitialized time object returns nullopt.
  base::Time uninitialized_ts;
  value = GetActiveStatus()->CalculateNewValue(uninitialized_ts);
  EXPECT_FALSE(value.has_value());

  // Verify unix epoch timestamp returns nullopt.
  base::Time unix_epoch_ts = base::Time::UnixEpoch();
  value = GetActiveStatus()->CalculateNewValue(unix_epoch_ts);
  EXPECT_FALSE(value.has_value());

  // Verify timestamp before |kActiveStatusInceptionDate| returns nullopt.
  base::Time inception_ts;
  EXPECT_TRUE(base::Time::FromUTCString(
      ActiveStatus::kActiveStatusInceptionDate, &inception_ts));
  value = GetActiveStatus()->CalculateNewValue(inception_ts - base::Days(1));
  EXPECT_FALSE(value.has_value());
}

TEST_F(ActiveStatusTest, CalculateNewValueSameMonthAsPreviousReturnsNullopt) {
  base::Time current_month_ts = GetFakeTimeNow();

  // Set up the initial active status value and the current month timestamp.
  std::optional<int> value =
      GetActiveStatus()->CalculateNewValue(current_month_ts);
  GetActiveStatus()->SetValue(value.value());

  // Calculate the new value for the same month as the previous one.
  value = GetActiveStatus()->CalculateNewValue(current_month_ts);

  // Ensure that the new value is not calculated and nullopt is returned.
  EXPECT_FALSE(value.has_value());
}

TEST_F(ActiveStatusTest, CalculateNewValueNewMonthReturnsUpdatedValue) {
  base::Time current_month_ts = GetFakeTimeNow();

  // Set up the initial active status value and the current month timestamp.
  std::optional<int> value =
      GetActiveStatus()->CalculateNewValue(current_month_ts);
  GetActiveStatus()->SetValue(value.value());

  // Set the previous month's timestamp to simulate a new month.
  base::Time next_month_ts = utils::GetNextMonth(current_month_ts).value();

  // Calculate the new value for the new month.
  std::optional<int> new_value =
      GetActiveStatus()->CalculateNewValue(next_month_ts);

  // Ensure the updated new value is generated based on the newer timestamp.
  EXPECT_TRUE(new_value.has_value());
  EXPECT_GT(new_value.value(), value.value());
}

TEST_F(ActiveStatusTest, GetCurrentActiveMonthTimestamp) {
  ASSERT_EQ(GetActiveStatus()->GetValue(), 0);

  std::optional<base::Time> current_active_month_ts =
      GetActiveStatus()->GetCurrentActiveMonthTimestamp();

  // Return inception ts since active status value has never been updated (= 0).
  base::Time inception_ts;
  EXPECT_TRUE(base::Time::FromUTCString(
      ActiveStatus::kActiveStatusInceptionDate, &inception_ts));
  EXPECT_TRUE(current_active_month_ts.has_value());
  EXPECT_EQ(current_active_month_ts.value(), inception_ts);

  // Set value to a fake time.
  int val = GetActiveStatus()->CalculateNewValue(GetFakeTimeNow()).value();
  GetActiveStatus()->SetValue(val);

  // Get the current active month timestamp. The value was set to
  // |GetFakeTimeNow()|.
  current_active_month_ts = GetActiveStatus()->GetCurrentActiveMonthTimestamp();
  EXPECT_TRUE(current_active_month_ts.has_value());
  EXPECT_EQ(current_active_month_ts.value(), GetFakeTimeNow());
}

TEST_F(ActiveStatusTest, CalculateCohortMetadataReturnExpectedMetadata) {
  // Set up the initial active status value and the current month timestamp.
  base::Time current_month_ts = GetFakeTimeNow();
  int val = GetActiveStatus()->CalculateNewValue(current_month_ts).value();
  GetActiveStatus()->SetValue(val);

  // Simulate a new month when calculating churn cohort metadata.
  base::Time next_month_ts = utils::GetNextMonth(current_month_ts).value();

  // Calculate the cohort metadata.
  ChurnCohortMetadata metadata =
      GetActiveStatus()->CalculateCohortMetadata(next_month_ts).value();

  // Ensure that the metadata is calculated correctly.
  EXPECT_EQ(metadata.active_status_value(), 72613891);
  EXPECT_FALSE(metadata.is_first_active_in_cohort());

  // Ensure local state doesn't get updated on calculate cohort metadata call.
  EXPECT_EQ(GetActiveStatus()->GetCurrentActiveMonthTimestamp(),
            current_month_ts);
}

TEST_F(ActiveStatusTest, CalculateCohortMetadataReturnFirstActive) {
  base::Time current_month_ts = GetFakeTimeNow();

  // Initialize ActivateDate VPD field to be first active in current month ts.
  SetActivateDate("2023-02");

  // Calculate the cohort metadata.
  ChurnCohortMetadata metadata =
      GetActiveStatus()->CalculateCohortMetadata(current_month_ts).value();

  // Ensure that the metadata is calculated correctly.
  EXPECT_EQ(metadata.active_status_value(), 72351745);
  EXPECT_TRUE(metadata.is_first_active_in_cohort());
}

TEST_F(ActiveStatusTest,
       CalculateCohortMetadataSameMonthAsPreviousReturnsNullopt) {
  // Set up the initial active status value and the current month timestamp.
  base::Time current_month_ts = GetFakeTimeNow();
  int val = GetActiveStatus()->CalculateNewValue(current_month_ts).value();
  GetActiveStatus()->SetValue(val);

  // Calculate the cohort metadata.
  std::optional<ChurnCohortMetadata> metadata =
      GetActiveStatus()->CalculateCohortMetadata(current_month_ts);

  // Ensure the metadata is not regenerated for same |current_month_ts|.
  EXPECT_FALSE(metadata.has_value());
}

TEST_F(ActiveStatusTest, CalculateObservationMetadataReturnsExpectedMetadata) {
  // Setup initial value to be last active in Jan-2023.
  // Value represents device was active each of the 18 months prior.
  // Represents binary: 0100010100 111111111111111111
  int cur_value = 72613887;
  base::Time cur_ts = GetFakeTimeNow();
  GetActiveStatus()->SetValue(cur_value);

  // Calculate the observation metadata for 3 periods.
  std::optional<ChurnObservationMetadata> metadata_0 =
      GetActiveStatus()->CalculateObservationMetadata(cur_ts, 0);
  std::optional<ChurnObservationMetadata> metadata_1 =
      GetActiveStatus()->CalculateObservationMetadata(cur_ts, 1);
  std::optional<ChurnObservationMetadata> metadata_2 =
      GetActiveStatus()->CalculateObservationMetadata(cur_ts, 2);

  // Ensure that the metadata is calculated correctly.
  EXPECT_TRUE(metadata_0->monthly_active_status());
  EXPECT_TRUE(metadata_1->monthly_active_status());
  EXPECT_TRUE(metadata_2->monthly_active_status());
  EXPECT_TRUE(metadata_0->yearly_active_status());
  EXPECT_TRUE(metadata_1->yearly_active_status());
  EXPECT_TRUE(metadata_2->yearly_active_status());
  EXPECT_EQ(
      metadata_0->first_active_during_cohort(),
      ChurnObservationMetadata_FirstActiveDuringCohort_EXISTED_OR_NOT_ACTIVE_YET);
  EXPECT_EQ(
      metadata_1->first_active_during_cohort(),
      ChurnObservationMetadata_FirstActiveDuringCohort_EXISTED_OR_NOT_ACTIVE_YET);
  EXPECT_EQ(
      metadata_2->first_active_during_cohort(),
      ChurnObservationMetadata_FirstActiveDuringCohort_EXISTED_OR_NOT_ACTIVE_YET);
}

TEST_F(ActiveStatusTest,
       CalculateObservationMetadataReturnsExpectedMetadataActiveMonths) {
  // Setup initial value to be last active in Jan-2023.
  // Value represents device was active each of the 18 months prior.
  // Represents binary: 0100010100 111101111111111101
  int cur_value = 72605693;
  base::Time cur_ts = GetFakeTimeNow();
  GetActiveStatus()->SetValue(cur_value);

  // Calculate the observation metadata for 3 periods.
  std::optional<ChurnObservationMetadata> metadata_0 =
      GetActiveStatus()->CalculateObservationMetadata(cur_ts, 0);
  std::optional<ChurnObservationMetadata> metadata_1 =
      GetActiveStatus()->CalculateObservationMetadata(cur_ts, 1);
  std::optional<ChurnObservationMetadata> metadata_2 =
      GetActiveStatus()->CalculateObservationMetadata(cur_ts, 2);

  // Ensure that the metadata is calculated correctly.
  EXPECT_FALSE(metadata_0->monthly_active_status());
  EXPECT_TRUE(metadata_1->monthly_active_status());
  EXPECT_TRUE(metadata_2->monthly_active_status());
  EXPECT_FALSE(metadata_0->yearly_active_status());
  EXPECT_TRUE(metadata_1->yearly_active_status());
  EXPECT_TRUE(metadata_2->yearly_active_status());
  EXPECT_EQ(
      metadata_0->first_active_during_cohort(),
      ChurnObservationMetadata_FirstActiveDuringCohort_EXISTED_OR_NOT_ACTIVE_YET);
  EXPECT_EQ(
      metadata_1->first_active_during_cohort(),
      ChurnObservationMetadata_FirstActiveDuringCohort_EXISTED_OR_NOT_ACTIVE_YET);
  EXPECT_EQ(
      metadata_2->first_active_during_cohort(),
      ChurnObservationMetadata_FirstActiveDuringCohort_EXISTED_OR_NOT_ACTIVE_YET);
}

TEST_F(ActiveStatusTest,
       CalculateObservationMetadataReturnsExpectedMetadataFirstActiveYearly) {
  // Setup initial value to be last active in Jan-2023.
  // Value represents device was active each of the 18 months prior.
  // Represents binary: 0100010100 000010000000000001
  int cur_value = 72359937;
  base::Time cur_ts = GetFakeTimeNow();
  GetActiveStatus()->SetValue(cur_value);

  SetActivateDate("2021-52");

  // Calculate the observation metadata for 3 periods.
  std::optional<ChurnObservationMetadata> metadata_0 =
      GetActiveStatus()->CalculateObservationMetadata(cur_ts, 0);
  std::optional<ChurnObservationMetadata> metadata_1 =
      GetActiveStatus()->CalculateObservationMetadata(cur_ts, 1);
  std::optional<ChurnObservationMetadata> metadata_2 =
      GetActiveStatus()->CalculateObservationMetadata(cur_ts, 2);

  // Ensure that the metadata is calculated correctly.
  EXPECT_FALSE(metadata_0->monthly_active_status());
  EXPECT_FALSE(metadata_1->monthly_active_status());
  EXPECT_FALSE(metadata_2->monthly_active_status());
  EXPECT_TRUE(metadata_0->yearly_active_status());
  EXPECT_FALSE(metadata_1->yearly_active_status());
  EXPECT_FALSE(metadata_2->yearly_active_status());
  EXPECT_EQ(
      metadata_0->first_active_during_cohort(),
      ChurnObservationMetadata_FirstActiveDuringCohort_FIRST_ACTIVE_IN_YEARLY_COHORT);
  EXPECT_EQ(
      metadata_1->first_active_during_cohort(),
      ChurnObservationMetadata_FirstActiveDuringCohort_EXISTED_OR_NOT_ACTIVE_YET);
  EXPECT_EQ(
      metadata_2->first_active_during_cohort(),
      ChurnObservationMetadata_FirstActiveDuringCohort_EXISTED_OR_NOT_ACTIVE_YET);
}

TEST_F(ActiveStatusTest,
       CalculateObservationMetadataReturnsExpectedMetadataFirstActiveMonthly) {
  // Setup initial value to be last active in Jan-2023.
  // Value represents device was active each of the 18 months prior.
  // Represents binary: 0100010100 000000000000000011
  int cur_value = 72351747;
  base::Time cur_ts = GetFakeTimeNow();
  GetActiveStatus()->SetValue(cur_value);

  SetActivateDate("2022-52");

  // Calculate the observation metadata for 3 periods.
  std::optional<ChurnObservationMetadata> metadata_0 =
      GetActiveStatus()->CalculateObservationMetadata(cur_ts, 0);
  std::optional<ChurnObservationMetadata> metadata_1 =
      GetActiveStatus()->CalculateObservationMetadata(cur_ts, 1);
  std::optional<ChurnObservationMetadata> metadata_2 =
      GetActiveStatus()->CalculateObservationMetadata(cur_ts, 2);

  // Ensure that the metadata is calculated correctly.
  EXPECT_TRUE(metadata_0->monthly_active_status());
  EXPECT_FALSE(metadata_1->monthly_active_status());
  EXPECT_FALSE(metadata_2->monthly_active_status());
  EXPECT_FALSE(metadata_0->yearly_active_status());
  EXPECT_FALSE(metadata_1->yearly_active_status());
  EXPECT_FALSE(metadata_2->yearly_active_status());
  EXPECT_EQ(
      metadata_0->first_active_during_cohort(),
      ChurnObservationMetadata_FirstActiveDuringCohort_FIRST_ACTIVE_IN_MONTHLY_COHORT);
  EXPECT_EQ(
      metadata_1->first_active_during_cohort(),
      ChurnObservationMetadata_FirstActiveDuringCohort_EXISTED_OR_NOT_ACTIVE_YET);
  EXPECT_EQ(
      metadata_2->first_active_during_cohort(),
      ChurnObservationMetadata_FirstActiveDuringCohort_EXISTED_OR_NOT_ACTIVE_YET);
}

TEST_F(
    ActiveStatusTest,
    CalculateObservationMetadataReturnsNulloptMisalignedCohortAndObservation) {
  // Setup initial value to be last active in Dec-2022.
  // Value represents device was active each of the 18 months prior.
  // Represents binary: 0100010011 000000000000000001
  int cur_value = 72089601;
  GetActiveStatus()->SetValue(cur_value);

  // Simulate unexpected scenario of observation being 1 month ahead of cohort.
  base::Time next_month_ts = GetFakeTimeNow();

  // Calculate the observation metadata for 3 periods.
  std::optional<ChurnObservationMetadata> metadata_0 =
      GetActiveStatus()->CalculateObservationMetadata(next_month_ts, 0);
  std::optional<ChurnObservationMetadata> metadata_1 =
      GetActiveStatus()->CalculateObservationMetadata(next_month_ts, 1);
  std::optional<ChurnObservationMetadata> metadata_2 =
      GetActiveStatus()->CalculateObservationMetadata(next_month_ts, 2);

  // Ensure that the metadata is calculated correctly.
  EXPECT_FALSE(metadata_0.has_value());
  EXPECT_FALSE(metadata_1.has_value());
  EXPECT_FALSE(metadata_2.has_value());
}

TEST_F(ActiveStatusTest, GetFirstActiveWeekActivateDateNotSetReturnsNullopt) {
  // Get the first active week.
  std::optional<base::Time> first_active_week = GetFirstActiveWeekForTest();

  // Ensure that nullopt is returned when the activate date is not set.
  EXPECT_FALSE(first_active_week.has_value());
}

TEST_F(ActiveStatusTest, GetFirstActiveWeekActivateDateSetReturnsExpectedWeek) {
  // Setup the activate date (ISO8601) and expected first active week for test.
  std::string activate_date = "2021-23";
  base::Time expected_activate_date_ts;
  ASSERT_TRUE(base::Time::FromUTCString("2021-06-07 00:00:00 GMT",
                                        &expected_activate_date_ts));

  SetActivateDate(activate_date);
  std::optional<base::Time> first_active_week = GetFirstActiveWeekForTest();

  // Ensure that the returned week matches the expected value.
  EXPECT_TRUE(first_active_week.has_value());
  EXPECT_EQ(first_active_week.value(), expected_activate_date_ts);

  // Setup the activate date (ISO8601) and expected first active week for test.
  activate_date = "2023-52";
  ASSERT_TRUE(base::Time::FromUTCString("2023-12-25 00:00:00 GMT",
                                        &expected_activate_date_ts));

  SetActivateDate(activate_date);
  first_active_week = GetFirstActiveWeekForTest();

  // Ensure that the returned week matches the expected value.
  EXPECT_TRUE(first_active_week.has_value());
  EXPECT_EQ(first_active_week.value(), expected_activate_date_ts);
}

TEST_F(ActiveStatusTest, GetFirstActiveWeekInvalidActivateDateSet) {
  // Setup the activate date (ISO8601) and expected first active week for test.
  std::string activate_date = "2021-xx";
  SetActivateDate(activate_date);
  std::optional<base::Time> first_active_week = GetFirstActiveWeekForTest();
  EXPECT_FALSE(first_active_week.has_value());

  // Setup the activate date (ISO8601) and expected first active week for test.
  activate_date = "2023-99";
  SetActivateDate(activate_date);
  first_active_week = GetFirstActiveWeekForTest();
  EXPECT_FALSE(first_active_week.has_value());

  // Setup the activate date (ISO8601) and expected first active week for test.
  activate_date = "-------";
  SetActivateDate(activate_date);
  first_active_week = GetFirstActiveWeekForTest();
  EXPECT_FALSE(first_active_week.has_value());
}

}  // namespace ash::report::device_metrics
