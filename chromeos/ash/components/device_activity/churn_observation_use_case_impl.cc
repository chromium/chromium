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

// There are 18 bits (indexed 17 to 0, left to right) representing the past 18
// months of the device actives. The right-most bit will always represent the
// device was active for the current month.
//
// The cohort use case will ping for the current month before
// the observation use case reads the active status bits.
// In other words, the current month is represented at index 0 (right-most bit).
//
// Index (1-3) in the active status bits represents the monthly active churn
// status for the 3 different observation windows.
constexpr int kMonthlyChurnActiveStatusOffsetIndex = 1;

// Index (13-15) in the active status bits represents the
// yearly active churn status for the 3 different observation windows.
constexpr int kYearlyChurnActiveStatusOffsetIndex = 13;

// Number of months in a given year.
constexpr int kMonthsInYear = 12;

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

ObservationWindow::ObservationWindow(int period,
                                     const std::string& observation_period) {
  SetPeriod(period);
  SetObservationPeriod(observation_period);
}

bool ObservationWindow::IsObservationWindowSet() const {
  return (period_ != -1) && (!observation_period_.empty());
}

int ObservationWindow::GetPeriod() const {
  if (!IsObservationWindowSet()) {
    LOG(ERROR) << "Error - period or observation_period is unset.";
  }
  return period_;
}

const std::string& ObservationWindow::GetObservationPeriod() const {
  if (!IsObservationWindowSet()) {
    LOG(ERROR) << "Error - period or observation_period is unset.";
  }
  return observation_period_;
}

bool ObservationWindow::SetPeriod(int period) {
  if (period < 0 || period > 2) {
    LOG(ERROR) << "Error - failed to set period to value that is "
               << "not within [0,2] inclusive." << std::endl
               << "Attempted to set period as = " << period;
    return false;
  }

  period_ = period;
  return true;
}

bool ObservationWindow::SetObservationPeriod(
    const std::string& observation_period) {
  // Perform a simple check to verify observation period is of length
  // YYYYMM-YYYYMM.
  //
  // std::size method includes null terminated character, so we can subtract 1.
  if (observation_period.length() !=
      (std::size(kObservationPeriodFormat) - 1)) {
    LOG(ERROR) << "Error - observation period is not set correctly."
               << std::endl
               << "Attempted to set observation period as = "
               << observation_period;
    return false;
  }

  observation_period_ = observation_period;
  return true;
}

void ObservationWindow::Reset() {
  period_ = -1;
  observation_period_ = std::string();
}

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
  if (!CohortCheckInSuccessfullyUpdatedActiveStatus()) {
    LOG(ERROR)
        << "Churn observation use case should only generate import request "
        << "if the cohort use case successfully reported and updated the "
           "active_status object.";
    LOG(ERROR) << "Active status object currently has value = "
               << churn_active_status_ptr_->GetValueAsInt();
    return absl::nullopt;
  }

  // Initializes the 3 observation period window identifiers based on the
  // current active ts month.
  SetObservationPeriodWindows(GetActiveTs());

  // Verify observation periods were set as expected.
  if (!observation_window_0_.IsObservationWindowSet() &&
      !observation_window_1_.IsObservationWindowSet() &&
      !observation_window_2_.IsObservationWindowSet()) {
    LOG(ERROR) << "All observation periods are currently unset. "
               << "Returning empty FresnelImportDataRequest.";
    return absl::nullopt;
  }

  // Generate Fresnel PSM import request body.
  FresnelImportDataRequest import_request;

  // Create fresh |DeviceMetadata| object.
  // Note every dimension added to this proto must be approved by privacy.
  DeviceMetadata* device_metadata = import_request.mutable_device_metadata();
  device_metadata->set_chromeos_version(GetChromeOSVersion());
  device_metadata->set_chromeos_channel(GetChromeOSChannel());
  device_metadata->set_market_segment(GetMarketSegment());
  device_metadata->set_hardware_id(GetFullHardwareClass());

  import_request.set_use_case(GetPsmUseCase());

  if (observation_window_0_.IsObservationWindowSet()) {
    *import_request.add_import_data() =
        GenerateObservationFresnelImportData(observation_window_0_);
  }

  if (observation_window_1_.IsObservationWindowSet()) {
    *import_request.add_import_data() =
        GenerateObservationFresnelImportData(observation_window_1_);
  }

  if (observation_window_2_.IsObservationWindowSet()) {
    *import_request.add_import_data() =
        GenerateObservationFresnelImportData(observation_window_2_);
  }

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

// This method is called after all use cases have completed check in and the
// latest values are updated to the local state.
private_computing::ActiveStatus
ChurnObservationUseCaseImpl::GenerateActiveStatus() {
  private_computing::ActiveStatus status;

  status.set_use_case(private_computing::PrivateComputingUseCase::
                          CROS_FRESNEL_CHURN_MONTHLY_OBSERVATION);

  private_computing::ChurnObservationStatus* period_status =
      status.mutable_period_status();

  // The local state should contain the latest active period booleans.
  period_status->set_is_active_current_period_minus_0(
      GetLocalState()->GetBoolean(
          prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus0));
  period_status->set_is_active_current_period_minus_1(
      GetLocalState()->GetBoolean(
          prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus1));
  period_status->set_is_active_current_period_minus_2(
      GetLocalState()->GetBoolean(
          prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus2));

  return status;
}

