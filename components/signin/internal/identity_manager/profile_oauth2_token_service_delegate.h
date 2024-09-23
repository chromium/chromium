// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_PROFILE_OAUTH2_TOKEN_SERVICE_DELEGATE_H_
#define COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_PROFILE_OAUTH2_TOKEN_SERVICE_DELEGATE_H_

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "build/build_config.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/load_credentials_state.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_access_token_manager.h"
#include "net/base/backoff_entry.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#endif

namespace network {
class SharedURLLoaderFactory;
}

class OAuth2AccessTokenFetcher;
class OAuth2AccessTokenConsumer;
class ProfileOAuth2TokenServiceObserver;
class ProfileOAuth2TokenService;

// Abstract base class to fetch and maintain refresh tokens from various
// entities. Concrete subclasses should implement RefreshTokenIsAvailable and
// CreateAccessTokenFetcher properly.
class ProfileOAuth2TokenServiceDelegate {
 public:
  // Equivalent typedefs in `ProfileOAuth2TokenService`.
  typedef base::RepeatingCallback<void(const CoreAccountId& /* account_id */,
                                       bool /* is_refresh_token_valid */,
                                       const std::string& /* source */)>
      RefreshTokenAvailableFromSourceCallback;
  typedef base::RepeatingCallback<void(const CoreAccountId& /* account_id */,
                                       const std::string& /* source */)>
      RefreshTokenRevokedFromSourceCallback;

  explicit ProfileOAuth2TokenServiceDelegate(bool use_backoff);

  ProfileOAuth2TokenServiceDelegate(const ProfileOAuth2TokenServiceDelegate&) =
      delete;
  ProfileOAuth2TokenServiceDelegate& operator=(
      const ProfileOAuth2TokenServiceDelegate&) = delete;

  virtual ~ProfileOAuth2TokenServiceDelegate();

  [[nodiscard]] virtual std::unique_ptr<OAuth2AccessTokenFetcher>
  CreateAccessTokenFetcher(
      const CoreAccountId& account_id,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      OAuth2AccessTokenConsumer* consumer,
      const std::string& token_binding_challenge) = 0;

  // Returns |true| if a refresh token is available for |account_id|, and
  // |false| otherwise.
  // Note: Implementations must make sure that |RefreshTokenIsAvailable| returns
  // |true| if and only if |account_id| is contained in the list of accounts
  // returned by |GetAccounts|.
  virtual bool RefreshTokenIsAvailable(
      const CoreAccountId& account_id) const = 0;

