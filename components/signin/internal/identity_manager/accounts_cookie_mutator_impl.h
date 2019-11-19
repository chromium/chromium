// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_ACCOUNTS_COOKIE_MUTATOR_IMPL_H_
#define COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_ACCOUNTS_COOKIE_MUTATOR_IMPL_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "build/build_config.h"
#include "components/signin/public/identity_manager/accounts_cookie_mutator.h"

class AccountTrackerService;
class GaiaCookieManagerService;

namespace gaia {
class GaiaSource;
}

namespace signin {

// Concrete implementation of the AccountsCookieMutator interface.
class AccountsCookieMutatorImpl : public AccountsCookieMutator {
 public:
  explicit AccountsCookieMutatorImpl(
      GaiaCookieManagerService* gaia_cookie_manager_service,
      AccountTrackerService* account_tracker_service);
  ~AccountsCookieMutatorImpl() override;

  void AddAccountToCookie(
      const CoreAccountId& account_id,
      gaia::GaiaSource source,
      AddAccountToCookieCompletedCallback completion_callback) override;

  void AddAccountToCookieWithToken(
      const CoreAccountId& account_id,
      const std::string& access_token,
      gaia::GaiaSource source,
      AddAccountToCookieCompletedCallback completion_callback) override;

  void SetAccountsInCookie(
      const MultiloginParameters& parameters,
      gaia::GaiaSource source,
      base::OnceCallback<void(SetAccountsInCookieResult)>
          set_accounts_in_cookies_completed_callback) override;

  void TriggerCookieJarUpdate() override;

#if defined(OS_IOS)
  void ForceTriggerOnCookieChange() override;
#endif

  void LogOutAllAccounts(gaia::GaiaSource source) override;

 private:
  GaiaCookieManagerService* gaia_cookie_manager_service_;
  AccountTrackerService* account_tracker_service_;

  DISALLOW_COPY_AND_ASSIGN(AccountsCookieMutatorImpl);
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_ACCOUNTS_COOKIE_MUTATOR_IMPL_H_