FresnelImportData
ChurnObservationUseCaseImpl::GenerateObservationFresnelImportData(
    const ObservationWindow& observation_window) const {
  DCHECK(observation_window.IsObservationWindowSet());

  std::string observation_window_id = observation_window.GetObservationPeriod();
  absl::optional<psm_rlwe::RlwePlaintextId> psm_id =
      GeneratePsmIdentifier(observation_window_id);
  std::string psm_id_str = psm_id.value().sensitive_id();

  FresnelImportData import_data;
  import_data.set_plaintext_id(psm_id_str);
  import_data.set_window_identifier(observation_window_id);
  import_data.set_is_pt_window_identifier(true);

  // Set the observation metadata used in churn computation.
  ChurnObservationMetadata* observation_metadata =
      import_data.mutable_churn_observation_metadata();
  observation_metadata->set_monthly_active_status(
      IsPreviousMonthlyActive(observation_window));
  observation_metadata->set_yearly_active_status(
      IsPreviousYearlyActive(observation_window));

  absl::optional<ChurnObservationMetadata::FirstActiveDuringCohort>
      first_active_during_cohort =
          GetFirstActiveDuringCohort(observation_window);

  // Only set the proto observation metadata if we were able to calculate the
  // first active during cohort enum successfully.
  if (first_active_during_cohort.has_value()) {
    observation_metadata->set_first_active_during_cohort(
        first_active_during_cohort.value());
  }

  return import_data;
}

bool ChurnObservationUseCaseImpl::IsPreviousMonthlyActive(
    const ObservationWindow& observation_window) const {
  DCHECK(churn_active_status_ptr_);
  DCHECK(observation_window.IsObservationWindowSet());

  int active_month_val = churn_active_status_ptr_->GetActiveMonthBits();

  std::bitset<ChurnActiveStatus::kActiveMonthsBitSize> active_month_bits(
      active_month_val);

  // Calculate the monthly churn rate by determining whether device was active
  // in the previous month of the observation window.
  // For example, for observation window "202303-202305", we would check for
  // whether "202302" month was active.
  return active_month_bits.test(kMonthlyChurnActiveStatusOffsetIndex +
                                observation_window.GetPeriod());
}

bool ChurnObservationUseCaseImpl::IsPreviousYearlyActive(
    const ObservationWindow& observation_window) const {
  DCHECK(churn_active_status_ptr_);
  DCHECK(observation_window.IsObservationWindowSet());

  int active_month_val = churn_active_status_ptr_->GetActiveMonthBits();

  std::bitset<ChurnActiveStatus::kActiveMonthsBitSize> active_month_bits(
      active_month_val);

  // Calculate the yearly churn rate by determining whether device was active
  // 12 months before the previous month of the observation window.
  // For example, for observation window "202303-202305", we would check for
  // whether "202202" month was active.
  return active_month_bits.test(kYearlyChurnActiveStatusOffsetIndex +
                                observation_window.GetPeriod());
}

absl::optional<ChurnObservationMetadata::FirstActiveDuringCohort>
ChurnObservationUseCaseImpl::GetFirstActiveDuringCohort(
    const ObservationWindow& observation_window) const {
  DCHECK(churn_active_status_ptr_);
  DCHECK(observation_window.IsObservationWindowSet());

  // TODO(hirthanan): Add UMA histogram to measure start of ActivateDate Period
  base::Time first_active_week = churn_active_status_ptr_->GetFirstActiveWeek();

  if (first_active_week == base::Time()) {
    LOG(ERROR)
        << "Reached an invalid state where the first active week is unset.";
    return absl::nullopt;
  }

  // This case should never happen since the device reports the churn cohort use
  // case before this, churn observation use case. The churn cohort use case
  // updates the active month bits for the current month, meaning this value
  // should never be 0.
  if (churn_active_status_ptr_->GetActiveMonthBits() == 0) {
    LOG(ERROR) << "Reached an invalid state where the Active Month Bits is 0.";
    return absl::nullopt;
  }

  // Determine whether the device was first active in the month previous to the
  // observation window.
  // 1. Get new timestamp for months since inception.
  // 2. IsPreviousMonthlyActive(observation_window) will tell us whether the
  //    device was active in the month before the observation window.
  // 3. It is labelled first active if
  //    first_active_week == (observation_window_month-1) && bool in step 2 is
  //    true.
  base::Time current_active_month =
      churn_active_status_ptr_->GetCurrentActiveMonth();
  base::Time prev_active_month = current_active_month;

  // Get the month before the start of the observation period, [0,2].
  // This depends on the observation window period to know how
  // many months back to go from the current active month.
  // 1. Period 0 will be 1 month before the current active month.
  // 2. Period 1 will be 2 months before the current active month.
  // 3. Period 2 will be 3 months before the current active month.
  int observation_window_period = observation_window.GetPeriod();
  for (int i = 0; i <= observation_window_period; i++) {
    prev_active_month = GetPreviousMonth(prev_active_month);
  }

  base::Time::Exploded first_active_week_exploded;
  base::Time::Exploded prev_active_month_exploded;

  first_active_week.UTCExplode(&first_active_week_exploded);
  prev_active_month.UTCExplode(&prev_active_month_exploded);

  if ((first_active_week_exploded.month == prev_active_month_exploded.month) &&
      (first_active_week_exploded.year == prev_active_month_exploded.year) &&
      IsPreviousMonthlyActive(observation_window)) {
    return ChurnObservationMetadata_FirstActiveDuringCohort_FIRST_ACTIVE_IN_MONTHLY_COHORT;
  }

  // Determine whether the device was first active in previous cohort year.
  // The previous cohort year is 12 months behind the previous cohort month.
  base::Time prev_active_year = prev_active_month;
  for (int i = 0; i < kMonthsInYear; i++) {
    prev_active_year = GetPreviousMonth(prev_active_year);
  }

  base::Time::Exploded prev_active_year_exploded;
  prev_active_year.UTCExplode(&prev_active_year_exploded);

  if ((first_active_week_exploded.month == prev_active_year_exploded.month) &&
      (first_active_week_exploded.year == prev_active_year_exploded.year) &&
      IsPreviousYearlyActive(observation_window)) {
    return ChurnObservationMetadata_FirstActiveDuringCohort_FIRST_ACTIVE_IN_YEARLY_COHORT;
  }

  // Since the device was not first active in the previous cohort month or
  // previous cohort year, return EXISTED_OR_NOT_ACTIVE_YET.
  return ChurnObservationMetadata_FirstActiveDuringCohort_EXISTED_OR_NOT_ACTIVE_YET;
}

