// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DEVICE_ACTIVITY_CHURN_COHORT_USE_CASE_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_DEVICE_ACTIVITY_CHURN_COHORT_USE_CASE_IMPL_H_

#include "base/component_export.h"
#include "base/time/time.h"
#include "chromeos/ash/components/device_activity/device_active_use_case.h"

class PrefService;

namespace version_info {
enum class Channel;
}  // namespace version_info

namespace ash::device_activity {

// Forward declaration from fresnel_service.proto.
class FresnelImportDataRequest;

// Contains the methods required to report the churn cohort device active.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_DEVICE_ACTIVITY)
    ChurnCohortUseCaseImpl : public DeviceActiveUseCase {
 public:
  ChurnCohortUseCaseImpl(
      const std::string& psm_device_active_secret,
      const ChromeDeviceMetadataParameters& chrome_passed_device_params,
      PrefService* local_state,
      std::unique_ptr<PsmDelegateInterface> psm_delegate);
  ChurnCohortUseCaseImpl(const ChurnCohortUseCaseImpl&) = delete;
  ChurnCohortUseCaseImpl& operator=(const ChurnCohortUseCaseImpl&) = delete;
  ~ChurnCohortUseCaseImpl() override;

  // The Churn Cohort window identifier is the year-month when the device
  // report its cohort active request to Fresnel.
  //
  // For example, if the device has reported its active on `20221202`,
  // then the Churn Cohort window identifier is `202212`
  std::string GenerateWindowIdentifier(base::Time ts) const override;

  absl::optional<FresnelImportDataRequest> GenerateImportRequestBody() override;

  // Whether current device active use case check-in is enabled or not.
  bool IsEnabledCheckIn() override;

  // Whether current device active use case check membership is enabled or not.
  bool IsEnabledCheckMembership() override;

  private_computing::ActiveStatus GenerateActiveStatus() override;
};

}  // namespace ash::device_activity

#endif  // CHROMEOS_ASH_COMPONENTS_DEVICE_ACTIVITY_CHURN_COHORT_USE_CASE_IMPL_H_
