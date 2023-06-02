// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/browser/ash/user_permission_service_ash.h"

#include <utility>

#include "ash/constants/ash_features.h"
#include "components/device_signals/core/browser/user_delegate.h"

namespace device_signals {

UserPermissionServiceAsh::UserPermissionServiceAsh(
    policy::ManagementService* management_service,
    std::unique_ptr<UserDelegate> user_delegate,
    PrefService* user_prefs)
    : UserPermissionServiceImpl(management_service,
                                std::move(user_delegate),
                                user_prefs) {}

UserPermissionServiceAsh::~UserPermissionServiceAsh() = default;

bool UserPermissionServiceAsh::ShouldCollectConsent() const {
  // Consent flow is not supported on CrOS yet.
  return false;
}

UserPermission UserPermissionServiceAsh::CanCollectSignals() const {
  if (IsDeviceCloudManaged() &&
      (user_delegate_->IsSigninContext() || user_delegate_->IsAffiliated())) {
    return UserPermission::kGranted;
  }

  if (ash::features::IsUnmanagedDeviceDeviceTrustConnectorFeatureEnabled() &&
      !IsDeviceCloudManaged() && user_delegate_->IsManagedUser()) {
    return UserPermission::kGranted;
  }

  // Other use-cases are currently unsupported. Further scenarios breakdown will
  // be added in the future.
  return UserPermission::kUnsupported;
}

}  // namespace device_signals
