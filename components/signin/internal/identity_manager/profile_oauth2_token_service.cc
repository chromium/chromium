// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/profile_oauth2_token_service.h"

#include "base/auto_reset.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/signin/internal/identity_manager/profile_oauth2_token_service_delegate.h"
#include "components/signin/public/base/device_id_helper.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_access_token_consumer.h"
#include "google_apis/gaia/oauth2_access_token_fetcher.h"
#include "google_apis/gaia/oauth2_access_token_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace {

using TokenResponseBuilder = OAuth2AccessTokenConsumer::TokenResponse::Builder;

}  // namespace

ProfileOAuth2TokenService::ProfileOAuth2TokenService(
    PrefService* user_prefs,
    std::unique_ptr<ProfileOAuth2TokenServiceDelegate> delegate)
    : user_prefs_(user_prefs),
      delegate_(std::move(delegate)),
      all_credentials_loaded_(false) {
  DCHECK(user_prefs_);
  DCHECK(delegate_);
  token_manager_ =
      std::make_unique<OAuth2AccessTokenManager>(/*delegate=*/this);
  // The `ProfileOAuth2TokenService` must be the first observer of `delegate_`.
  DCHECK(!delegate_->HasObserver());
  // `base::Unretained(this)` is safe as `this` owns `delegate_`.
  delegate_->SetOnRefreshTokenRevokedNotified(base::BindRepeating(
      &ProfileOAuth2TokenService::OnRefreshTokenRevokedNotified,
      base::Unretained(this)));
  token_service_observation_.Observe(delegate_.get());
  DCHECK(delegate_->HasObserver());
}

ProfileOAuth2TokenService::~ProfileOAuth2TokenService() {
  token_manager_.reset();
  GetDelegate()->Shutdown();
}

std::unique_ptr<OAuth2AccessTokenFetcher>
ProfileOAuth2TokenService::CreateAccessTokenFetcher(
    const CoreAccountId& account_id,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    OAuth2AccessTokenConsumer* consumer,
    const std::string& token_binding_challenge) {
  return delegate_->CreateAccessTokenFetcher(account_id, url_loader_factory,
                                             consumer, token_binding_challenge);
}

void ProfileOAuth2TokenService::FixAccountErrorIfPossible() {
  delegate_->FixAccountErrorIfPossible();
}

scoped_refptr<network::SharedURLLoaderFactory>
ProfileOAuth2TokenService::GetURLLoaderFactory() const {
  return delegate_->GetURLLoaderFactory();
}

void ProfileOAuth2TokenService::OnAccessTokenInvalidated(
    const CoreAccountId& account_id,
    const std::string& client_id,
    const OAuth2AccessTokenManager::ScopeSet& scopes,
    const std::string& access_token) {
  delegate_->OnAccessTokenInvalidated(account_id, client_id, scopes,
                                      access_token);
}

void ProfileOAuth2TokenService::OnAccessTokenFetched(
    const CoreAccountId& account_id,
    const GoogleServiceAuthError& error) {
  // Update the auth error state so auth errors are appropriately communicated
  // to the user.
  delegate_->UpdateAuthError(account_id, error);
  if (error.IsPersistentError()) {
    // Needed for Enterprise on Windows to allow
    // `signin_util::ReauthWithCredentialProviderIfPossible()` to fix the
    // account.
    FixAccountErrorIfPossible();
  }
}

bool ProfileOAuth2TokenService::HasRefreshToken(
    const CoreAccountId& account_id) const {
  return RefreshTokenIsAvailable(account_id);
}

// static
void ProfileOAuth2TokenService::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kGoogleServicesSigninScopedDeviceId,
                               std::string());
}

ProfileOAuth2TokenServiceDelegate* ProfileOAuth2TokenService::GetDelegate() {
  return delegate_.get();
}

const ProfileOAuth2TokenServiceDelegate*
ProfileOAuth2TokenService::GetDelegate() const {
  return delegate_.get();
}