std::string ChurnObservationUseCaseImpl::GetObservationPeriod(int period) {
  if (period == 0) {
    return observation_window_0_.GetObservationPeriod();
  }
  if (period == 1) {
    return observation_window_1_.GetObservationPeriod();
  }
  if (period == 2) {
    return observation_window_2_.GetObservationPeriod();
  }
  LOG(ERROR) << "Invalid period passed to method. "
             << "There is only 3 observation periods.";
  return std::string();
}

bool ChurnObservationUseCaseImpl::CohortCheckInSuccessfullyUpdatedActiveStatus()
    const {
  // Verify the active status object was updated from the inception date.
  if (churn_active_status_ptr_->GetActiveMonthBits() == 0) {
    LOG(ERROR) << "Active status has no active bits set. "
               << "Active status value = "
               << churn_active_status_ptr_->GetValueAsInt();
    return false;
  }

  base::Time active_status_ts =
      churn_active_status_ptr_->GetCurrentActiveMonth();
  base::Time cur_ping_ts = GetActiveTs();

  // The active_status_ts and cur_ping_ts should already be initialized.
  if (active_status_ts == base::Time() || cur_ping_ts == base::Time()) {
    LOG(ERROR) << "active status or cur ping ts is not initialized. "
               << std::endl
               << "Active status ts = " << active_status_ts << std::endl
               << "Current ping ts = " << cur_ping_ts;
    return false;
  }

  // Check that the active status object was updated at some point in this
  // month.
  base::Time::Exploded active_status_exploded;
  base::Time::Exploded cur_ts_exploded;

  active_status_ts.UTCExplode(&active_status_exploded);
  cur_ping_ts.UTCExplode(&cur_ts_exploded);

  if ((active_status_exploded.month == cur_ts_exploded.month) &&
      (active_status_exploded.year == cur_ts_exploded.year)) {
    return true;
  }

  return false;
}

void ChurnObservationUseCaseImpl::SetObservationPeriodWindows(base::Time ts) {
  if (ts == base::Time()) {
    LOG(ERROR) << "Timestamp ts is not initialized. "
               << "Observation periods are left unset.";
    return;
  }

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

  // Generate the observation period windows.
  std::string observation_period_0 =
      cur_month_window_id + "-" + GenerateWindowIdentifier(cur_month_plus_2);
  std::string observation_period_1 =
      GenerateWindowIdentifier(cur_month_minus_1) + "-" +
      GenerateWindowIdentifier(cur_month_plus_1);
  std::string observation_period_2 =
      GenerateWindowIdentifier(cur_month_minus_2) + "-" + cur_month_window_id;

  // Update the observation windows that need to be sent to Fresnel only
  // if the last known active period is false.
  if (!GetLocalState()->GetBoolean(
          prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus0)) {
    observation_window_0_ = ObservationWindow(0, observation_period_0);
  } else {
    observation_window_0_.Reset();
  }

  if (!GetLocalState()->GetBoolean(
          prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus1)) {
    observation_window_1_ = ObservationWindow(1, observation_period_1);
  } else {
    observation_window_1_.Reset();
  }

  if (!GetLocalState()->GetBoolean(
          prefs::kDeviceActiveLastKnownIsActiveCurrentPeriodMinus2)) {
    observation_window_2_ = ObservationWindow(2, observation_period_2);
  } else {
    observation_window_2_.Reset();
  }
}

}  // namespace ash::device_activity
