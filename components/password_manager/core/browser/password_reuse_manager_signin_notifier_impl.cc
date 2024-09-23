// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_reuse_manager_signin_notifier_impl.h"

#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_reuse_manager.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace password_manager {

PasswordReuseManagerSigninNotifierImpl::PasswordReuseManagerSigninNotifierImpl(
    signin::IdentityManager* identity_manager)
    : identity_manager_(identity_manager) {
  DCHECK(identity_manager_);
}

PasswordReuseManagerSigninNotifierImpl::~PasswordReuseManagerSigninNotifierImpl() = default;

void PasswordReuseManagerSigninNotifierImpl::SubscribeToSigninEvents(
    PasswordReuseManager* reuse_manager) {
  reuse_manager_ = reuse_manager;
  identity_manager_->AddObserver(this);
}

void PasswordReuseManagerSigninNotifierImpl::UnsubscribeFromSigninEvents() {
  identity_manager_->RemoveObserver(this);
}

void PasswordReuseManagerSigninNotifierImpl::NotifySignedOut(
    const std::string& username,
    bool syncing_account) {
  if (!reuse_manager_) {
    return;
  }

  if (syncing_account) {
    metrics_util::LogGaiaPasswordHashChange(
        metrics_util::GaiaPasswordHashChange::CLEARED_ON_CHROME_SIGNOUT,
        /*is_sync_password=*/true);
    reuse_manager_->ClearAllGaiaPasswordHash();
  } else {
    metrics_util::LogGaiaPasswordHashChange(
        metrics_util::GaiaPasswordHashChange::CLEARED_ON_CHROME_SIGNOUT,
        /*is_sync_password=*/false);
    reuse_manager_->ClearGaiaPasswordHash(username);
  }
}

// IdentityManager::Observer implementation.
void PasswordReuseManagerSigninNotifierImpl::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event) {
  // TODO(crbug.com/40067058): Remove this code when ConsentLevel::kSync is
  // deleted (since kSignin users are handled by
  // OnExtendedAccountInfoRemoved()). See ConsentLevel::kSync documentation for
  // details.
  if (event.GetEventTypeFor(signin::ConsentLevel::kSync) ==
      signin::PrimaryAccountChangeEvent::Type::kCleared) {
    NotifySignedOut(event.GetPreviousState().primary_account.email,
                    /*syncing_account=*/true);
  }
}

// IdentityManager::Observer implementation.
void PasswordReuseManagerSigninNotifierImpl::OnExtendedAccountInfoRemoved(
    const AccountInfo& info) {
  // Only react to non-syncing Gaia account sign-out event - the syncing
  // account is handled separately in OnPrimaryAccountChanged().
  // TODO(crbug.com/40067058): Remove the not-kSync check when
  // ConsentLevel::kSync is deleted. See ConsentLevel::kSync documentation for
  // details.
  if (info.account_id !=
      identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSync)) {
    NotifySignedOut(info.email, /*syncing_account=*/false);
  }
}

}  // namespace password_manager
