// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_PROFILE_OAUTH2_TOKEN_SERVICE_DELEGATE_CHROMEOS_H_
#define COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_PROFILE_OAUTH2_TOKEN_SERVICE_DELEGATE_CHROMEOS_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/account_manager_core/account.h"
#include "components/account_manager_core/account_manager_facade.h"
#include "components/signin/internal/identity_manager/profile_oauth2_token_service_delegate.h"
#include "services/network/public/cpp/network_connection_tracker.h"

class AccountTrackerService;
class SigninClient;

namespace signin {
class ProfileOAuth2TokenServiceDelegateChromeOS
    : public ProfileOAuth2TokenServiceDelegate,
      public account_manager::AccountManagerFacade::Observer,
      public network::NetworkConnectionTracker::NetworkConnectionObserver {
 public:
  // Accepts non-owning pointers to `AccountTrackerService`,
  // `NetworkConnectorTracker`, and `account_manager::AccountManagerFacade`.
  // These objects must all outlive `this` delegate.
  ProfileOAuth2TokenServiceDelegateChromeOS(
      SigninClient* signin_client,
      AccountTrackerService* account_tracker_service,
      network::NetworkConnectionTracker* network_connection_tracker,
      account_manager::AccountManagerFacade* account_manager_facade,
      bool is_regular_profile);

  ProfileOAuth2TokenServiceDelegateChromeOS(
      const ProfileOAuth2TokenServiceDelegateChromeOS&) = delete;
  ProfileOAuth2TokenServiceDelegateChromeOS& operator=(
      const ProfileOAuth2TokenServiceDelegateChromeOS&) = delete;

  ~ProfileOAuth2TokenServiceDelegateChromeOS() override;

  // ProfileOAuth2TokenServiceDelegate overrides.
  std::unique_ptr<OAuth2AccessTokenFetcher> CreateAccessTokenFetcher(
      const CoreAccountId& account_id,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      OAuth2AccessTokenConsumer* consumer) override;
  bool RefreshTokenIsAvailable(const CoreAccountId& account_id) const override;
  void UpdateAuthError(const CoreAccountId& account_id,
                       const GoogleServiceAuthError& error) override;
  void UpdateAuthErrorInternal(const CoreAccountId& account_id,
                               const GoogleServiceAuthError& error,
                               bool fire_auth_error_changed = true);
  GoogleServiceAuthError GetAuthError(
      const CoreAccountId& account_id) const override;
  std::vector<CoreAccountId> GetAccounts() const override;
  void LoadCredentials(const CoreAccountId& primary_account_id) override;
  void UpdateCredentials(const CoreAccountId& account_id,
                         const std::string& refresh_token) override;
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory()
      const override;
  void RevokeCredentials(const CoreAccountId& account_id) override;
  void RevokeAllCredentials() override;
  const net::BackoffEntry* BackoffEntry() const override;

  // `account_manager::AccountManagerFacade::Observer` overrides.
  void OnAccountUpserted(const account_manager::Account& account) override;
  void OnAccountRemoved(const account_manager::Account& account) override;

  // |NetworkConnectionTracker::NetworkConnectionObserver| overrides.
  void OnConnectionChanged(network::mojom::ConnectionType type) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(ProfileOAuth2TokenServiceDelegateChromeOSTest,
                           BackOffIsTriggerredForTransientErrors);
  FRIEND_TEST_ALL_PREFIXES(ProfileOAuth2TokenServiceDelegateChromeOSTest,
                           BackOffIsResetOnNetworkChange);

  // A utility class to keep track of |GoogleServiceAuthError|s for an account.
  struct AccountErrorStatus {
    // The last auth error seen.
    GoogleServiceAuthError last_auth_error;
  };

  // Callback handler for `account_manager::AccountManagerFacade::GetAccounts`.
  void OnGetAccounts(const std::vector<account_manager::Account>& accounts);

  void FinishLoadingCredentials(
      const std::vector<account_manager::Account>& accounts,
      const std::map<account_manager::AccountKey, GoogleServiceAuthError>&
          persistent_errors);

  // Callback handler for |AccountManagerFacade::GetPersistentError|.
  void FinishAddingPendingAccount(const account_manager::Account& account,
                                  const GoogleServiceAuthError& error);

  // Non-owning pointers.
  SigninClient* const signin_client_;
  AccountTrackerService* const account_tracker_service_;
  network::NetworkConnectionTracker* const network_connection_tracker_;
  account_manager::AccountManagerFacade* const account_manager_facade_;

  // When the delegate receives an account from either `GetAccounts` or
  // `OnAccountUpserted`, this account is first added to pending accounts, until
  // the persistent error for this account is obtained. When the persistent
  // error status is known, the account is moved from `pending_accounts_` to
  // `account_keys_`.
  std::map<account_manager::AccountKey, account_manager::Account>
      pending_accounts_;

  // A cache of AccountKeys.
  std::set<account_manager::AccountKey> account_keys_;

  // A map from account id to the last seen error for that account.
  std::map<CoreAccountId, AccountErrorStatus> errors_;

  // Used to rate-limit token fetch requests so as to not overload the server.
  net::BackoffEntry backoff_entry_;
  GoogleServiceAuthError backoff_error_;

  // Is |this| attached to a regular (non-Signin && non-LockScreen) Profile.
  const bool is_regular_profile_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<ProfileOAuth2TokenServiceDelegateChromeOS> weak_factory_;
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_PROFILE_OAUTH2_TOKEN_SERVICE_DELEGATE_CHROMEOS_H_