void ProfileOAuth2TokenService::AddObserver(
    ProfileOAuth2TokenServiceObserver* observer) {
  delegate_->AddObserver(observer);
}

void ProfileOAuth2TokenService::RemoveObserver(
    ProfileOAuth2TokenServiceObserver* observer) {
  delegate_->RemoveObserver(observer);
}

void ProfileOAuth2TokenService::AddAccessTokenDiagnosticsObserver(
    OAuth2AccessTokenManager::DiagnosticsObserver* observer) {
  token_manager_->AddDiagnosticsObserver(observer);
}

void ProfileOAuth2TokenService::RemoveAccessTokenDiagnosticsObserver(
    OAuth2AccessTokenManager::DiagnosticsObserver* observer) {
  token_manager_->RemoveDiagnosticsObserver(observer);
}

std::unique_ptr<OAuth2AccessTokenManager::Request>
ProfileOAuth2TokenService::StartRequest(
    const CoreAccountId& account_id,
    const OAuth2AccessTokenManager::ScopeSet& scopes,
    OAuth2AccessTokenManager::Consumer* consumer) {
  return token_manager_->StartRequest(account_id, scopes, consumer);
}

std::unique_ptr<OAuth2AccessTokenManager::Request>
ProfileOAuth2TokenService::StartRequestForMultilogin(
    const CoreAccountId& account_id,
    OAuth2AccessTokenManager::Consumer* consumer) {
  const std::string refresh_token =
      delegate_->GetTokenForMultilogin(account_id);
  if (refresh_token.empty()) {
    // If we can't get refresh token from the delegate, start request for access
    // token.
    OAuth2AccessTokenManager::ScopeSet scopes;
    scopes.insert(GaiaConstants::kOAuth1LoginScope);
    return token_manager_->StartRequest(account_id, scopes, consumer);
  }
  std::unique_ptr<OAuth2AccessTokenManager::RequestImpl> request(
      new OAuth2AccessTokenManager::RequestImpl(account_id, consumer));
  // Create token response from token. Expiration time and id token do not
  // matter and should not be accessed.
  // TODO(crbug.com/40158125): See bug description for why the refresh token is
  // passed in the access token field.
  OAuth2AccessTokenConsumer::TokenResponse token_response =
      TokenResponseBuilder().WithAccessToken(refresh_token).build();
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&OAuth2AccessTokenManager::RequestImpl::InformConsumer,
                     request.get()->AsWeakPtr(),
                     GoogleServiceAuthError(GoogleServiceAuthError::NONE),
                     token_response));
  return std::move(request);
}

std::unique_ptr<OAuth2AccessTokenManager::Request>
ProfileOAuth2TokenService::StartRequestForClient(
    const CoreAccountId& account_id,
    const std::string& client_id,
    const std::string& client_secret,
    const OAuth2AccessTokenManager::ScopeSet& scopes,
    OAuth2AccessTokenManager::Consumer* consumer) {
  return token_manager_->StartRequestForClient(account_id, client_id,
                                               client_secret, scopes, consumer);
}

std::unique_ptr<OAuth2AccessTokenManager::Request>
ProfileOAuth2TokenService::StartRequestWithContext(
    const CoreAccountId& account_id,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const OAuth2AccessTokenManager::ScopeSet& scopes,
    OAuth2AccessTokenManager::Consumer* consumer) {
  return token_manager_->StartRequestWithContext(account_id, url_loader_factory,
                                                 scopes, consumer);
}

void ProfileOAuth2TokenService::InvalidateAccessToken(
    const CoreAccountId& account_id,
    const OAuth2AccessTokenManager::ScopeSet& scopes,
    const std::string& access_token) {
  token_manager_->InvalidateAccessToken(account_id, scopes, access_token);
}

