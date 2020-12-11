// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_PROFILE_OAUTH2_TOKEN_SERVICE_H_
#define COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_PROFILE_OAUTH2_TOKEN_SERVICE_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "build/buildflag.h"
#include "components/signin/internal/identity_manager/profile_oauth2_token_service_observer.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_metrics.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_access_token_manager.h"
#include "net/base/backoff_entry.h"

namespace signin {
class IdentityManager;
}

class PrefService;
class PrefRegistrySimple;
class OAuth2AccessTokenConsumer;
class ProfileOAuth2TokenServiceDelegate;

// ProfileOAuth2TokenService is a KeyedService that retrieves
// OAuth2 access tokens for a given set of scopes using the OAuth2 login
// refresh tokens.
//
// To use this service, call StartRequest() with a given set of scopes and a
// consumer of the request results. The consumer is required to outlive the
// request. The request can be deleted. The consumer may be called back
// asynchronously with the fetch results.
//
// - If the consumer is not called back before the request is deleted, it will
//   never be called back.
//   Note in this case, the actual network requests are not canceled and the
//   cache will be populated with the fetched results; it is just the consumer
//   callback that is aborted.
//
// - Otherwise the consumer will be called back with the request and the fetch
//   results.
//
// The caller of StartRequest() owns the returned request and is responsible to
// delete the request even once the callback has been invoked.
//
// Note: after StartRequest returns, in-flight requests will continue
// even if the TokenService refresh token that was used to initiate
// the request changes or is cleared.  When the request completes,
// Consumer::OnGetTokenSuccess will be invoked, but the access token
// won't be cached.
//
// Note: requests should be started from the UI thread.
class ProfileOAuth2TokenService : public OAuth2AccessTokenManager::Delegate,
                                  public ProfileOAuth2TokenServiceObserver {
 public:
  typedef base::RepeatingCallback<void(const CoreAccountId& /* account_id */,
                                       bool /* is_refresh_token_valid */,
                                       const std::string& /* source */)>
      RefreshTokenAvailableFromSourceCallback;
  typedef base::RepeatingCallback<void(const CoreAccountId& /* account_id */,
                                       const std::string& /* source */)>
      RefreshTokenRevokedFromSourceCallback;

  ProfileOAuth2TokenService(
      PrefService* user_prefs,
      std::unique_ptr<ProfileOAuth2TokenServiceDelegate> delegate);
  ~ProfileOAuth2TokenService() override;

  // Overridden from OAuth2AccessTokenManager::Delegate.
  std::unique_ptr<OAuth2AccessTokenFetcher> CreateAccessTokenFetcher(
      const CoreAccountId& account_id,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      OAuth2AccessTokenConsumer* consumer) override;
  bool HasRefreshToken(const CoreAccountId& account_id) const override;
  bool FixRequestErrorIfPossible() override;
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory()
      const override;
  void OnAccessTokenInvalidated(
      const CoreAccountId& account_id,
      const std::string& client_id,
      const OAuth2AccessTokenManager::ScopeSet& scopes,
      const std::string& access_token) override;
  void OnAccessTokenFetched(const CoreAccountId& account_id,
                            const GoogleServiceAuthError& error) override;

  // Registers per-profile prefs.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  ProfileOAuth2TokenServiceDelegate* GetDelegate();
  const ProfileOAuth2TokenServiceDelegate* GetDelegate() const;

  // Add or remove observers of this token service.
  void AddObserver(ProfileOAuth2TokenServiceObserver* observer);
  void RemoveObserver(ProfileOAuth2TokenServiceObserver* observer);

  // Add or remove observers of access token manager.
  void AddAccessTokenDiagnosticsObserver(
      OAuth2AccessTokenManager::DiagnosticsObserver* observer);
  void RemoveAccessTokenDiagnosticsObserver(
      OAuth2AccessTokenManager::DiagnosticsObserver* observer);

  // Checks in the cache for a valid access token for a specified |account_id|
  // and |scopes|, and if not found starts a request for an OAuth2 access token
  // using the OAuth2 refresh token maintained by this instance for that
  // |account_id|. The caller owns the returned Request.
  // |scopes| is the set of scopes to get an access token for, |consumer| is
  // the object that will be called back with results if the returned request
  // is not deleted.
  std::unique_ptr<OAuth2AccessTokenManager::Request> StartRequest(
      const CoreAccountId& account_id,
      const OAuth2AccessTokenManager::ScopeSet& scopes,
      OAuth2AccessTokenManager::Consumer* consumer);

  // Try to get refresh token from delegate. If it is accessible (i.e. not
  // empty), return it directly, otherwise start request to get access token.
  // Used for getting tokens to send to Gaia Multilogin endpoint.
  std::unique_ptr<OAuth2AccessTokenManager::Request> StartRequestForMultilogin(
      const CoreAccountId& account_id,
      OAuth2AccessTokenManager::Consumer* consumer);

  // This method does the same as |StartRequest| except it uses |client_id| and
  // |client_secret| to identify OAuth client app instead of using
  // Chrome's default values.
  std::unique_ptr<OAuth2AccessTokenManager::Request> StartRequestForClient(
      const CoreAccountId& account_id,
      const std::string& client_id,
      const std::string& client_secret,
      const OAuth2AccessTokenManager::ScopeSet& scopes,
      OAuth2AccessTokenManager::Consumer* consumer);

  // This method does the same as |StartRequest| except it uses the
  // URLLoaderFactory given by |url_loader_factory| instead of using the one
  // returned by Delegate::GetURLLoaderFactory().
  std::unique_ptr<OAuth2AccessTokenManager::Request> StartRequestWithContext(
      const CoreAccountId& account_id,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const OAuth2AccessTokenManager::ScopeSet& scopes,
      OAuth2AccessTokenManager::Consumer* consumer);

  // Mark an OAuth2 |access_token| issued for |account_id| and |scopes| as
  // invalid. This should be done if the token was received from this class,
  // but was not accepted by the server (e.g., the server returned
  // 401 Unauthorized). The token will be removed from the cache for the given
  // scopes.
  void InvalidateAccessToken(const CoreAccountId& account_id,
                             const OAuth2AccessTokenManager::ScopeSet& scopes,
                             const std::string& access_token);

  // Removes token from cache (if it is cached) and calls
  // InvalidateTokenForMultilogin method of the delegate. This should be done if
  // the token was received from this class, but was not accepted by the server
  // (e.g., the server returned 401 Unauthorized).
  virtual void InvalidateTokenForMultilogin(const CoreAccountId& failed_account,
                                            const std::string& token);

  // If set, this callback will be invoked when a new refresh token is
  // available. Contains diagnostic information about the source of the update
  // credentials operation.
  void SetRefreshTokenAvailableFromSourceCallback(
      RefreshTokenAvailableFromSourceCallback callback);

  // If set, this callback will be invoked when a refresh token is revoked.
  // Contains diagnostic information about the source that initiated the
  // revocation operation.
  void SetRefreshTokenRevokedFromSourceCallback(
      RefreshTokenRevokedFromSourceCallback callback);

  void Shutdown();

  // Loads credentials from a backing persistent store to make them available
  // after service is used between profile restarts.
  //
  // The primary account is specified with the |primary_account_id| argument.
  // For a regular profile, the primary account id comes from
  // PrimaryAccountManager.
  // For a supervised user, the id comes from SupervisedUserService.
  void LoadCredentials(const CoreAccountId& primary_account_id);

  // Returns true if LoadCredentials finished with no errors.
  bool HasLoadCredentialsFinishedWithNoErrors();

  // Updates a |refresh_token| for an |account_id|. Credentials are persisted,
  // and available through |LoadCredentials| after service is restarted.
  void UpdateCredentials(
      const CoreAccountId& account_id,
      const std::string& refresh_token,
      signin_metrics::SourceForRefreshTokenOperation source =
          signin_metrics::SourceForRefreshTokenOperation::kUnknown);

  void RevokeCredentials(
      const CoreAccountId& account_id,
      signin_metrics::SourceForRefreshTokenOperation source =
          signin_metrics::SourceForRefreshTokenOperation::kUnknown);

  // Revokes all credentials.
  void RevokeAllCredentials(
      signin_metrics::SourceForRefreshTokenOperation source =
          signin_metrics::SourceForRefreshTokenOperation::kUnknown);

  // Returns a pointer to its instance of net::BackoffEntry or nullptr if there
  // is no such instance.
  const net::BackoffEntry* GetDelegateBackoffEntry();

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  // Removes the credentials associated to account_id from the internal storage,
  // and moves them to |to_service|. The credentials are not revoked on the
  // server, but the OnRefreshTokenRevoked() notification is sent to the
  // observers.
  void ExtractCredentials(ProfileOAuth2TokenService* to_service,
                          const CoreAccountId& account_id);
#endif

  // Returns true iff all credentials have been loaded from disk.
  bool AreAllCredentialsLoaded() const;

  void set_all_credentials_loaded_for_testing(bool loaded) {
    all_credentials_loaded_ = loaded;
  }

  // Lists account IDs of all accounts with a refresh token maintained by this
  // instance.
  // Note: For each account returned by |GetAccounts|, |RefreshTokenIsAvailable|
  // will return true.
  // Note: If tokens have not been fully loaded yet, an empty list is returned.
  std::vector<CoreAccountId> GetAccounts() const;

  // Returns true if a refresh token exists for |account_id|. If false, calls to
  // |StartRequest| will result in a Consumer::OnGetTokenFailure callback.
  // Note: This will return |true| if and only if |account_id| is contained in
  // the list returned by |GetAccounts|.
  bool RefreshTokenIsAvailable(const CoreAccountId& account_id) const;

  // Returns true if a refresh token exists for |account_id| and it is in a
  // persistent error state.
  bool RefreshTokenHasError(const CoreAccountId& account_id) const;

  // Returns the auth error associated with |account_id|. Only persistent errors
  // will be returned.
  GoogleServiceAuthError GetAuthError(const CoreAccountId& account_id) const;

  // Exposes the ability to update auth errors to tests.
  void UpdateAuthErrorForTesting(const CoreAccountId& account_id,
                                 const GoogleServiceAuthError& error);

  void set_max_authorization_token_fetch_retries_for_testing(int max_retries);

  // Override |token_manager_| for testing.
  void OverrideAccessTokenManagerForTesting(
      std::unique_ptr<OAuth2AccessTokenManager> token_manager);

  virtual bool IsFakeProfileOAuth2TokenServiceForTesting() const;

 protected:
  OAuth2AccessTokenManager* GetAccessTokenManager();

 private:
  friend class signin::IdentityManager;

  // ProfileOAuth2TokenServiceObserver implementation.
  void OnRefreshTokenAvailable(const CoreAccountId& account_id) override;
  void OnRefreshTokenRevoked(const CoreAccountId& account_id) override;
  void OnRefreshTokensLoaded() override;

  // Creates a new device ID if there are no accounts, or if the current device
  // ID is empty.
  void RecreateDeviceIdIfNeeded();

  PrefService* user_prefs_;

  std::unique_ptr<ProfileOAuth2TokenServiceDelegate> delegate_;

  // Whether all credentials have been loaded.
  bool all_credentials_loaded_;

  std::unique_ptr<OAuth2AccessTokenManager> token_manager_;

  // Callbacks to invoke, if set, for refresh token-related events.
  RefreshTokenAvailableFromSourceCallback on_refresh_token_available_callback_;
  RefreshTokenRevokedFromSourceCallback on_refresh_token_revoked_callback_;

  signin_metrics::SourceForRefreshTokenOperation update_refresh_token_source_ =
      signin_metrics::SourceForRefreshTokenOperation::kUnknown;

  FRIEND_TEST_ALL_PREFIXES(ProfileOAuth2TokenServiceTest,
                           SameScopesRequestedForDifferentClients);

  DISALLOW_COPY_AND_ASSIGN(ProfileOAuth2TokenService);
};

#endif  // COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_PROFILE_OAUTH2_TOKEN_SERVICE_H_
