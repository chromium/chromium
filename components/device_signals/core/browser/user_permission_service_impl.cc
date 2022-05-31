// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/browser/user_permission_service_impl.h"

#include "base/check.h"
#include "components/device_signals/core/browser/user_context.h"
#include "components/device_signals/core/browser/user_delegate.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace device_signals {

UserPermissionServiceImpl::UserPermissionServiceImpl(
    signin::IdentityManager* identity_manager,
    policy::ManagementService* management_service,
    std::unique_ptr<UserDelegate> user_delegate)
    : identity_manager_(identity_manager),
      management_service_(management_service),
      user_delegate_(std::move(user_delegate)) {
  DCHECK(identity_manager_);
  DCHECK(management_service_);
  DCHECK(user_delegate_);
}

UserPermissionServiceImpl::~UserPermissionServiceImpl() = default;

void UserPermissionServiceImpl::CanCollectSignals(
    const UserContext& user_context,
    UserPermissionService::CanCollectCallback callback) {
  // Return "unknown user" if the user ID is invalid, or does not represent a
  // logged-in user.
  if (user_context.user_id.empty()) {
    std::move(callback).Run(UserPermission::kUnknownUser);
    return;
  }

  // TODO(b:233250828): Verify this function covers the required use cases.
  auto account_info =
      identity_manager_->FindExtendedAccountInfoByGaiaId(user_context.user_id);

  if (account_info.IsEmpty()) {
    std::move(callback).Run(UserPermission::kUnknownUser);
    return;
  }

  // Return "consumer user" if the user is not managed by an organization.
  if (!account_info.IsManaged()) {
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

  // If the account info represents the current browser user (e.g. Profile
  // user), verify if that user is part of an affiliated organization.
  if (user_delegate_->IsSameManagedUser(account_info)) {
    UserPermission permission = user_delegate_->IsAffiliated()
                                    ? UserPermission::kGranted
                                    : UserPermission::kUnaffiliated;
    std::move(callback).Run(permission);
    return;
  }

  // TODO(b:232269863): Fetch customer IDs for that user by calling the DM
  // server, then cache results here (and clear cache when the account is
  // logged-out).
  std::move(callback).Run(UserPermission::kUnaffiliated);
}

}  // namespace device_signals