void ProfileOAuth2TokenService::InvalidateTokenForMultilogin(
    const CoreAccountId& failed_account,
    const std::string& token) {
  OAuth2AccessTokenManager::ScopeSet scopes;
  scopes.insert(GaiaConstants::kOAuth1LoginScope);
  // Remove from cache. This will have no effect on desktop since token is a
  // refresh token and is not in cache.
  InvalidateAccessToken(failed_account, scopes, token);
  // For desktop refresh tokens can be invalidated directly in delegate. This
  // will have no effect on mobile.
  delegate_->InvalidateTokenForMultilogin(failed_account);
}

void ProfileOAuth2TokenService::SetRefreshTokenAvailableFromSourceCallback(
    RefreshTokenAvailableFromSourceCallback callback) {
  GetDelegate()->SetRefreshTokenAvailableFromSourceCallback(callback);
}

void ProfileOAuth2TokenService::SetRefreshTokenRevokedFromSourceCallback(
    RefreshTokenRevokedFromSourceCallback callback) {
  GetDelegate()->SetRefreshTokenRevokedFromSourceCallback(callback);
}

void ProfileOAuth2TokenService::LoadCredentials(
    const CoreAccountId& primary_account_id,
    bool is_syncing) {
  GetDelegate()->LoadCredentials(primary_account_id, is_syncing);
}

void ProfileOAuth2TokenService::UpdateCredentials(
    const CoreAccountId& account_id,
    const std::string& refresh_token,
    signin_metrics::SourceForRefreshTokenOperation source
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
    ,
    const std::vector<uint8_t>& wrapped_binding_key
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
) {
  GetDelegate()->UpdateCredentials(account_id, refresh_token, source
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
                                   ,
                                   wrapped_binding_key
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  );
}

void ProfileOAuth2TokenService::RevokeCredentials(
    const CoreAccountId& account_id,
    signin_metrics::SourceForRefreshTokenOperation source) {
  GetDelegate()->RevokeCredentials(account_id, source);
}

void ProfileOAuth2TokenService::RevokeAllCredentials(
    signin_metrics::SourceForRefreshTokenOperation source) {
  GetDelegate()->RevokeAllCredentials(source);
}

const net::BackoffEntry* ProfileOAuth2TokenService::GetDelegateBackoffEntry() {
  return GetDelegate()->BackoffEntry();
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
void ProfileOAuth2TokenService::ExtractCredentials(
    ProfileOAuth2TokenService* to_service,
    const CoreAccountId& account_id) {
  GetDelegate()->ExtractCredentials(to_service, account_id);
}
#endif

bool ProfileOAuth2TokenService::AreAllCredentialsLoaded() const {
  return all_credentials_loaded_;
}

std::vector<CoreAccountId> ProfileOAuth2TokenService::GetAccounts() const {
  if (!AreAllCredentialsLoaded()) {
    return std::vector<CoreAccountId>();
  }

  return GetDelegate()->GetAccounts();
}

bool ProfileOAuth2TokenService::RefreshTokenIsAvailable(
    const CoreAccountId& account_id) const {
  return delegate_->RefreshTokenIsAvailable(account_id);
}

bool ProfileOAuth2TokenService::RefreshTokenHasError(
    const CoreAccountId& account_id) const {
  return GetAuthError(account_id) != GoogleServiceAuthError::AuthErrorNone();
}

GoogleServiceAuthError ProfileOAuth2TokenService::GetAuthError(
    const CoreAccountId& account_id) const {
  GoogleServiceAuthError error = delegate_->GetAuthError(account_id);
  DCHECK(!error.IsTransientError());
  return error;
}

void ProfileOAuth2TokenService::UpdateAuthErrorForTesting(
    const CoreAccountId& account_id,
    const GoogleServiceAuthError& error) {
  GetDelegate()->UpdateAuthError(account_id, error);
}

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
std::vector<uint8_t> ProfileOAuth2TokenService::GetWrappedBindingKey(
    const CoreAccountId& account_id) const {
  return delegate_->GetWrappedBindingKey(account_id);
}
#endif

void ProfileOAuth2TokenService::
    set_max_authorization_token_fetch_retries_for_testing(int max_retries) {
  token_manager_->set_max_authorization_token_fetch_retries_for_testing(
      max_retries);
}

void ProfileOAuth2TokenService::OverrideAccessTokenManagerForTesting(
    std::unique_ptr<OAuth2AccessTokenManager> token_manager) {
  token_manager_ = std::move(token_manager);
}

bool ProfileOAuth2TokenService::IsFakeProfileOAuth2TokenServiceForTesting()
    const {
  return false;
}

OAuth2AccessTokenManager* ProfileOAuth2TokenService::GetAccessTokenManager() {
  return token_manager_.get();
}

void ProfileOAuth2TokenService::OnRefreshTokenAvailable(
    const CoreAccountId& account_id) {
  token_manager_->CancelRequestsForAccount(
      account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED));
  token_manager_->ClearCacheForAccount(account_id);
}

