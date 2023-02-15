// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DEVICE_ACTIVITY_CHURN_OBSERVATION_USE_CASE_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_DEVICE_ACTIVITY_CHURN_OBSERVATION_USE_CASE_IMPL_H_

#include "base/component_export.h"
#include "base/time/time.h"
#include "chromeos/ash/components/device_activity/churn_active_status.h"
#include "chromeos/ash/components/device_activity/device_active_use_case.h"
#include "fresnel_service.pb.h"

class PrefService;

namespace version_info {
enum class Channel;
}  // namespace version_info

namespace ash::device_activity {

// Forward declaration from fresnel_service.proto.
class FresnelImportDataRequest;

// Contains the methods required to report the churn observation device active.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_DEVICE_ACTIVITY)
    ChurnObservationUseCaseImpl : public DeviceActiveUseCase {
 public:
  ChurnObservationUseCaseImpl(
      ChurnActiveStatus* churn_active_status_ptr,
      const std::string& psm_device_active_secret,
      const ChromeDeviceMetadataParameters& chrome_passed_device_params,
      PrefService* local_state,
      std::unique_ptr<PsmDelegateInterface> psm_delegate);
  ChurnObservationUseCaseImpl(const ChurnObservationUseCaseImpl&) = delete;
  ChurnObservationUseCaseImpl& operator=(const ChurnObservationUseCaseImpl&) =
      delete;
  ~ChurnObservationUseCaseImpl() override;

  // DeviceActiveUseCase:
  std::string GenerateWindowIdentifier(base::Time ts) const override;
  absl::optional<FresnelImportDataRequest> GenerateImportRequestBody() override;
  bool IsEnabledCheckIn() override;
  bool IsEnabledCheckMembership() override;
  private_computing::ActiveStatus GenerateActiveStatus() override;

 private:
  // The observation use case generates 3 observation window identifiers
  // for the 3 periods that it will need to ping for.
  // Sets the |observation_period_minus_0_id_|,
  // |observation_period_minus_1_id_|, and |observation_period_minus_2_id_|
  // based on the current ts month.
  //
  // For example, for the ts representing the date 03/01/2022, this method will
  // set the observation period strings to:
  // 202203-202205 ,202202-202204, and 202201-202203 respectively.
  void SetObservationPeriodWindowIds(base::Time ts);

  // Generates FresnelImportData message given the window identifier.
  // The churn observation use case will call this method 3 times for each of
  // it's observation windows.
  FresnelImportData GenerateObservationFresnelImportData(
      const std::string& observation_window_id) const;

  // TODO(hirthanan): Implement following three methods in new CL.
  bool IsPreviousMonthlyActive() const;

  bool IsPreviousYearlyActive() const;

  ChurnObservationMetadata::FirstActiveDuringCohort GetFirstActiveDuringCohort()
      const;

  ChurnActiveStatus* const churn_active_status_ptr_;

  std::string observation_period_minus_0_id_;
  std::string observation_period_minus_1_id_;
  std::string observation_period_minus_2_id_;
};

}  // namespace ash::device_activity

#endif  // CHROMEOS_ASH_COMPONENTS_DEVICE_ACTIVITY_CHURN_OBSERVATION_USE_CASE_IMPL_H_
