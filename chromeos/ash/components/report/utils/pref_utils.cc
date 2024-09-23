// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/report/utils/pref_utils.h"

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "chromeos/ash/components/dbus/private_computing/private_computing_service.pb.h"
#include "chromeos/ash/components/report/prefs/fresnel_pref_names.h"
#include "chromeos/ash/components/report/utils/time_utils.h"
#include "components/prefs/pref_service.h"

namespace ash::report::utils {

namespace {

using private_computing::ActiveStatus;
using private_computing::ChurnObservationStatus;
using private_computing::GetStatusResponse;
using private_computing::PrivateComputingUseCase;
using private_computing::SaveStatusRequest;

// UMA histogram names for preserved file read records.
const char kHistogramsPreservedFileRead[] =
    "Ash.Report.PreservedFileReadAndParsed";

// |ts| must be defined and not unix epoch time.
void WriteLocalStateTimestampIfValid(PrefService* local_state,
                                     const std::string& pref_name,
                                     base::Time ts) {
  if (ts != base::Time::UnixEpoch() || ts != base::Time()) {
    local_state->SetTime(pref_name, ts);
  }
}

void WriteObservationLastPingTimestampIfValid(PrefService* local_state,
                                              const std::string& pref_name,
                                              base::Time ts) {
  if (ts == base::Time::UnixEpoch() || ts == base::Time()) {
    LOG(ERROR) << "Observation timestamp is not valid. ts = " << ts;
    return;
  }

  if (local_state->GetBoolean(
          prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus0)) {
    local_state->SetTime(pref_name, ts);
  } else if (local_state->GetBoolean(
                 prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus1)) {
    local_state->SetTime(pref_name, utils::GetPreviousMonth(ts).value());
  } else if (local_state->GetBoolean(
                 prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus2)) {
    local_state->SetTime(
        pref_name,
        utils::GetPreviousMonth(utils::GetPreviousMonth(ts).value()).value());
  }
}

}  // namespace

void RestoreLocalStateWithPreservedFile(PrefService* local_state,
                                        GetStatusResponse response) {
  bool read_success = true;
  for (ActiveStatus active_status : response.active_status()) {
    base::Time last_ping_ts;
    // Parse and validate the ping date before attempting to restore value.
    if (active_status.has_last_ping_date()) {
      bool success = base::Time::FromUTCString(
          active_status.last_ping_date().c_str(), &last_ping_ts);
      if (!success) {
        read_success = false;
        LOG(ERROR) << "Fail to convert last ping date to ts for use case = "
                   << PrivateComputingUseCase_Name(active_status.use_case());
        continue;
      }
    }

    switch (active_status.use_case()) {
      case PrivateComputingUseCase::CROS_FRESNEL_DAILY:
        WriteLocalStateTimestampIfValid(
            local_state, prefs::kDeviceActiveLastKnown1DayActivePingTimestamp,
            last_ping_ts);
        break;
      case PrivateComputingUseCase::CROS_FRESNEL_28DAY_ACTIVE:
        WriteLocalStateTimestampIfValid(
            local_state, prefs::kDeviceActiveLastKnown28DayActivePingTimestamp,
            last_ping_ts);
        break;
      case PrivateComputingUseCase::CROS_FRESNEL_CHURN_MONTHLY_COHORT:
        WriteLocalStateTimestampIfValid(
            local_state, prefs::kDeviceActiveChurnCohortMonthlyPingTimestamp,
            last_ping_ts);

        if (active_status.has_churn_active_status() &&
            active_status.churn_active_status() != 0) {
          local_state->SetInteger(
              prefs::kDeviceActiveLastKnownChurnActiveStatus,
              active_status.churn_active_status());
        }
        break;
      case PrivateComputingUseCase::CROS_FRESNEL_CHURN_MONTHLY_OBSERVATION:
        if (active_status.has_period_status()) {
          local_state->SetBoolean(
              prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus0,
              active_status.period_status().is_active_current_period_minus_0());
          local_state->SetBoolean(
              prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus1,
              active_status.period_status().is_active_current_period_minus_1());
          local_state->SetBoolean(
              prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus2,
              active_status.period_status().is_active_current_period_minus_2());

          WriteObservationLastPingTimestampIfValid(
              local_state,
              prefs::kDeviceActiveChurnObservationMonthlyPingTimestamp,
              local_state->GetTime(
                  prefs::kDeviceActiveChurnCohortMonthlyPingTimestamp));
        }
        break;
      default:
        read_success = false;
        LOG(ERROR) << "Restore local state failed - unknown use case.";
        continue;
    }
  }

  base::UmaHistogramBoolean(kHistogramsPreservedFileRead, read_success);
}

SaveStatusRequest CreatePreservedFileContents(PrefService* local_state) {
  base::Time one_day_ts = local_state->GetTime(
      prefs::kDeviceActiveLastKnown1DayActivePingTimestamp);
  base::Time twenty_eight_day_ts = local_state->GetTime(
      prefs::kDeviceActiveLastKnown28DayActivePingTimestamp);
  base::Time cohort_ts =
      local_state->GetTime(prefs::kDeviceActiveChurnCohortMonthlyPingTimestamp);
  base::Time observation_ts = local_state->GetTime(
      prefs::kDeviceActiveChurnObservationMonthlyPingTimestamp);
  int churn_active_status =
      local_state->GetInteger(prefs::kDeviceActiveLastKnownChurnActiveStatus);
  bool period_0 = local_state->GetBoolean(
      prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus0);
  bool period_1 = local_state->GetBoolean(
      prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus1);
  bool period_2 = local_state->GetBoolean(
      prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus2);

  SaveStatusRequest save_request;

  // Store 1-day-active data.
  if (one_day_ts != base::Time() && one_day_ts != base::Time::UnixEpoch()) {
    ActiveStatus one_day_status;
    one_day_status.set_use_case(PrivateComputingUseCase::CROS_FRESNEL_DAILY);
    one_day_status.set_last_ping_date(
        utils::FormatTimestampToMidnightGMTString(one_day_ts));

    *save_request.add_active_status() = one_day_status;
  }

  // Store 28-day-active data.
  if (twenty_eight_day_ts != base::Time() &&
      twenty_eight_day_ts != base::Time::UnixEpoch()) {
    ActiveStatus twenty_eight_day_status;
    twenty_eight_day_status.set_use_case(
        PrivateComputingUseCase::CROS_FRESNEL_28DAY_ACTIVE);
    twenty_eight_day_status.set_last_ping_date(
        utils::FormatTimestampToMidnightGMTString(twenty_eight_day_ts));

    *save_request.add_active_status() = twenty_eight_day_status;
  }

  // Store Churn data.
  if (cohort_ts != base::Time() && cohort_ts != base::Time::UnixEpoch()) {
    ActiveStatus cohort_status;
    cohort_status.set_use_case(
        PrivateComputingUseCase::CROS_FRESNEL_CHURN_MONTHLY_COHORT);
    cohort_status.set_last_ping_date(
        utils::FormatTimestampToMidnightGMTString(cohort_ts));
    cohort_status.set_churn_active_status(churn_active_status);
    *save_request.add_active_status() = cohort_status;

    // Store Monthly Observation data.
    //
    // Observation active status will only be saved to preserved file,
    // if it is aligned with when Cohort use case last pinged.
    if (utils::IsSameYearAndMonth(observation_ts, cohort_ts)) {
      ActiveStatus observation_status;
      observation_status.set_use_case(
          PrivateComputingUseCase::CROS_FRESNEL_CHURN_MONTHLY_OBSERVATION);
      ChurnObservationStatus* period_status =
          observation_status.mutable_period_status();
      period_status->set_is_active_current_period_minus_0(period_0);
      period_status->set_is_active_current_period_minus_1(period_1);
      period_status->set_is_active_current_period_minus_2(period_2);
      *save_request.add_active_status() = observation_status;
    }
  }

  return save_request;
}

}  // namespace ash::report::utils
