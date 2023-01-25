// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DEVICE_ACTIVITY_TWENTY_EIGHT_DAY_ACTIVE_USE_CASE_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_DEVICE_ACTIVITY_TWENTY_EIGHT_DAY_ACTIVE_USE_CASE_IMPL_H_

#include "base/component_export.h"
#include "base/time/time.h"
#include "chromeos/ash/components/device_activity/device_active_use_case.h"
#include "third_party/private_membership/src/private_membership_rlwe_client.h"

class PrefService;

namespace version_info {
enum class Channel;
}  // namespace version_info

namespace ash::device_activity {

// Forward declaration from fresnel_service.proto.
class FresnelImportDataRequest;

// Contains the methods required to report the 28 day active (28da) use case.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_DEVICE_ACTIVITY)
    TwentyEightDayActiveUseCaseImpl : public DeviceActiveUseCase {
 public:
  TwentyEightDayActiveUseCaseImpl(
      const std::string& psm_device_active_secret,
      const ChromeDeviceMetadataParameters& chrome_passed_device_params,
      PrefService* local_state,
      std::unique_ptr<PsmDelegateInterface> psm_delegate);
  TwentyEightDayActiveUseCaseImpl(const TwentyEightDayActiveUseCaseImpl&) =
      delete;
  TwentyEightDayActiveUseCaseImpl& operator=(
      const TwentyEightDayActiveUseCaseImpl&) = delete;
  ~TwentyEightDayActiveUseCaseImpl() override;

  // DeviceActiveUseCase:
  absl::optional<FresnelImportDataRequest> GenerateImportRequestBody() override;
  private_computing::ActiveStatus GenerateActiveStatus() override;

  // Whether current device active use case check-in is enabled or not.
  bool IsEnabledCheckIn() override;

  // Whether current device active use case check membership is enabled or not.
  bool IsEnabledCheckMembership() override;

  // For example, the 28 day lookback queries on 01/28/2022 will generate the
  // vector of psm ids for days 01, 02, 03, 04, 05, 06, ..., 28 of January 2022.
  bool SavePsmIdToDateMap(base::Time cur_ts) override;

  // For example, the 28 day ping ahead imports on 01/01/2022 will generate the
  // vector of psm ids for days 01, 02, 03, 04, 05, 06, ..., 28 of January 2022.
  bool SetPsmIdentifiersToImport(base::Time cur_ts) override;
};

}  // namespace ash::device_activity

#endif  // CHROMEOS_ASH_COMPONENTS_DEVICE_ACTIVITY_TWENTY_EIGHT_DAY_ACTIVE_USE_CASE_IMPL_H_
