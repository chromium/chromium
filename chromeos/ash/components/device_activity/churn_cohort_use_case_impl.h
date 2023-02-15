// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DEVICE_ACTIVITY_CHURN_COHORT_USE_CASE_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_DEVICE_ACTIVITY_CHURN_COHORT_USE_CASE_IMPL_H_

#include "base/component_export.h"
#include "base/time/time.h"
#include "chromeos/ash/components/device_activity/churn_active_status.h"
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
      ChurnActiveStatus* churn_active_status_ptr,
      const std::string& psm_device_active_secret,
      const ChromeDeviceMetadataParameters& chrome_passed_device_params,
      PrefService* local_state,
      std::unique_ptr<PsmDelegateInterface> psm_delegate);
  ChurnCohortUseCaseImpl(const ChurnCohortUseCaseImpl&) = delete;
  ChurnCohortUseCaseImpl& operator=(const ChurnCohortUseCaseImpl&) = delete;
  ~ChurnCohortUseCaseImpl() override;

  // DeviceActiveUseCase:
  std::string GenerateWindowIdentifier(base::Time ts) const override;
  absl::optional<FresnelImportDataRequest> GenerateImportRequestBody() override;
  bool IsEnabledCheckIn() override;
  bool IsEnabledCheckMembership() override;
  private_computing::ActiveStatus GenerateActiveStatus() override;

 private:
  ChurnActiveStatus* const churn_active_status_ptr_;
};

}  // namespace ash::device_activity

#endif  // CHROMEOS_ASH_COMPONENTS_DEVICE_ACTIVITY_CHURN_COHORT_USE_CASE_IMPL_H_
