// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/report/utils/pref_utils.h"

#include "chromeos/ash/components/dbus/private_computing/private_computing_service.pb.h"
#include "chromeos/ash/components/report/prefs/fresnel_pref_names.h"
#include "chromeos/ash/components/report/report_controller.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::report::utils {

namespace {

using private_computing::ActiveStatus;
using private_computing::ChurnObservationStatus;
using private_computing::GetStatusResponse;
using private_computing::PrivateComputingUseCase;
using private_computing::SaveStatusRequest;

}  // namespace

class PrefUtilsTest : public testing::Test {
 public:
  void SetUp() override {
    // Register all related local state prefs.
    ReportController::RegisterPrefs(local_state_.registry());
  }

 protected:
  PrefService* GetLocalState() { return &local_state_; }

 private:
  TestingPrefServiceSimple local_state_;
};

TEST_F(PrefUtilsTest, RestoreLocalStateWithPreservedFile) {
  // Example test case for restoring local state with preserved file.
  // Prepare a sample response with active statuses.
  GetStatusResponse response;
  ActiveStatus active_status1;
  active_status1.set_use_case(PrivateComputingUseCase::CROS_FRESNEL_DAILY);
  active_status1.set_last_ping_date("2023-05-01");
  response.add_active_status()->CopyFrom(active_status1);

  ActiveStatus active_status2;
  active_status2.set_use_case(
      PrivateComputingUseCase::CROS_FRESNEL_28DAY_ACTIVE);
  active_status2.set_last_ping_date("2023-04-01");
  response.add_active_status()->CopyFrom(active_status2);

  // Restore the local state with the preserved file contents.
  PrefService* local_state = GetLocalState();
  RestoreLocalStateWithPreservedFile(local_state, response);

  // Verify that the local state is updated correctly.
  base::Time ts;
  EXPECT_TRUE(base::Time::FromUTCString("2023-05-01", &ts));
  EXPECT_EQ(ts, local_state->GetTime(
                    prefs::kDeviceActiveLastKnown1DayActivePingTimestamp));

  EXPECT_TRUE(base::Time::FromUTCString("2023-04-01", &ts));
  EXPECT_EQ(ts, local_state->GetTime(
                    prefs::kDeviceActiveLastKnown28DayActivePingTimestamp));
}

TEST_F(PrefUtilsTest, RestoreLocalStateWithInvalidPreservedFile) {
  // Example test case for restoring local state with preserved file.
  // Prepare a sample response with active statuses.
  GetStatusResponse response;
  ActiveStatus active_status1;
  active_status1.set_use_case(PrivateComputingUseCase::CROS_FRESNEL_DAILY);
  active_status1.set_last_ping_date("1970-01-01");
  response.add_active_status()->CopyFrom(active_status1);

  ActiveStatus active_status2;
  active_status2.set_use_case(
      PrivateComputingUseCase::CROS_FRESNEL_28DAY_ACTIVE);
  active_status2.set_last_ping_date("1970-01-01");
  response.add_active_status()->CopyFrom(active_status2);

  ActiveStatus active_status3;
  active_status3.set_use_case(
      PrivateComputingUseCase::CROS_FRESNEL_CHURN_MONTHLY_COHORT);
  active_status3.set_last_ping_date("1970-01-01");
  active_status3.set_churn_active_status(0);
  response.add_active_status()->CopyFrom(active_status3);

  PrefService* local_state = GetLocalState();

  // Restore the local state with the preserved file contents.
  RestoreLocalStateWithPreservedFile(local_state, response);

  // The local state will have it's default registered values since the
  // preserved file contains invalid values.
  base::Time ts = base::Time::UnixEpoch();
  EXPECT_EQ(local_state->GetTime(
                prefs::kDeviceActiveLastKnown1DayActivePingTimestamp),
            ts);
  EXPECT_EQ(local_state->GetTime(
                prefs::kDeviceActiveLastKnown28DayActivePingTimestamp),
            ts);
  EXPECT_EQ(
      local_state->GetTime(prefs::kDeviceActiveChurnCohortMonthlyPingTimestamp),
      ts);
  EXPECT_EQ(
      local_state->GetValue(prefs::kDeviceActiveLastKnownChurnActiveStatus), 0);
}

TEST_F(PrefUtilsTest, RestoreLocalStateWithInvalidLastPingDateInPreservedFile) {
  // Example test case for restoring local state with preserved file.
  // Prepare a sample response with active statuses.
  GetStatusResponse response;
  ActiveStatus active_status1;
  active_status1.set_use_case(PrivateComputingUseCase::CROS_FRESNEL_DAILY);
  active_status1.set_last_ping_date("INVALID_TS_STRING");
  response.add_active_status()->CopyFrom(active_status1);

  PrefService* local_state = GetLocalState();

  // Restore the local state with the preserved file contents.
  RestoreLocalStateWithPreservedFile(local_state, response);

  // The local state will have it's default registered values since the
  // preserved file contains invalid values.
  base::Time ts = base::Time::UnixEpoch();
  EXPECT_EQ(local_state->GetTime(
                prefs::kDeviceActiveLastKnown1DayActivePingTimestamp),
            ts);
}

