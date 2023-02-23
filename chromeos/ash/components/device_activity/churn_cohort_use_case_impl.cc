// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/device_activity/churn_cohort_use_case_impl.h"

#include "ash/constants/ash_features.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "chromeos/ash/components/device_activity/fresnel_pref_names.h"
#include "chromeos/ash/components/device_activity/fresnel_service.pb.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/channel.h"
#include "third_party/private_membership/src/private_membership_rlwe_client.h"

namespace ash::device_activity {

namespace psm_rlwe = private_membership::rlwe;

namespace {

bool IsFirstActiveInCohort(base::Time first_active_week,
                           base::Time cohort_active_ts) {
  base::Time::Exploded exploded;
  first_active_week.UTCExplode(&exploded);
  int first_active_year = exploded.year;
  int first_active_month = exploded.month;

  cohort_active_ts.UTCExplode(&exploded);
  int cohort_year = exploded.year;
  int cohort_month = exploded.month;

  return first_active_year == cohort_year && first_active_month == cohort_month;
}
}  // namespace

ChurnCohortUseCaseImpl::ChurnCohortUseCaseImpl(
    ChurnActiveStatus* churn_active_status_ptr,
    const std::string& psm_device_active_secret,
    const ChromeDeviceMetadataParameters& chrome_passed_device_params,
    PrefService* local_state,
    std::unique_ptr<PsmDelegateInterface> psm_delegate)
    : DeviceActiveUseCase(
          psm_device_active_secret,
          chrome_passed_device_params,
          prefs::kDeviceActiveChurnCohortMonthlyPingTimestamp,
          psm_rlwe::RlweUseCase::CROS_FRESNEL_CHURN_MONTHLY_COHORT,
          local_state,
          std::move(psm_delegate)),
      churn_active_status_ptr_(churn_active_status_ptr) {
  DCHECK(churn_active_status_ptr_);
}

ChurnCohortUseCaseImpl::~ChurnCohortUseCaseImpl() = default;

// The Churn Cohort window identifier is the year-month when the device
// report its cohort active request to Fresnel.
//
// For example, if the device has reported its active on `20221202`,
// then the Churn Cohort window identifier is `202212`
std::string ChurnCohortUseCaseImpl::GenerateWindowIdentifier(
    base::Time ts) const {
  base::Time::Exploded exploded;
  ts.UTCExplode(&exploded);
  return base::StringPrintf("%04d%02d", exploded.year, exploded.month);
}

absl::optional<FresnelImportDataRequest>
ChurnCohortUseCaseImpl::GenerateImportRequestBody() {
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

  std::string psm_id_str = GetPsmIdentifier().value().sensitive_id();
  std::string window_id_str = GetWindowIdentifier().value();

  FresnelImportData* import_data = import_request.add_import_data();
  import_data->set_plaintext_id(psm_id_str);
  import_data->set_window_identifier(window_id_str);
  import_data->set_is_pt_window_identifier(true);

  ChurnCohortMetadata* cohort_metadata =
      import_data->mutable_churn_cohort_metadata();
  cohort_metadata->set_active_status_value(
      churn_active_status_ptr_->GetValueAsInt());
  base::Time first_active_week = churn_active_status_ptr_->GetFirstActiveWeek();
  // Only when we can get the ActivateDate from VPD then set whether the
  // device is first active during the churn cohort period. If we cannot
  // get value from VPD, then we don't set value for this field.
  if (first_active_week != base::Time()) {
    cohort_metadata->set_is_first_active_in_cohort(IsFirstActiveInCohort(
        churn_active_status_ptr_->GetFirstActiveWeek(), GetActiveTs()));
  }

  return import_request;
}

bool ChurnCohortUseCaseImpl::IsEnabledCheckIn() {
  return base::FeatureList::IsEnabled(
      features::kDeviceActiveClientChurnCohortCheckIn);
}

bool ChurnCohortUseCaseImpl::IsEnabledCheckMembership() {
  return base::FeatureList::IsEnabled(
      features::kDeviceActiveClientChurnCohortCheckMembership);
}

private_computing::ActiveStatus ChurnCohortUseCaseImpl::GenerateActiveStatus() {
  private_computing::ActiveStatus status;

  status.set_use_case(private_computing::PrivateComputingUseCase::
                          CROS_FRESNEL_CHURN_MONTHLY_COHORT);

  // TODO(qianwan) Make sure the date in preserved file is PST.
  std::string last_ping_pt_date =
      FormatPTDateString(GetLastKnownPingTimestamp());
  status.set_last_ping_date(last_ping_pt_date);
  status.set_churn_active_status(churn_active_status_ptr_->GetValueAsInt());

  return status;
}
}  // namespace ash::device_activity
