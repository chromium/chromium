// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DEVICE_ACTIVITY_CHURN_OBSERVATION_USE_CASE_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_DEVICE_ACTIVITY_CHURN_OBSERVATION_USE_CASE_IMPL_H_

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
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

// For a given device active month, there can be up to 3 observation windows
// that may need to be sent to Fresnel to compute monthly, yearly, and first
// active churn.
//
// For example, a device that comes online in Feb 2023 that has never sent
// observation windows for previous months may send.
// (Period, Observation Window)
// (0, 202302-202304)
// (1, 202301-202303)
// (2, 202212-202302)
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_DEVICE_ACTIVITY)
    ObservationWindow {
 public:
  static constexpr char kObservationPeriodFormat[] = "YYYYMM-YYYYMM";

  ObservationWindow() = default;
  ObservationWindow(int period, const std::string& observation_period);
  ObservationWindow(const ObservationWindow&) = delete;
  ObservationWindow& operator=(const ObservationWindow&) = default;
  ~ObservationWindow() = default;

  bool IsObservationWindowSet() const;

  int GetPeriod() const;

  const std::string& GetObservationPeriod() const;

  bool SetPeriod(int period);

  bool SetObservationPeriod(const std::string& observation_period);

  void Reset();

 private:
  // Observation window period should be between [0, 2].
  int period_ = -1;

  // String representing the observation period formatted
  // as "YYYYMM-YYYYMM" where the period length is 3 months.
  std::string observation_period_;
};

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
  std::string GetObservationPeriod(int period) override;

 private:
  // On successful churn cohort check in, the active status object is updated to
  // the current month. If churn cohort check in failed this month, the
  // active status object will reflect an old month.
  bool CohortCheckInSuccessfullyUpdatedActiveStatus() const;

  // The observation use case generates 3 observation window identifiers
  // for the 3 periods that it will need to ping for.
  // Sets the |observation_window_0_|, |observation_window_1_|,
  // and |observation_window_2_| based on the current ts month.
  //
  // For example, for the ts representing the date 03/01/2022, this method will
  // set the observation period strings to:
  // 202203-202205 ,202202-202204, and 202201-202203 respectively.
  void SetObservationPeriodWindows(base::Time ts);

  // Generates FresnelImportData message given the window identifier.
  // The churn observation use case will call this method 3 times for each of
  // it's observation windows.
  FresnelImportData GenerateObservationFresnelImportData(
      const ObservationWindow& observation_window) const;

  bool IsPreviousMonthlyActive(
      const ObservationWindow& observation_window) const;

  bool IsPreviousYearlyActive(
      const ObservationWindow& observation_window) const;

  absl::optional<ChurnObservationMetadata::FirstActiveDuringCohort>
  GetFirstActiveDuringCohort(const ObservationWindow& observation_window) const;

  const raw_ptr<ChurnActiveStatus, ExperimentalAsh> churn_active_status_ptr_;

  ObservationWindow observation_window_0_;
  ObservationWindow observation_window_1_;
  ObservationWindow observation_window_2_;
};

}  // namespace ash::device_activity

#endif  // CHROMEOS_ASH_COMPONENTS_DEVICE_ACTIVITY_CHURN_OBSERVATION_USE_CASE_IMPL_H_
