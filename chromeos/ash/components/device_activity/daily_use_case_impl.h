// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DEVICE_ACTIVITY_DAILY_USE_CASE_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_DEVICE_ACTIVITY_DAILY_USE_CASE_IMPL_H_

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

// Contains the methods required to report the daily active use case.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_DEVICE_ACTIVITY) DailyUseCaseImpl
    : public DeviceActiveUseCase {
 public:
  DailyUseCaseImpl(
      const std::string& psm_device_active_secret,
      const ChromeDeviceMetadataParameters& chrome_passed_device_params,
      PrefService* local_state,
      std::unique_ptr<PsmDelegateInterface> psm_delegate);
  DailyUseCaseImpl(const DailyUseCaseImpl&) = delete;
  DailyUseCaseImpl& operator=(const DailyUseCaseImpl&) = delete;
  ~DailyUseCaseImpl() override;

  // DeviceActiveUseCase:
  absl::optional<FresnelImportDataRequest> GenerateImportRequestBody() override;

  // Whether current device active use case check-in is enabled or not.
  bool IsEnabledCheckIn() override;

 // Whether current device active use case check membership is enabled or not.
  bool IsEnabledCheckMembership() override;

  private_computing::ActiveStatus GenerateActiveStatus() override;
};

}  // namespace ash::device_activity

#endif  // CHROMEOS_ASH_COMPONENTS_DEVICE_ACTIVITY_DAILY_USE_CASE_IMPL_H_
