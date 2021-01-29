// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_MUTABLE_PROFILE_OAUTH2_TOKEN_SERVICE_DELEGATE_H_
#define COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_MUTABLE_PROFILE_OAUTH2_TOKEN_SERVICE_DELEGATE_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "components/signin/internal/identity_manager/account_tracker_service.h"
#include "components/signin/internal/identity_manager/profile_oauth2_token_service.h"
#include "components/signin/internal/identity_manager/profile_oauth2_token_service_delegate.h"
#include "components/signin/public/base/account_consistency_method.h"
#include "components/webdata/common/web_data_service_base.h"
#include "components/webdata/common/web_data_service_consumer.h"
#include "net/base/backoff_entry.h"
#include "services/network/public/cpp/network_connection_tracker.h"

class PrefRegistrySimple;
class SigninClient;
class TokenWebData;

class MutableProfileOAuth2TokenServiceDelegate
    : public ProfileOAuth2TokenServiceDelegate,
      public WebDataServiceConsumer,
      public network::NetworkConnectionTracker::NetworkConnectionObserver {
 public:
  using FixRequestErrorCallback = base::RepeatingCallback<bool()>;

  MutableProfileOAuth2TokenServiceDelegate(
      SigninClient* client,
      AccountTrackerService* account_tracker_service,
      network::NetworkConnectionTracker* network_connection_tracker,
      scoped_refptr<TokenWebData> token_web_data,
      signin::AccountConsistencyMethod account_consistency,
      bool revoke_all_tokens_on_load,
      FixRequestErrorCallback fix_request_error_callback);
  ~MutableProfileOAuth2TokenServiceDelegate() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Overridden from ProfileOAuth2TokenServiceDelegate.
  std::unique_ptr<OAuth2AccessTokenFetcher> CreateAccessTokenFetcher(
      const CoreAccountId& account_id,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      OAuth2AccessTokenConsumer* consumer) override;

  // Updates the internal cache of the result from the most-recently-completed
  // auth request (used for reporting errors to the user).
  void UpdateAuthError(const CoreAccountId& account_id,
                       const GoogleServiceAuthError& error) override;

  std::string GetTokenForMultilogin(
      const CoreAccountId& account_id) const override;
  bool RefreshTokenIsAvailable(const CoreAccountId& account_id) const override;
  GoogleServiceAuthError GetAuthError(
      const CoreAccountId& account_id) const override;
  std::vector<CoreAccountId> GetAccounts() const override;
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory()
      const override;
  void LoadCredentials(const CoreAccountId& primary_account_id) override;
  void UpdateCredentials(const CoreAccountId& account_id,
                         const std::string& refresh_token) override;
  void RevokeAllCredentials() override;
  void RevokeCredentials(const CoreAccountId& account_id) override;
  void ExtractCredentials(ProfileOAuth2TokenService* to_service,
                          const CoreAccountId& account_id) override;
  void Shutdown() override;

  // Overridden from NetworkConnectionTracker::NetworkConnectionObserver.
  void OnConnectionChanged(network::mojom::ConnectionType type) override;

  // Overridden from ProfileOAuth2TokenServiceDelegate.
  const net::BackoffEntry* BackoffEntry() const override;

  bool FixRequestErrorIfPossible() override;

  // Returns the account's refresh token used for testing purposes.
  std::string GetRefreshTokenForTest(const CoreAccountId& account_id) const;

 private:
  friend class MutableProfileOAuth2TokenServiceDelegateTest;

  class RevokeServerRefreshToken;

  struct AccountStatus {
    std::string refresh_token;
    GoogleServiceAuthError last_auth_error;
  };

  FRIEND_TEST_ALL_PREFIXES(MutableProfileOAuth2TokenServiceDelegateTest,
                           PersistenceDBUpgrade);
  FRIEND_TEST_ALL_PREFIXES(MutableProfileOAuth2TokenServiceDelegateTest,
                           FetchPersistentError);
  FRIEND_TEST_ALL_PREFIXES(
      MutableProfileOAuth2TokenServiceDelegateTest,
      PersistenceLoadCredentialsEmptyPrimaryAccountId_DiceEnabled);
  FRIEND_TEST_ALL_PREFIXES(
      MutableProfileOAuth2TokenServiceDelegateTest,
      LoadCredentialsClearsTokenDBWhenNoPrimaryAccount_DiceDisabled);
  FRIEND_TEST_ALL_PREFIXES(MutableProfileOAuth2TokenServiceDelegateTest,
                           PersistenceLoadCredentials);
  FRIEND_TEST_ALL_PREFIXES(MutableProfileOAuth2TokenServiceDelegateTest,
                           RevokeOnUpdate);
  FRIEND_TEST_ALL_PREFIXES(MutableProfileOAuth2TokenServiceDelegateTest,
                           DelayedRevoke);
  FRIEND_TEST_ALL_PREFIXES(MutableProfileOAuth2TokenServiceDelegateTest,
                           DiceMigrationWithMissingHostedDomain);
  FRIEND_TEST_ALL_PREFIXES(MutableProfileOAuth2TokenServiceDelegateTest,
                           DiceMigrationHostedDomainPrimaryAccount);
  FRIEND_TEST_ALL_PREFIXES(MutableProfileOAuth2TokenServiceDelegateTest,
                           ShutdownDuringRevoke);
  FRIEND_TEST_ALL_PREFIXES(MutableProfileOAuth2TokenServiceDelegateTest,
                           RevokeRetries);
  FRIEND_TEST_ALL_PREFIXES(MutableProfileOAuth2TokenServiceDelegateTest,
                           UpdateInvalidToken);
  FRIEND_TEST_ALL_PREFIXES(MutableProfileOAuth2TokenServiceDelegateTest,
                           LoadInvalidToken);
  FRIEND_TEST_ALL_PREFIXES(MutableProfileOAuth2TokenServiceDelegateTest,
                           GetAccounts);
  FRIEND_TEST_ALL_PREFIXES(MutableProfileOAuth2TokenServiceDelegateTest,
                           RetryBackoff);
  FRIEND_TEST_ALL_PREFIXES(MutableProfileOAuth2TokenServiceDelegateTest,
                           CanonicalizeAccountId);
  FRIEND_TEST_ALL_PREFIXES(MutableProfileOAuth2TokenServiceDelegateTest,
                           CanonAndNonCanonAccountId);
  FRIEND_TEST_ALL_PREFIXES(MutableProfileOAuth2TokenServiceDelegateTest,
                           ShutdownService);
  FRIEND_TEST_ALL_PREFIXES(MutableProfileOAuth2TokenServiceDelegateTest,
                           ClearTokensOnStartup);
  FRIEND_TEST_ALL_PREFIXES(MutableProfileOAuth2TokenServiceDelegateTest,
                           InvalidateTokensForMultilogin);
  FRIEND_TEST_ALL_PREFIXES(MutableProfileOAuth2TokenServiceDelegateTest,
                           ExtractCredentials);

  // WebDataServiceConsumer implementation:
  void OnWebDataServiceRequestDone(
      WebDataServiceBase::Handle handle,
      std::unique_ptr<WDTypedResult> result) override;

  // Loads credentials into in memory stucture.
  void LoadAllCredentialsIntoMemory(
      const std::map<std::string, std::string>& db_tokens);

  // Updates the in-memory representation of the credentials.
  void UpdateCredentialsInMemory(const CoreAccountId& account_id,
                                 const std::string& refresh_token);

  // Sets refresh token in error.
  void InvalidateTokenForMultilogin(
      const CoreAccountId& failed_account) override;

  // Persists credentials for |account_id|. Enables overriding for
  // testing purposes, or other cases, when accessing the DB is not desired.
  void PersistCredentials(const CoreAccountId& account_id,
                          const std::string& refresh_token);

  // Clears credentials persisted for |account_id|. Enables overriding for
  // testing purposes, or other cases, when accessing the DB is not desired.
  void ClearPersistedCredentials(const CoreAccountId& account_id);

  // Revokes the refresh token on the server.
  void RevokeCredentialsOnServer(const std::string& refresh_token);

  // Cancels any outstanding fetch for tokens from the web database.
  void CancelWebTokenFetch();

  std::string GetRefreshToken(const CoreAccountId& account_id) const;

  // Creates a new AccountStatus and adds it to the AccountStatusMap.
  // The account must not be already in the map.
  void AddAccountStatus(const CoreAccountId& account_id,
                        const std::string& refresh_token,
                        const GoogleServiceAuthError& error);

  // Called at when tokens are loaded. Performs housekeeping tasks and notifies
  // the observers.
  void FinishLoadingCredentials();

  // Deletes the credential locally and notifies observers through
  // OnRefreshTokenRevoked(). If |revoke_on_server| is true, the token is also
  // revoked on the server.
  void RevokeCredentialsImpl(const CoreAccountId& account_id,
                             bool revoke_on_server);

  // If the Dice migration happened before the tokens could be migrated, delete
  // all the tokens. This is only called if the tokens could not be loaded
  // successfully.
  void MaybeDeletePreDiceTokens();

  // Maps the |account_id| of accounts known to ProfileOAuth2TokenService
  // to information about the account.
  typedef std::map<CoreAccountId, AccountStatus> AccountStatusMap;
  // In memory refresh token store mapping account_id to refresh_token.
  AccountStatusMap refresh_tokens_;

  // Handle to the request reading tokens from database.
  WebDataServiceBase::Handle web_data_service_request_;

  // The primary account id of this service's profile during the loading of
  // credentials.  This member is empty otherwise.
  CoreAccountId loading_primary_account_id_;

  std::vector<std::unique_ptr<RevokeServerRefreshToken>> server_revokes_;

  // Used to verify that certain methods are called only on the thread on which
  // this instance was created.
  THREAD_CHECKER(thread_checker_);

  // Used to rate-limit network token requests so as to not overload the server.
  net::BackoffEntry::Policy backoff_policy_;
  net::BackoffEntry backoff_entry_;
  GoogleServiceAuthError backoff_error_;

  SigninClient* client_;
  AccountTrackerService* account_tracker_service_;
  network::NetworkConnectionTracker* network_connection_tracker_;
  scoped_refptr<TokenWebData> token_web_data_;
  signin::AccountConsistencyMethod account_consistency_;

  // Revokes all the tokens after loading them. Secondary accounts will be
  // completely removed, and the primary account will be kept in authentication
  // error state.
  const bool revoke_all_tokens_on_load_;

  // Callback function that attempts to correct request errors.  Best effort
  // only.  Returns true if the error was fixed and retry should be reattempted.
  FixRequestErrorCallback fix_request_error_callback_;

  DISALLOW_COPY_AND_ASSIGN(MutableProfileOAuth2TokenServiceDelegate);
};

#endif  // COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_MUTABLE_PROFILE_OAUTH2_TOKEN_SERVICE_DELEGATE_H_
