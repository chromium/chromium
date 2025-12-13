// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/experiences/arc/arc_platform_support_impl.h"

#include "base/check.h"
#include "base/logging.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "chromeos/ash/components/settings/cros_settings.h"

namespace arc {

bool ArcPlatformSupportImpl::IsDlcEnabled() const {
  // This will crash if CheckDlcRequirement has not been called.
  // This is the intended behavior as the value should be checked first.
  CHECK(is_arcvm_dlc_enabled_.has_value());
  return is_arcvm_dlc_enabled_.value();
}

void ArcPlatformSupportImpl::CheckDlcRequirement() {
  CHECK(!is_arcvm_dlc_enabled_.has_value());
  CHECK(ash::InstallAttributes::IsInitialized());
  CHECK(ash::CrosSettings::IsInitialized());

  if (!ash::InstallAttributes::Get()->IsEnterpriseManaged()) {
    VLOG(1) << "Device is not managed; ARCVM DLC is not enabled "
               "based on enterprise policy.";
    is_arcvm_dlc_enabled_ = false;
    return;
  }

  bool device_flex_arc_preload_enabled = false;
  if (!ash::CrosSettings::Get()->GetBoolean(ash::kDeviceFlexArcPreloadEnabled,
                                            &device_flex_arc_preload_enabled)) {
    VLOG(1) << "Failed to get DeviceFlexArcPreloadEnabled policy; defaulting "
               "to DLC not enabled.";
    is_arcvm_dlc_enabled_ = false;
    return;
  }

  if (!device_flex_arc_preload_enabled) {
    VLOG(1) << "ARCVM DLC is not enabled because "
               "DeviceFlexArcPreloadEnabled policy is false.";
    is_arcvm_dlc_enabled_ = false;
    return;
  }

  VLOG(1) << "ARCVM DLC is enabled for this managed device.";
  is_arcvm_dlc_enabled_ = true;
}

}  // namespace arc
