// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_ACCOUNTS_COOKIE_MUTATOR_IMPL_H_
#define COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_ACCOUNTS_COOKIE_MUTATOR_IMPL_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "components/signin/internal/identity_manager/oauth_multilogin_helper.h"
#include "components/signin/public/identity_manager/accounts_cookie_mutator.h"

class AccountTrackerService;
class GaiaCookieManagerService;
class ProfileOAuth2TokenService;
class SigninClient;

namespace gaia {
class GaiaSource;
}

namespace signin {

// Concrete implementation of the AccountsCookieMutator interface.
class AccountsCookieMutatorImpl : public AccountsCookieMutator {
 public:
  explicit AccountsCookieMutatorImpl(
      SigninClient* signin_client,
      ProfileOAuth2TokenService* token_service,
      GaiaCookieManagerService* gaia_cookie_manager_service,
      AccountTrackerService* account_tracker_service);

  AccountsCookieMutatorImpl(const AccountsCookieMutatorImpl&) = delete;
  AccountsCookieMutatorImpl& operator=(const AccountsCookieMutatorImpl&) =
      delete;

  ~AccountsCookieMutatorImpl() override;

  void SetAccountsInCookie(
      const MultiloginParameters& parameters,
      gaia::GaiaSource source,
      base::OnceCallback<void(SetAccountsInCookieResult)>
          set_accounts_in_cookies_completed_callback) override;

  void TriggerCookieJarUpdate() override;

#if BUILDFLAG(IS_IOS)
  void ForceTriggerOnCookieChange() override;
#endif

  void LogOutAllAccounts(
      gaia::GaiaSource source,
      LogOutFromCookieCompletedCallback completion_callback) override;

  void RemoveLoggedOutAccountByGaiaId(const std::string& gaia_id) override;

 private:
  raw_ptr<SigninClient> signin_client_;
  raw_ptr<ProfileOAuth2TokenService> token_service_;
  raw_ptr<GaiaCookieManagerService> gaia_cookie_manager_service_;
  raw_ptr<AccountTrackerService> account_tracker_service_;
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_ACCOUNTS_COOKIE_MUTATOR_IMPL_H_
