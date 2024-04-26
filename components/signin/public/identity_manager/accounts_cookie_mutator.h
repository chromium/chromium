// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_ACCOUNTS_COOKIE_MUTATOR_H_
#define COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_ACCOUNTS_COOKIE_MUTATOR_H_

#include <string>

#include "build/build_config.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"

class GoogleServiceAuthError;

namespace network::mojom {
class CookieManager;
}

namespace signin {

struct MultiloginParameters;
enum class SetAccountsInCookieResult;

// AccountsCookieMutator is the interface to support merging known local Google
// accounts into the cookie jar tracking the list of logged-in Google sessions.
class AccountsCookieMutator {
 public:
  // Delegate class used to interact with storage partitions other than the
  // default one. The default storage partition is managed by the SigninClient.
  class PartitionDelegate {
   public:
    // Creates a new GaiaAuthFetcher for the partition.
    virtual std::unique_ptr<GaiaAuthFetcher> CreateGaiaAuthFetcherForPartition(
        GaiaAuthConsumer* consumer,
        const gaia::GaiaSource& source) = 0;

    // Returns the CookieManager for the partition.
    virtual network::mojom::CookieManager* GetCookieManagerForPartition() = 0;
  };

  // Task handle for SetAccountsInCookieForPartition. Deleting this object
  // cancels the task. Must not outlive the AccountsInCookieMutator.
  class SetAccountsInCookieTask {
   public:
    virtual ~SetAccountsInCookieTask() = default;
  };

  AccountsCookieMutator() = default;

  AccountsCookieMutator(const AccountsCookieMutator&) = delete;
  AccountsCookieMutator& operator=(const AccountsCookieMutator&) = delete;

  virtual ~AccountsCookieMutator() = default;

  typedef base::OnceCallback<void(const GoogleServiceAuthError& error)>
      LogOutFromCookieCompletedCallback;

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

#if BUILDFLAG(IS_IOS)
  // Forces the processing of GaiaCookieManagerService::OnCookieChange. On
  // iOS, it's necessary to force-trigger the processing of cookie changes
  // from the client as the normal mechanism for internally observing them
  // is not wired up.
  // TODO(crbug.com/40613324) : Remove the need to expose this method
  // or move it to the network::CookieManager.
  virtual void ForceTriggerOnCookieChange() = 0;
#endif

  // Remove all accounts from the Gaia cookie.
  // Note: this only clears the Gaia cookies. Other cookies such as the SAML
  // provider cookies are not cleared. To cleanly remove an account from the
  // web, the Gaia logout page should be loaded as a navigation.
  virtual void LogOutAllAccounts(
      gaia::GaiaSource source,
      LogOutFromCookieCompletedCallback completion_callback) = 0;

  // Indicates that an account previously listed via ListAccounts should now
  // be removed.
  virtual void RemoveLoggedOutAccountByGaiaId(const std::string& gaia_id) = 0;
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_ACCOUNTS_COOKIE_MUTATOR_H_