  virtual GoogleServiceAuthError GetAuthError(
      const CoreAccountId& account_id) const;
  virtual void UpdateAuthError(const CoreAccountId& account_id,
                               const GoogleServiceAuthError& error,
                               bool fire_auth_error_changed = true);

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  // Returns the wrapped binding key of a refresh token associated with
  // `account_id`, if any.
  // Returns a non-empty vector iff (a) a refresh token exists for `account_id`,
  // and (b) the refresh token is bound to a device.
  virtual std::vector<uint8_t> GetWrappedBindingKey(
      const CoreAccountId& account_id) const = 0;
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)

  // Returns a list of accounts for which a refresh token is maintained by
  // |this| instance, in the order the refresh tokens were added.
  // Note: If tokens have not been fully loaded yet, an empty list is returned.
  // Also, see |RefreshTokenIsAvailable|.
  virtual std::vector<CoreAccountId> GetAccounts() const;

  virtual void OnAccessTokenInvalidated(
      const CoreAccountId& account_id,
      const std::string& client_id,
      const OAuth2AccessTokenManager::ScopeSet& scopes,
      const std::string& access_token) {}

  // If refresh token is accessible (on Desktop) sets error for it to
  // INVALID_GAIA_CREDENTIALS and notifies the observers. Otherwise
  // does nothing.
  virtual void InvalidateTokenForMultilogin(
      const CoreAccountId& failed_account) {}

  virtual void Shutdown() {}

  virtual scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory()
      const;

  // Returns refresh token if the platform allows it (on Desktop) and if it is
  // available and doesn't have error. Otherwise returns empty string (for iOS
  // and Android).
  virtual std::string GetTokenForMultilogin(
      const CoreAccountId& account_id) const;

  bool ValidateAccountId(const CoreAccountId& account_id) const;

  // Add or remove observers of this token service.
  void AddObserver(ProfileOAuth2TokenServiceObserver* observer);
  void RemoveObserver(ProfileOAuth2TokenServiceObserver* observer);

  // Returns true if there is at least one observer.
  bool HasObserver() const;

  // Returns a pointer to its instance of net::BackoffEntry if it has one
  // (`use_backoff` was true in the constructor), or a nullptr otherwise.
  virtual const net::BackoffEntry* BackoffEntry() const;

  // -----------------------------------------------------------------------
  // Methods that are only used by ProfileOAuth2TokenService.
  // -----------------------------------------------------------------------

  // Redirects to `UpdateCredentialsInternal()` which can be overridden by
  // subclasses. Sets the source for the refresh token operation.
  void UpdateCredentials(
      const CoreAccountId& account_id,
      const std::string& refresh_token,
      signin_metrics::SourceForRefreshTokenOperation source =
          signin_metrics::SourceForRefreshTokenOperation::kUnknown
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
      ,
      const std::vector<uint8_t>& wrapped_binding_key = std::vector<uint8_t>()
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  );

  // Redirects to `RevokeCredentialsInternal()` which can be overridden by
  // subclasses. Sets the source for the refresh token operation.
  void RevokeCredentials(
      const CoreAccountId& account_id,
      signin_metrics::SourceForRefreshTokenOperation source =
          signin_metrics::SourceForRefreshTokenOperation::kUnknown);

  // Redirects to `RevokeAllCredentialsInternal()` which can be overridden by
  // subclasses. Sets the source for the refresh token operation.
  void RevokeAllCredentials(
      signin_metrics::SourceForRefreshTokenOperation source =
          signin_metrics::SourceForRefreshTokenOperation::kUnknown);

  // Loads the credentials from disk. Called only once when the token service
  // is initialized. Default implementation is NOTREACHED - subsclasses that
  // are used by the ProfileOAuth2TokenService must provide an implementation
  // for this method.
  // Redirects to `LoadCredentialsInternal()` which can be overridden by
  // subclasses. Sets the source for the refresh token operation.
  void LoadCredentials(const CoreAccountId& primary_account_id,
                       bool is_syncing);

  // Returns the state of the load credentials operation.
  signin::LoadCredentialsState load_credentials_state() const {
    return load_credentials_state_;
  }

  // Removes the credentials associated to account_id from the internal storage,
  // and moves them to |to_service|. The credentials are not revoked on the
  // server, but the OnRefreshTokenRevoked() notification is sent to the
  // observers.
  // Redirects to `ExtractCredentialsInternal()` which can be overridden by
  // subclasses. Sets the source for the refresh token operation.
  void ExtractCredentials(ProfileOAuth2TokenService* to_service,
                          const CoreAccountId& account_id);

  // Attempts to fix account error. This is only possible for some cases where
  // signin happens with a credential provider. See
  // `signin_util::SigninWithCredentialProviderIfPossible()`.
  virtual bool FixAccountErrorIfPossible();

#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)
  // Triggers platform specific implementation to reload accounts from system.
  virtual void ReloadAllAccountsFromSystemWithPrimaryAccount(
      const std::optional<CoreAccountId>& primary_account_id) {}
#endif

#if BUILDFLAG(IS_IOS)
  // Triggers platform specific implementation for iOS to add a given account
  // to the token service from a system account.
  virtual void ReloadAccountFromSystem(const CoreAccountId& account_id) {}
#endif

#if BUILDFLAG(IS_ANDROID)
  // Triggers platform specific implementation to reload accounts from system.
  virtual void SeedAccountsThenReloadAllAccountsWithPrimaryAccount(
      const std::vector<CoreAccountInfo>& core_account_infos,
      const std::optional<CoreAccountId>& primary_account_id) {}

  // Returns a reference to the corresponding Java object.
  virtual base::android::ScopedJavaLocalRef<jobject> GetJavaObject() = 0;
