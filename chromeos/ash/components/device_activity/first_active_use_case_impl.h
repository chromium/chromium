// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DEVICE_ACTIVITY_FIRST_ACTIVE_USE_CASE_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_DEVICE_ACTIVITY_FIRST_ACTIVE_USE_CASE_IMPL_H_

#include "base/component_export.h"
#include "base/time/time.h"
#include "chromeos/ash/components/device_activity/device_active_use_case.h"

class PrefService;

namespace version_info {
enum class Channel;
}  // namespace version_info

namespace ash {
namespace device_activity {

// Forward declaration from fresnel_service.proto.
class ImportDataRequest;

// Contains the methods required to report the first active use case.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_DEVICE_ACTIVITY)
    FirstActiveUseCaseImpl : public DeviceActiveUseCase {
 public:
  FirstActiveUseCaseImpl(
      const std::string& psm_device_active_secret,
      const ChromeDeviceMetadataParameters& chrome_passed_device_params,
      PrefService* local_state);
  FirstActiveUseCaseImpl(const FirstActiveUseCaseImpl&) = delete;
  FirstActiveUseCaseImpl& operator=(const FirstActiveUseCaseImpl&) = delete;
  ~FirstActiveUseCaseImpl() override;

  // DeviceActiveUseCase:
  std::string GenerateUTCWindowIdentifier(base::Time ts) const override;

  // DeviceActiveUseCase:
  ImportDataRequest GenerateImportRequestBody() override;
};

}  // namespace device_activity
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DEVICE_ACTIVITY_FIRST_ACTIVE_USE_CASE_IMPL_H_
