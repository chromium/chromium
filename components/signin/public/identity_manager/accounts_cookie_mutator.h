// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_ACCOUNTS_COOKIE_MUTATOR_H_
#define COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_ACCOUNTS_COOKIE_MUTATOR_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "build/build_config.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"

struct CoreAccountId;
class GoogleServiceAuthError;

namespace signin {

struct MultiloginParameters;
enum class SetAccountsInCookieResult;

// AccountsCookieMutator is the interface to support merging known local Google
// accounts into the cookie jar tracking the list of logged-in Google sessions.
class AccountsCookieMutator {
 public:
  AccountsCookieMutator() = default;
  virtual ~AccountsCookieMutator() = default;

  typedef base::OnceCallback<void(const CoreAccountId& account_id,
                                  const GoogleServiceAuthError& error)>
      AddAccountToCookieCompletedCallback;

  // Adds an account identified by |account_id| to the cookie responsible for
  // tracking the list of logged-in Google sessions across the web.
  virtual void AddAccountToCookie(
      const CoreAccountId& account_id,
      gaia::GaiaSource source,
      AddAccountToCookieCompletedCallback completion_callback) = 0;

  // Adds an account identified by |account_id| and with |access_token| to the
  // cookie responsible for tracking the list of logged-in Google sessions
  // across the web.
  virtual void AddAccountToCookieWithToken(
      const CoreAccountId& account_id,
      const std::string& access_token,
      gaia::GaiaSource source,
      AddAccountToCookieCompletedCallback completion_callback) = 0;

  // Updates the state of the Gaia cookie to contain the accounts in
  // |parameters|.
  // If the mode is MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER the order of the
  // accounts will be enforced, and any extra account (including invalid
  // sessions) will be removed.
  // If the mode is MULTILOGIN_PRESERVE_COOKIE_ACCOUNTS_ORDER, the ordering of
  // accounts will not be enforced. Additionally, extra accounts won't be
  // removed from the cookie but only invalidated.
  // |set_accounts_in_cookies_completed_callback| will be invoked with the
  // result of the operation: if the error is equal to
  // GoogleServiceAuthError::AuthErrorNone() then the operation succeeded.
  // Notably, if there are accounts being added for which IdentityManager does
  // not have refresh tokens, the operation will fail with a
  // GoogleServiceAuthError::USER_NOT_SIGNED_UP error.
  virtual void SetAccountsInCookie(
      const MultiloginParameters& parameters,
      gaia::GaiaSource source,
      base::OnceCallback<void(SetAccountsInCookieResult)>
          set_accounts_in_cookies_completed_callback) = 0;

  // Triggers a ListAccounts fetch. Can be used in circumstances where clients
  // know that the contents of the Gaia cookie might have changed.
  virtual void TriggerCookieJarUpdate() = 0;

#if defined(OS_IOS)
  // Forces the processing of GaiaCookieManagerService::OnCookieChange. On
  // iOS, it's necessary to force-trigger the processing of cookie changes
  // from the client as the normal mechanism for internally observing them
  // is not wired up.
  // TODO(https://crbug.com/930582) : Remove the need to expose this method
  // or move it to the network::CookieManager.
  virtual void ForceTriggerOnCookieChange() = 0;
#endif

  // Remove all accounts from the Gaia cookie.
  virtual void LogOutAllAccounts(gaia::GaiaSource source) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(AccountsCookieMutator);
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_ACCOUNTS_COOKIE_MUTATOR_H_
