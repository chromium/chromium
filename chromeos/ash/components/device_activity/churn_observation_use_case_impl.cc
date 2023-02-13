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

namespace ash::device_activity {

namespace psm_rlwe = private_membership::rlwe;

ChurnObservationUseCaseImpl::ChurnObservationUseCaseImpl(
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
          std::move(psm_delegate)) {}

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
  // TODO(hirthanan): In subsequent CL add logic to appropriately add the
  // 3 observation window identifiers.
  FresnelImportDataRequest import_request;
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

// TODO(hirthanan): Implement following three methods in new CL.
bool ChurnObservationUseCaseImpl::IsPreviousMonthlyActive() const {
  return true;
}

// TODO(hirthanan): Implement following three methods in new CL.
bool ChurnObservationUseCaseImpl::IsPreviousYearlyActive() const {
  return true;
}

// TODO(hirthanan): Implement following three methods in new CL.
ChurnObservationMetadata::FirstActiveDuringCohort
ChurnObservationUseCaseImpl::GetFirstActiveDuringCohort() const {
  return ChurnObservationMetadata_FirstActiveDuringCohort_EXISTED_OR_NOT_ACTIVE_YET;
}

}  // namespace ash::device_activity
