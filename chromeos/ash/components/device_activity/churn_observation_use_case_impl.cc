// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/device_activity/churn_observation_use_case_impl.h"

#include "ash/constants/ash_features.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "chromeos/ash/components/device_activity/fresnel_pref_names.h"
#include "chromeos/ash/components/device_activity/fresnel_service.pb.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/channel.h"
#include "third_party/private_membership/src/private_membership_rlwe_client.h"

namespace {

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

base::Time GetPreviousMonth(base::Time ts) {
  base::Time::Exploded exploded;
  ts.UTCExplode(&exploded);

  // Set new time to the first midnight of the next month.
  exploded.day_of_month = 1;
  exploded.month -= 1;
  exploded.hour = 0;
  exploded.minute = 0;
  exploded.second = 0;
  exploded.millisecond = 0;

  // Handle case when month is January.
  if (exploded.month < 1) {
    exploded.year -= 1;
    exploded.month = 12;
  }

  base::Time new_month_ts;
  bool success = base::Time::FromUTCExploded(exploded, &new_month_ts);

  if (!success) {
    return base::Time();
  }

  return new_month_ts;
}

}  // namespace

namespace ash::device_activity {

namespace psm_rlwe = private_membership::rlwe;

ChurnObservationUseCaseImpl::ChurnObservationUseCaseImpl(
    ChurnActiveStatus* churn_active_status_ptr,
    const std::string& psm_device_active_secret,
    const ChromeDeviceMetadataParameters& chrome_passed_device_params,
    PrefService* local_state,
    std::unique_ptr<PsmDelegateInterface> psm_delegate)
    : DeviceActiveUseCase(
          psm_device_active_secret,
          chrome_passed_device_params,
          prefs::kDeviceActiveChurnObservationMonthlyPingTimestamp,
          psm_rlwe::RlweUseCase::CROS_FRESNEL_CHURN_MONTHLY_OBSERVATION,
          local_state,
          std::move(psm_delegate)),
      churn_active_status_ptr_(churn_active_status_ptr) {}

ChurnObservationUseCaseImpl::~ChurnObservationUseCaseImpl() = default;

// The Churn observation window identifier is the year-month when the device
// report its observation active request to Fresnel.
//
// For example, if the device has reported its active on `20221202`,
// then the Churn observation window identifier is `202212`
std::string ChurnObservationUseCaseImpl::GenerateWindowIdentifier(
    base::Time ts) const {
  base::Time::Exploded exploded;
  ts.UTCExplode(&exploded);

  return base::StringPrintf("%04d%02d", exploded.year, exploded.month);
}

absl::optional<FresnelImportDataRequest>
ChurnObservationUseCaseImpl::GenerateImportRequestBody() {
  // Initializes the 3 observation period window identifiers based on the
  // current active ts month.
  SetObservationPeriodWindowIds(GetActiveTs());

  // Generate Fresnel PSM import request body.
  FresnelImportDataRequest import_request;

  // Verify observation periods were set as expected.
  if (observation_period_minus_0_id_.empty() &&
      observation_period_minus_1_id_.empty() &&
      observation_period_minus_2_id_.empty()) {
    LOG(ERROR) << "All observation periods are currently unset. "
               << "Returning empty FresnelImportDataRequest.";
    return import_request;
  }

  // Create fresh |DeviceMetadata| object.
  // Note every dimension added to this proto must be approved by privacy.
  DeviceMetadata* device_metadata = import_request.mutable_device_metadata();
  device_metadata->set_chromeos_version(GetChromeOSVersion());
  device_metadata->set_chromeos_channel(GetChromeOSChannel());
  device_metadata->set_market_segment(GetMarketSegment());
  device_metadata->set_hardware_id(GetFullHardwareClass());

  import_request.set_use_case(GetPsmUseCase());

  DCHECK(!observation_period_minus_0_id_.empty());
  DCHECK(!observation_period_minus_1_id_.empty());
  DCHECK(!observation_period_minus_2_id_.empty());

  *import_request.add_import_data() =
      GenerateObservationFresnelImportData(observation_period_minus_0_id_);
  *import_request.add_import_data() =
      GenerateObservationFresnelImportData(observation_period_minus_1_id_);
  *import_request.add_import_data() =
      GenerateObservationFresnelImportData(observation_period_minus_2_id_);

  return import_request;
}

bool ChurnObservationUseCaseImpl::IsEnabledCheckIn() {
  return base::FeatureList::IsEnabled(
      features::kDeviceActiveClientChurnObservationCheckIn);
}

bool ChurnObservationUseCaseImpl::IsEnabledCheckMembership() {
  return base::FeatureList::IsEnabled(
      features::kDeviceActiveClientChurnObservationCheckMembership);
}

private_computing::ActiveStatus
ChurnObservationUseCaseImpl::GenerateActiveStatus() {
  private_computing::ActiveStatus status;

  status.set_use_case(private_computing::PrivateComputingUseCase::
                          CROS_FRESNEL_CHURN_MONTHLY_OBSERVATION);

  // TODO(hirthanan): Add preserved file persistence of 3 observation periods.
  std::string last_ping_pt_date =
      FormatPTDateString(GetLastKnownPingTimestamp());
  status.set_last_ping_date(last_ping_pt_date);

  return status;
}

FresnelImportData
ChurnObservationUseCaseImpl::GenerateObservationFresnelImportData(
    const std::string& observation_window_id) const {
  absl::optional<psm_rlwe::RlwePlaintextId> psm_id =
      GeneratePsmIdentifier(observation_window_id);
  std::string psm_id_str = psm_id.value().sensitive_id();

  // TODO(hirthanan): Verify whether active status needs to be imported with
  // observation use case.

  FresnelImportData import_data;
  import_data.set_plaintext_id(psm_id_str);
  import_data.set_window_identifier(observation_window_id);
  import_data.set_is_pt_window_identifier(true);

  // Set the observation metadata used in churn computation.
  ChurnObservationMetadata* observation_metadata =
      import_data.mutable_churn_observation_metadata();
  observation_metadata->set_monthly_active_status(IsPreviousMonthlyActive());
  observation_metadata->set_yearly_active_status(IsPreviousYearlyActive());
  observation_metadata->set_first_active_during_cohort(
      GetFirstActiveDuringCohort());

  return import_data;
}

// TODO(hirthanan): Implement method to calculate previous monthly active.
// We will need the active status object pointer in the following three methods
// to proceed with implementation.
bool ChurnObservationUseCaseImpl::IsPreviousMonthlyActive() const {
  (void)churn_active_status_ptr_;
  return true;
}

// TODO(hirthanan): Implement method to calculate previous yearly active.
// We will need the active status object pointer in the following three methods
// to proceed with implementation.
bool ChurnObservationUseCaseImpl::IsPreviousYearlyActive() const {
  (void)churn_active_status_ptr_;
  return true;
}

// TODO(hirthanan): Implement method to calculate first active during cohort.
// We will need the active status object pointer in the following three methods
// to proceed with implementation.
ChurnObservationMetadata::FirstActiveDuringCohort
ChurnObservationUseCaseImpl::GetFirstActiveDuringCohort() const {
  (void)churn_active_status_ptr_;
  return ChurnObservationMetadata_FirstActiveDuringCohort_EXISTED_OR_NOT_ACTIVE_YET;
}

void ChurnObservationUseCaseImpl::SetObservationPeriodWindowIds(base::Time ts) {
  base::Time cur_month_minus_1 = GetPreviousMonth(ts);
  base::Time cur_month_minus_2 = GetPreviousMonth(cur_month_minus_1);

  base::Time cur_month_plus_1 = GetNextMonth(ts);
  base::Time cur_month_plus_2 = GetNextMonth(cur_month_plus_1);

  if (cur_month_minus_1 == base::Time() || cur_month_minus_2 == base::Time() ||
      cur_month_plus_1 == base::Time() || cur_month_plus_2 == base::Time()) {
    LOG(ERROR) << "Failed to get base::Time object. "
               << "Do not set observation periods";
    return;
  }

  std::string cur_month_window_id = GenerateWindowIdentifier(ts);

  observation_period_minus_0_id_ =
      cur_month_window_id + "-" + GenerateWindowIdentifier(cur_month_plus_2);
  observation_period_minus_1_id_ = GenerateWindowIdentifier(cur_month_minus_1) +
                                   "-" +
                                   GenerateWindowIdentifier(cur_month_plus_1);
  observation_period_minus_2_id_ =
      GenerateWindowIdentifier(cur_month_minus_2) + "-" + cur_month_window_id;
}

}  // namespace ash::device_activity
