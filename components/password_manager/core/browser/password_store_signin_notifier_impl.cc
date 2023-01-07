// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store_signin_notifier_impl.h"

#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_reuse_manager.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace password_manager {

PasswordStoreSigninNotifierImpl::PasswordStoreSigninNotifierImpl(
    signin::IdentityManager* identity_manager)
    : identity_manager_(identity_manager) {
  DCHECK(identity_manager_);
}

PasswordStoreSigninNotifierImpl::~PasswordStoreSigninNotifierImpl() = default;

void PasswordStoreSigninNotifierImpl::SubscribeToSigninEvents(
    PasswordReuseManager* reuse_manager) {
  reuse_manager_ = reuse_manager;
  identity_manager_->AddObserver(this);
}

void PasswordStoreSigninNotifierImpl::UnsubscribeFromSigninEvents() {
  identity_manager_->RemoveObserver(this);
}

void PasswordStoreSigninNotifierImpl::NotifySignedOut(
    const std::string& username,
    bool primary_account) {
  if (!reuse_manager_)
    return;

  if (primary_account) {
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
void PasswordStoreSigninNotifierImpl::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event) {
  if (event.GetEventTypeFor(signin::ConsentLevel::kSync) ==
      signin::PrimaryAccountChangeEvent::Type::kCleared) {
    NotifySignedOut(event.GetPreviousState().primary_account.email,
                    /* primary_account= */ true);
  }
}

// IdentityManager::Observer implementation.
void PasswordStoreSigninNotifierImpl::OnExtendedAccountInfoRemoved(
    const AccountInfo& info) {
  // Only reacts to content area (non-primary) Gaia account sign-out event.
  if (info.account_id !=
      identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSync)) {
    NotifySignedOut(info.email, /* primary_account= */ false);
  }
}

}  // namespace password_manager
