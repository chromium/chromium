// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/browser/user_permission_service_impl.h"

#include "base/check.h"
#include "components/device_signals/core/browser/pref_names.h"
#include "components/device_signals/core/browser/user_context.h"
#include "components/device_signals/core/browser/user_delegate.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/prefs/pref_service.h"

namespace device_signals {

UserPermissionServiceImpl::UserPermissionServiceImpl(
    policy::ManagementService* management_service,
    std::unique_ptr<UserDelegate> user_delegate,
    PrefService* user_prefs)
    : management_service_(management_service),
      user_delegate_(std::move(user_delegate)),
      user_prefs_(user_prefs) {
  CHECK(management_service_);
  CHECK(user_delegate_);
  CHECK(user_prefs_);
}

UserPermissionServiceImpl::~UserPermissionServiceImpl() = default;

void UserPermissionServiceImpl::CanUserCollectSignals(
    const UserContext& user_context,
    UserPermissionService::CanCollectCallback callback) {
  // Return "unknown user" if no user ID was given.
  if (user_context.user_id.empty()) {
    std::move(callback).Run(UserPermission::kMissingUser);
    return;
  }

  if (!user_delegate_->IsSameUser(user_context.user_id)) {
    std::move(callback).Run(UserPermission::kUnknownUser);
    return;
  }

  if (!user_delegate_->IsManaged()) {
    std::move(callback).Run(UserPermission::kConsumerUser);
    return;
  }

  // User consent is required on unmanaged devices. Collection of the user's
  // consent happens via its own flow and hooks, so only the resulting value
  // needs to be evaluated here.
  if (!management_service_->HasManagementAuthority(
          policy::EnterpriseManagementAuthority::CLOUD_DOMAIN)) {
    std::move(callback).Run(HasUserConsented()
                                ? UserPermission::kGranted
                                : UserPermission::kMissingConsent);
    return;
  }

  if (!user_delegate_->IsAffiliated()) {
    std::move(callback).Run(UserPermission::kUnaffiliated);
    return;
  }

  // At this point, the given user is:
  // - The same user as the browser user,
  // - Is managed by an org affiliated with the org managing the browser.
  // They are, therefore, allowed to collect signals.
  std::move(callback).Run(UserPermission::kGranted);
}

void UserPermissionServiceImpl::CanCollectSignals(
    UserPermissionService::CanCollectCallback callback) {
  // For now, the only condition that is required is that the current
  // browser is Cloud-managed. The rationale being that signals can be
  // collected on managed devices by their managing organization, but
  // would require more scrutiny for unmanaged browsers (including
  // getting user consent). However, support for unmanaged browsers is
  // not required yet.
  if (!management_service_->HasManagementAuthority(
          policy::EnterpriseManagementAuthority::CLOUD_DOMAIN)) {
    std::move(callback).Run(UserPermission::kMissingConsent);
    return;
  }
  std::move(callback).Run(UserPermission::kGranted);
}

bool UserPermissionServiceImpl::HasUserConsented() const {
  return user_prefs_->GetBoolean(prefs::kDeviceSignalsConsentReceived);
}

}  // namespace device_signals
