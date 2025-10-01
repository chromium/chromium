// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_MUTABLE_PROFILE_OAUTH2_TOKEN_SERVICE_DELEGATE_H_
#define COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_MUTABLE_PROFILE_OAUTH2_TOKEN_SERVICE_DELEGATE_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/types/optional_ref.h"
#include "components/signin/internal/identity_manager/account_tracker_service.h"
#include "components/signin/internal/identity_manager/profile_oauth2_token_service.h"
#include "components/signin/internal/identity_manager/profile_oauth2_token_service_delegate.h"
#include "components/signin/public/base/account_consistency_method.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/webdata/token_service_table.h"
#include "components/webdata/common/web_data_service_base.h"
#include "components/webdata/common/web_data_service_consumer.h"
#include "crypto/process_bound_string.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/backoff_entry.h"
#include "services/network/public/cpp/network_connection_tracker.h"

class SigninClient;
class TokenWebData;
class TokenBindingHelper;

// This enum is used to know if an account is known by the client has a valid
// refresh token or not.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class AccountStartupState {
  kKnownValidToken = 0,
  kKnownInvalidToken = 1,
  kUnknownValidToken = 2,
  kUnknownInvalidToken = 3,

  kMaxValue = kUnknownInvalidToken,
};

enum class RevokeAllTokensOnLoad {
  kNo = 0,
  kDeleteSiteDataOnExit = 1,
  kExplicitRevoke = 2
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(AccountMoveDecision)
enum class AccountMoveDecision {
  kCanMoveWithRefreshToken = 0,
  kCannotMoveAlreadyExists = 1,
  kCannotMoveInsertWithoutRefreshToken = 2,
  kMaxValue = kCannotMoveInsertWithoutRefreshToken
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/signin/enums.xml:AccountMoveDecision)

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
      RevokeAllTokensOnLoad revoke_all_tokens_on_load,
      std::unique_ptr<TokenBindingHelper> token_binding_helper,
      FixRequestErrorCallback fix_request_error_callback);

  MutableProfileOAuth2TokenServiceDelegate(
      const MutableProfileOAuth2TokenServiceDelegate&) = delete;
  MutableProfileOAuth2TokenServiceDelegate& operator=(
      const MutableProfileOAuth2TokenServiceDelegate&) = delete;

  ~MutableProfileOAuth2TokenServiceDelegate() override;

  // Overridden from ProfileOAuth2TokenServiceDelegate.
  std::unique_ptr<OAuth2AccessTokenFetcher> CreateAccessTokenFetcher(
      const CoreAccountId& account_id,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      OAuth2AccessTokenConsumer* consumer,
      const std::string& token_binding_challenge) override;

  std::string GetTokenForMultilogin(
      const CoreAccountId& account_id) const override;
  bool RefreshTokenIsAvailable(const CoreAccountId& account_id) const override;
  bool IsRefreshTokenBound(const CoreAccountId& account_id) const override;
  std::vector<uint8_t> GetWrappedBindingKey(
      const CoreAccountId& account_id) const override;
  bool AllBoundTokensShareSameBindingKey() const override;
  void GenerateRefreshTokenBindingKeyAssertionForMultilogin(
      const CoreAccountId& account_id,
      std::string_view challenge,
      std::string_view ephemeral_public_key,
      TokenBindingHelper::GenerateAssertionCallback callback) override;
  std::vector<CoreAccountId> GetAccounts() const override;
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory()
      const override;
  void Shutdown() override;

  // Overridden from NetworkConnectionTracker::NetworkConnectionObserver.
  void OnConnectionChanged(network::mojom::ConnectionType type) override;

  bool FixAccountErrorIfPossible() override;

  // Returns the account's refresh token used for testing purposes.
  std::string GetRefreshTokenForTest(const CoreAccountId& account_id) const;

  // The use of the IssueToken endpoint for fetching access tokens is gated by
  // the presence of official Google Chrome API keys.
  // This function removes the official API keys check for testing.
  static void SetIgnoreNonOfficialApiKeysForTesting();

 private:
  friend class MutableProfileOAuth2TokenServiceDelegateTest;

  class RevokeServerRefreshToken;

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
                           RevokeAllCredentialsDuringLoad);
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
  FRIEND_TEST_ALL_PREFIXES(
      MutableProfileOAuth2TokenServiceDelegateTest,
      LoadAllCredentialsIntoMemoryAccountAvailabilityPrimaryAvailable);
  FRIEND_TEST_ALL_PREFIXES(
      MutableProfileOAuth2TokenServiceDelegateTest,
      LoadAllCredentialsIntoMemoryAccountAvailabilityPrimaryNotAvailable);
  FRIEND_TEST_ALL_PREFIXES(
      MutableProfileOAuth2TokenServiceDelegateTest,
      LoadAllCredentialsIntoMemoryAccountAvailabilitySecondaryAvailable);
  FRIEND_TEST_ALL_PREFIXES(
      MutableProfileOAuth2TokenServiceDelegateTest,
      LoadAllCredentialsIntoMemoryAccountAvailabilitySecondaryNotAvailable);
  FRIEND_TEST_ALL_PREFIXES(MutableProfileOAuth2TokenServiceDelegateTest,
                           GetAccounts);
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
  FRIEND_TEST_ALL_PREFIXES(MutableProfileOAuth2TokenServiceDelegateTest,
                           TokenReencryption);
  FRIEND_TEST_ALL_PREFIXES(
      MutableProfileOAuth2TokenServiceDelegateBoundTokensTest,
      UpdateBoundToken);
  FRIEND_TEST_ALL_PREFIXES(
      MutableProfileOAuth2TokenServiceDelegateBoundTokensTest,
      RevokeBoundToken);
  FRIEND_TEST_ALL_PREFIXES(
      MutableProfileOAuth2TokenServiceDelegateBoundTokensTest,
      PersistenceLoadBoundTokens);
  FRIEND_TEST_ALL_PREFIXES(
      MutableProfileOAuth2TokenServiceDelegateBoundTokensTest,
      ClearBoundTokenOnStartup);
  FRIEND_TEST_ALL_PREFIXES(MutableProfileOAuth2TokenServiceDelegateTest,
                           KeepPrimaryAccountTokenOnStartupWithClearOnExit);
  FRIEND_TEST_ALL_PREFIXES(MutableProfileOAuth2TokenServiceDelegateTest,
                           UpdateCredentialsClearsUnreadableTokens);
  FRIEND_TEST_ALL_PREFIXES(
      MutableProfileOAuth2TokenServiceDelegateTest,
      UpdateCredentialsWithNoErrorDoesNotClearUnreadableTokens);
  FRIEND_TEST_ALL_PREFIXES(
      MutableProfileOAuth2TokenServiceDelegateTest,
      UpdateCredentialsBeforeLoadCompletesDoesNotClearUnreadableTokens);

  // WebDataServiceConsumer implementation:
  void OnWebDataServiceRequestDone(
      WebDataServiceBase::Handle handle,
      std::unique_ptr<WDTypedResult> result) override;

  // ProfileOAuth2TokenServiceDelegate implementation:
  void LoadCredentialsInternal(
      const CoreAccountId& primary_account_id) override;
  void UpdateCredentialsInternal(
      const CoreAccountId& account_id,
      const std::string& refresh_token,
      const std::vector<uint8_t>& wrapped_binding_key) override;
  void RevokeAllCredentialsInternal(
      signin_metrics::SourceForRefreshTokenOperation source) override;
  void RevokeCredentialsInternal(const CoreAccountId& account_id) override;
  void ExtractCredentialsInternal(ProfileOAuth2TokenService* to_service,
                                  const CoreAccountId& account_id) override;

  // Loads credentials into in memory structure, and remove any invalid or
  // revoked tokens. If `should_reencrypt` is true then any tokens successfully
  // loaded will be written back to the database to rotate the encryption key.
  void LoadAllCredentialsIntoMemory(
      const std::map<std::string, TokenServiceTable::TokenWithBindingKey>&
          db_tokens,
      bool should_reencrypt = false);

  // Updates the in-memory representation of the credentials.
  void UpdateCredentialsInMemory(
      const CoreAccountId& account_id,
      const std::string& refresh_token,
      const std::vector<uint8_t>& wrapped_binding_key,
      base::optional_ref<const GoogleServiceAuthError> error_for_invalid_token =
          std::nullopt);

  // Sets refresh token in error.
  void InvalidateTokenForMultilogin(
      const CoreAccountId& failed_account) override;

  // Persists credentials for |account_id|. Enables overriding for
  // testing purposes, or other cases, when accessing the DB is not desired.
  void PersistCredentials(const CoreAccountId& account_id,
                          const std::string& refresh_token,
                          const std::vector<uint8_t>& wrapped_binding_key);

  // Clears credentials that have failed to load into memory but are still
  // persisted in the DB.
  void ClearUnreadableCredentials();

  // Clears credentials persisted for |account_id|. Enables overriding for
  // testing purposes, or other cases, when accessing the DB is not desired.
  void ClearPersistedCredentials(const CoreAccountId& account_id);

  // Revokes the refresh token on the server.
  void RevokeCredentialsOnServer(const std::string& refresh_token);

  // Cancels any outstanding fetch for tokens from the web database.
  void CancelWebTokenFetch();

  std::string GetRefreshToken(const CoreAccountId& account_id) const;

  // Called at when tokens are loaded. Performs housekeeping tasks and notifies
  // the observers.
  void FinishLoadingCredentials();

  // Deletes the credential locally and notifies observers through
  // OnRefreshTokenRevoked(). If |revoke_on_server| is true, the token is also
  // revoked on the server.
  void RevokeCredentialsImpl(const CoreAccountId& account_id,
                             bool revoke_on_server);

  // Records whether the `account_id` is available in the account tracker
  // service with a valid `refresh_token` or not. Called at startup.
  void RecordAccountAvailabilityStartup(const CoreAccountId& account_id,
                                        const std::string& refresh_token);

  // In memory refresh token store mapping account_id to refresh_token.
  std::map<CoreAccountId, crypto::ProcessBoundString> refresh_tokens_;

  // Handle to the request reading tokens from database.
  WebDataServiceBase::Handle web_data_service_request_;

  // Flag limiting `ClearUnreadableCredentials()` to take action at most once.
  bool has_cleared_unreadable_credentials_ = false;

  // The primary account id of this service's profile during the loading of
  // credentials.  This member is empty otherwise.
  CoreAccountId loading_primary_account_id_;

  std::vector<std::unique_ptr<RevokeServerRefreshToken>> server_revokes_;

  // Used to verify that certain methods are called only on the thread on which
  // this instance was created.
  THREAD_CHECKER(thread_checker_);

  raw_ptr<SigninClient> client_;
  raw_ptr<AccountTrackerService, DanglingUntriaged> account_tracker_service_;
  raw_ptr<network::NetworkConnectionTracker> network_connection_tracker_;
  scoped_refptr<TokenWebData> token_web_data_;
  signin::AccountConsistencyMethod account_consistency_;

  // Revokes all the tokens after loading them. Secondary accounts will be
  // completely removed, and the primary account will be kept in authentication
  // error state.
  RevokeAllTokensOnLoad revoke_all_tokens_on_load_ = RevokeAllTokensOnLoad::kNo;

  // This is null if token binding is disabled.
  const std::unique_ptr<TokenBindingHelper> token_binding_helper_;

  // Callback function that attempts to correct request errors.  Best effort
  // only.  Returns true if the error was fixed and retry should be reattempted.
  FixRequestErrorCallback fix_request_error_callback_;
};

#endif  // COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_MUTABLE_PROFILE_OAUTH2_TOKEN_SERVICE_DELEGATE_H_