void ProfileOAuth2TokenService::OnRefreshTokenRevoked(
    const CoreAccountId& account_id) {
  // If this was the last token, recreate the device ID.
  RecreateDeviceIdIfNeeded();

  token_manager_->ClearCacheForAccount(account_id);
}

void ProfileOAuth2TokenService::OnRefreshTokenRevokedNotified(
    const CoreAccountId& account_id) {
  token_manager_->CancelRequestsForAccount(
      account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::USER_NOT_SIGNED_UP));
}

void ProfileOAuth2TokenService::OnRefreshTokensLoaded() {
  all_credentials_loaded_ = true;

  DCHECK_NE(signin::LoadCredentialsState::LOAD_CREDENTIALS_NOT_STARTED,
            GetDelegate()->load_credentials_state());
  DCHECK_NE(signin::LoadCredentialsState::LOAD_CREDENTIALS_IN_PROGRESS,
            GetDelegate()->load_credentials_state());

  // Ensure the device ID is not empty, and recreate it if all tokens were
  // cleared during the loading process.
  RecreateDeviceIdIfNeeded();
}

bool ProfileOAuth2TokenService::HasLoadCredentialsFinishedWithNoErrors() {
  switch (GetDelegate()->load_credentials_state()) {
    case signin::LoadCredentialsState::LOAD_CREDENTIALS_NOT_STARTED:
    case signin::LoadCredentialsState::LOAD_CREDENTIALS_IN_PROGRESS:
      // LoadCredentials has not finished.
      return false;
    case signin::LoadCredentialsState::
        LOAD_CREDENTIALS_FINISHED_WITH_DB_CANNOT_BE_OPENED:
    case signin::LoadCredentialsState::LOAD_CREDENTIALS_FINISHED_WITH_DB_ERRORS:
    case signin::LoadCredentialsState::
        LOAD_CREDENTIALS_FINISHED_WITH_DECRYPT_ERRORS:
    case signin::LoadCredentialsState::
        LOAD_CREDENTIALS_FINISHED_WITH_UNKNOWN_ERRORS:
      // LoadCredentials finished, but with errors
      return false;
    case signin::LoadCredentialsState::LOAD_CREDENTIALS_FINISHED_WITH_SUCCESS:
    case signin::LoadCredentialsState::
        LOAD_CREDENTIALS_FINISHED_WITH_NO_TOKEN_FOR_PRIMARY_ACCOUNT:
      // Load credentials finished with success.
      return true;
  }
}

void ProfileOAuth2TokenService::RecreateDeviceIdIfNeeded() {
// On ChromeOS the device ID is not managed by the token service.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  if (AreAllCredentialsLoaded() && HasLoadCredentialsFinishedWithNoErrors() &&
      GetAccounts().empty()) {
    signin::RecreateSigninScopedDeviceId(user_prefs_);
  }
#endif
}