#endif

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

  // This callback will be invoked when a refresh token is revoked and observers
  // have been notified.
  void SetOnRefreshTokenRevokedNotified(
      base::RepeatingCallback<void(const CoreAccountId&)> callback);

  // -----------------------------------------------------------------------
  // End of methods that are only used by ProfileOAuth2TokenService
  // -----------------------------------------------------------------------

 protected:
  void set_load_credentials_state(signin::LoadCredentialsState state) {
    load_credentials_state_ = state;
  }

  virtual void ClearAuthError(const std::optional<CoreAccountId>& account_id);
  virtual GoogleServiceAuthError BackOffError() const;
  // Can be called only if `use_backoff` was true in the constructor.
  virtual void ResetBackOffEntry();

  // Called by subclasses to notify observers.
  void FireEndBatchChanges();
  void FireRefreshTokenAvailable(const CoreAccountId& account_id);
  void FireRefreshTokenRevoked(const CoreAccountId& account_id);
  // FireRefreshTokensLoaded is virtual and overridden in android implementation
  // where additional actions are required.
  virtual void FireRefreshTokensLoaded();
  void FireAuthErrorChanged(const CoreAccountId& account_id,
                            const GoogleServiceAuthError& error);

  // Helper class to scope batch changes.
  class ScopedBatchChange {
   public:
    explicit ScopedBatchChange(ProfileOAuth2TokenServiceDelegate* delegate);

    ScopedBatchChange(const ScopedBatchChange&) = delete;
    ScopedBatchChange& operator=(const ScopedBatchChange&) = delete;

    ~ScopedBatchChange();

   private:
    raw_ptr<ProfileOAuth2TokenServiceDelegate> delegate_;  // Weak.
  };

 private:
  FRIEND_TEST_ALL_PREFIXES(ProfileOAuth2TokenServiceDelegateTest,
                           FireRefreshTokenRevoked);
  FRIEND_TEST_ALL_PREFIXES(MutableProfileOAuth2TokenServiceDelegateTest,
                           RetryBackoff);
  FRIEND_TEST_ALL_PREFIXES(ProfileOAuth2TokenServiceDelegateChromeOSTest,
                           BackOffIsTriggerredForTransientErrors);
  FRIEND_TEST_ALL_PREFIXES(ProfileOAuth2TokenServiceDelegateTest,
                           UpdateAuthErrorTransientErrors);

  // Internal implementations of the methods that can be overridden by
  // subclasses.

  virtual void LoadCredentialsInternal(const CoreAccountId& primary_account_id,
                                       bool is_syncing) = 0;

  virtual void UpdateCredentialsInternal(
      const CoreAccountId& account_id,
      const std::string& refresh_token
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
      ,
      const std::vector<uint8_t>& wrapped_binding_key
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  ) {
  }

  virtual void RevokeCredentialsInternal(const CoreAccountId& account_id) {}

  virtual void RevokeAllCredentialsInternal(
      signin_metrics::SourceForRefreshTokenOperation source) {}

  virtual void ExtractCredentialsInternal(ProfileOAuth2TokenService* to_service,
                                          const CoreAccountId& account_id);

  // List of observers to notify when refresh token availability changes.
  // Makes sure list is empty on destruction.
  base::ObserverList<ProfileOAuth2TokenServiceObserver, true> observer_list_;

  // The state of the load credentials operation.
  signin::LoadCredentialsState load_credentials_state_ =
      signin::LoadCredentialsState::LOAD_CREDENTIALS_NOT_STARTED;

  void StartBatchChanges();
  void EndBatchChanges();

  // The depth of batch changes.
  int batch_change_depth_;

  // If the error is transient, back off is used on some platforms to rate-limit
  // network token requests so as to not overload the server. |backoff_entry_|
  // is initialized only if `use_backoff` was true in the constructor.
  std::unique_ptr<net::BackoffEntry> backoff_entry_;
  GoogleServiceAuthError backoff_error_;

  // A map from account id to the last seen error for that account.
  std::map<CoreAccountId, GoogleServiceAuthError> errors_;

  // Callbacks to invoke, if set, for refresh token-related events.
  RefreshTokenAvailableFromSourceCallback on_refresh_token_available_callback_;
  RefreshTokenRevokedFromSourceCallback on_refresh_token_revoked_callback_;
  base::RepeatingCallback<void(const CoreAccountId&)>
      on_refresh_token_revoked_notified_callback_;

  signin_metrics::SourceForRefreshTokenOperation update_refresh_token_source_ =
      signin_metrics::SourceForRefreshTokenOperation::kUnknown;
};

#endif  // COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_PROFILE_OAUTH2_TOKEN_SERVICE_DELEGATE_H_
