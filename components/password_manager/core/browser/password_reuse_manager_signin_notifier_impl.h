// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_REUSE_MANAGER_SIGNIN_NOTIFIER_IMPL_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_REUSE_MANAGER_SIGNIN_NOTIFIER_IMPL_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "components/password_manager/core/browser/password_reuse_manager_signin_notifier.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace password_manager {

// Responsible for subscribing to Google identity sign-in events and passing
// them to PasswordReuseManager.
class PasswordReuseManagerSigninNotifierImpl
    : public PasswordReuseManagerSigninNotifier,
      public signin::IdentityManager::Observer {
 public:
  explicit PasswordReuseManagerSigninNotifierImpl(
      signin::IdentityManager* identity_manager);
  ~PasswordReuseManagerSigninNotifierImpl() override;
  PasswordReuseManagerSigninNotifierImpl(const PasswordReuseManagerSigninNotifierImpl&) =
      delete;
  PasswordReuseManagerSigninNotifierImpl& operator=(
      const PasswordReuseManagerSigninNotifierImpl&) = delete;

  // PasswordReuseManagerSigninNotifier implementations.
  void SubscribeToSigninEvents(PasswordReuseManager* reuse_manager) override;
  void UnsubscribeFromSigninEvents() override;

  // IdentityManager::Observer implementations.
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event) override;
  void OnExtendedAccountInfoRemoved(const AccountInfo& info) override;

 private:
  // Passes the sign-out notification to `reuse_manager_`.
  void NotifySignedOut(const std::string& username, bool syncing_account);

  raw_ptr<signin::IdentityManager> identity_manager_ = nullptr;

  raw_ptr<PasswordReuseManager> reuse_manager_ = nullptr;  // weak
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_REUSE_MANAGER_SIGNIN_NOTIFIER_IMPL_H_