TEST_F(PrefUtilsTest, CreatePreservedFileContents) {
  PrefService* local_state = GetLocalState();

  // Prepare a sample PrefService instance with stored values.
  base::Time ts_1da;
  base::Time ts_28da;
  base::Time ts_churn_cohort;
  base::Time ts_churn_observation;
  EXPECT_TRUE(base::Time::FromUTCString("2023-05-01", &ts_1da));
  EXPECT_TRUE(base::Time::FromUTCString("2023-04-01", &ts_28da));

  // Normally churn and cohort should have the same last ping dates.
  // Below we will test this method in the odd case it stores different dates.
  EXPECT_TRUE(base::Time::FromUTCString("2023-03-01", &ts_churn_cohort));
  EXPECT_TRUE(base::Time::FromUTCString("2023-03-01", &ts_churn_observation));

  local_state->SetTime(prefs::kDeviceActiveLastKnown1DayActivePingTimestamp,
                       ts_1da);
  local_state->SetTime(prefs::kDeviceActiveLastKnown28DayActivePingTimestamp,
                       ts_28da);
  local_state->SetTime(prefs::kDeviceActiveChurnCohortMonthlyPingTimestamp,
                       ts_churn_cohort);
  local_state->SetTime(prefs::kDeviceActiveChurnObservationMonthlyPingTimestamp,
                       ts_churn_observation);

  local_state->SetInteger(prefs::kDeviceActiveLastKnownChurnActiveStatus, 1);
  local_state->SetBoolean(
      prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus0, true);
  local_state->SetBoolean(
      prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus1, true);
  local_state->SetBoolean(
      prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus2, true);

  // Create the preserved file contents.
  SaveStatusRequest save_request = CreatePreservedFileContents(local_state);

  // Verify that the preserved file contents are created correctly.
  // Active status stored for: 1DA, 28DA, Churn Cohort, and Churn Observation.
  EXPECT_EQ(4, save_request.active_status_size());

  EXPECT_EQ(PrivateComputingUseCase::CROS_FRESNEL_DAILY,
            save_request.active_status(0).use_case());
  EXPECT_EQ("2023-05-01 00:00:00.000 GMT",
            save_request.active_status(0).last_ping_date());

  EXPECT_EQ(PrivateComputingUseCase::CROS_FRESNEL_28DAY_ACTIVE,
            save_request.active_status(1).use_case());
  EXPECT_EQ("2023-04-01 00:00:00.000 GMT",
            save_request.active_status(1).last_ping_date());

  EXPECT_EQ(PrivateComputingUseCase::CROS_FRESNEL_CHURN_MONTHLY_COHORT,
            save_request.active_status(2).use_case());
  EXPECT_EQ("2023-03-01 00:00:00.000 GMT",
            save_request.active_status(2).last_ping_date());
  EXPECT_EQ(1, save_request.active_status(2).churn_active_status());

  EXPECT_EQ(PrivateComputingUseCase::CROS_FRESNEL_CHURN_MONTHLY_OBSERVATION,
            save_request.active_status(3).use_case());
  EXPECT_TRUE(save_request.active_status(3)
                  .period_status()
                  .is_active_current_period_minus_0());
  EXPECT_TRUE(save_request.active_status(3)
                  .period_status()
                  .is_active_current_period_minus_1());
  EXPECT_TRUE(save_request.active_status(3)
                  .period_status()
                  .is_active_current_period_minus_2());
}

TEST_F(PrefUtilsTest,
       CreatePreservedFileContentsForUnsynedCohortAndObservation) {
  PrefService* local_state = GetLocalState();

  // Prepare a sample PrefService instance with stored values.
  base::Time ts_churn_cohort;
  base::Time ts_churn_observation;

  // Test odd case cohort and observation store different last ping dates.
  EXPECT_TRUE(base::Time::FromUTCString("2023-03-01", &ts_churn_cohort));
  EXPECT_TRUE(base::Time::FromUTCString("2023-02-01", &ts_churn_observation));

  local_state->SetTime(prefs::kDeviceActiveChurnCohortMonthlyPingTimestamp,
                       ts_churn_cohort);
  local_state->SetTime(prefs::kDeviceActiveChurnObservationMonthlyPingTimestamp,
                       ts_churn_observation);

  local_state->SetInteger(prefs::kDeviceActiveLastKnownChurnActiveStatus, 1);
  local_state->SetBoolean(
      prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus0, true);
  local_state->SetBoolean(
      prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus1, true);
  local_state->SetBoolean(
      prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus2, true);

  // Create the preserved file contents.
  SaveStatusRequest save_request = CreatePreservedFileContents(local_state);

  // Active status stored for: Churn Cohort.
  // No preserved file data stored for Churn Observation because it's
  // out of sync with the cohort last ping.
  EXPECT_EQ(1, save_request.active_status_size());
  EXPECT_EQ(PrivateComputingUseCase::CROS_FRESNEL_CHURN_MONTHLY_COHORT,
            save_request.active_status(0).use_case());
  EXPECT_EQ("2023-03-01 00:00:00.000 GMT",
            save_request.active_status(0).last_ping_date());
  EXPECT_EQ(1, save_request.active_status(0).churn_active_status());
}

}  // namespace ash::report::utils
