// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/browser/user_permission_service_impl.h"

#include "base/check.h"
#include "components/device_signals/core/browser/user_context.h"
#include "components/device_signals/core/browser/user_delegate.h"
#include "components/policy/core/common/management/management_service.h"

namespace device_signals {

UserPermissionServiceImpl::UserPermissionServiceImpl(
    policy::ManagementService* management_service,
    std::unique_ptr<UserDelegate> user_delegate)
    : management_service_(management_service),
      user_delegate_(std::move(user_delegate)) {
  DCHECK(management_service_);
  DCHECK(user_delegate_);
}

UserPermissionServiceImpl::~UserPermissionServiceImpl() = default;

void UserPermissionServiceImpl::CanCollectSignals(
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

  // Automatically return "missing consent" if the browser is not managed. This
  // specific check is temporary until there is a flow in Chrome to gather user
  // consent, at which point lack of browser management would lead into a user
  // consent check.
  if (!management_service_->HasManagementAuthority(
          policy::EnterpriseManagementAuthority::CLOUD_DOMAIN)) {
    std::move(callback).Run(UserPermission::kMissingConsent);
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

}  // namespace device_signals
