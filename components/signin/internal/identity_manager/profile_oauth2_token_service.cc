// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/profile_oauth2_token_service.h"

#include "base/auto_reset.h"
#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/signin/internal/identity_manager/profile_oauth2_token_service_delegate.h"
#include "components/signin/public/base/device_id_helper.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_access_token_consumer.h"
#include "google_apis/gaia/oauth2_access_token_fetcher.h"
#include "google_apis/gaia/oauth2_access_token_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

using signin_metrics::SourceForRefreshTokenOperation;

namespace {
std::string SourceToString(SourceForRefreshTokenOperation source) {
  switch (source) {
    case SourceForRefreshTokenOperation::kUnknown:
      return "Unknown";
    case SourceForRefreshTokenOperation::kTokenService_LoadCredentials:
      return "TokenService::LoadCredentials";
    case SourceForRefreshTokenOperation::kDeprecatedSupervisedUser_InitSync:
      return "DeprecatedSupervisedUser::InitSync";
    case SourceForRefreshTokenOperation::kInlineLoginHandler_Signin:
      return "InlineLoginHandler::Signin";
    case SourceForRefreshTokenOperation::kPrimaryAccountManager_ClearAccount:
      return "PrimaryAccountManager::ClearAccount";
    case SourceForRefreshTokenOperation::
        kPrimaryAccountManager_LegacyPreDiceSigninFlow:
      return "PrimaryAccountManager::LegacyPreDiceSigninFlow";
    case SourceForRefreshTokenOperation::kUserMenu_RemoveAccount:
      return "UserMenu::RemoveAccount";
    case SourceForRefreshTokenOperation::kUserMenu_SignOutAllAccounts:
      return "UserMenu::SignOutAllAccounts";
    case SourceForRefreshTokenOperation::kSettings_Signout:
      return "Settings::Signout";
    case SourceForRefreshTokenOperation::kSettings_PauseSync:
      return "Settings::PauseSync";
    case SourceForRefreshTokenOperation::
        kAccountReconcilor_GaiaCookiesDeletedByUser:
      return "AccountReconcilor::GaiaCookiesDeletedByUser";
    case SourceForRefreshTokenOperation::kAccountReconcilor_GaiaCookiesUpdated:
      return "AccountReconcilor::GaiaCookiesUpdated";
    case SourceForRefreshTokenOperation::kAccountReconcilor_Reconcile:
      return "AccountReconcilor::Reconcile";
    case SourceForRefreshTokenOperation::kDiceResponseHandler_Signin:
      return "DiceResponseHandler::Signin";
    case SourceForRefreshTokenOperation::kDiceResponseHandler_Signout:
      return "DiceResponseHandler::Signout";
    case SourceForRefreshTokenOperation::kDiceTurnOnSyncHelper_Abort:
      return "DiceTurnOnSyncHelper::Abort";
    case SourceForRefreshTokenOperation::kMachineLogon_CredentialProvider:
      return "MachineLogon::CredentialProvider";
    case SourceForRefreshTokenOperation::kTokenService_ExtractCredentials:
      return "TokenService::ExtractCredentials";
    case SourceForRefreshTokenOperation::
        kAccountReconcilor_RevokeTokensNotInCookies:
      return "AccountReconcilor::RevokeTokensNotInCookies";
  }
}
}  // namespace

ProfileOAuth2TokenService::ProfileOAuth2TokenService(
    PrefService* user_prefs,
    std::unique_ptr<ProfileOAuth2TokenServiceDelegate> delegate)
    : user_prefs_(user_prefs),
      delegate_(std::move(delegate)),
      all_credentials_loaded_(false) {
  DCHECK(user_prefs_);
  DCHECK(delegate_);
  token_manager_ = std::make_unique<OAuth2AccessTokenManager>(
      this /* OAuth2AccessTokenManager::Delegate* */);
  AddObserver(this);
}

ProfileOAuth2TokenService::~ProfileOAuth2TokenService() {
  RemoveObserver(this);
}

std::unique_ptr<OAuth2AccessTokenFetcher>
ProfileOAuth2TokenService::CreateAccessTokenFetcher(
    const CoreAccountId& account_id,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    OAuth2AccessTokenConsumer* consumer) {
  return delegate_->CreateAccessTokenFetcher(account_id, url_loader_factory,
                                             consumer);
}

bool ProfileOAuth2TokenService::FixRequestErrorIfPossible() {
  return delegate_->FixRequestErrorIfPossible();
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
}

bool ProfileOAuth2TokenService::HasRefreshToken(
    const CoreAccountId& account_id) const {
  return RefreshTokenIsAvailable(account_id);
}

// static
void ProfileOAuth2TokenService::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
#if defined(OS_IOS)
  registry->RegisterBooleanPref(prefs::kTokenServiceExcludeAllSecondaryAccounts,
                                false);
  registry->RegisterListPref(prefs::kTokenServiceExcludedSecondaryAccounts);
#endif
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
  OAuth2AccessTokenConsumer::TokenResponse token_response(
      refresh_token, base::Time(), std::string());
  // If we can get refresh token from the delegate, inform consumer right away.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
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
  on_refresh_token_available_callback_ = callback;
}

void ProfileOAuth2TokenService::SetRefreshTokenRevokedFromSourceCallback(
    RefreshTokenRevokedFromSourceCallback callback) {
  on_refresh_token_revoked_callback_ = callback;
}

void ProfileOAuth2TokenService::Shutdown() {
  token_manager_->CancelAllRequests();
  GetDelegate()->Shutdown();
}

void ProfileOAuth2TokenService::LoadCredentials(
    const CoreAccountId& primary_account_id) {
  DCHECK_EQ(SourceForRefreshTokenOperation::kUnknown,
            update_refresh_token_source_);
  update_refresh_token_source_ =
      SourceForRefreshTokenOperation::kTokenService_LoadCredentials;
  GetDelegate()->LoadCredentials(primary_account_id);
}

void ProfileOAuth2TokenService::UpdateCredentials(
    const CoreAccountId& account_id,
    const std::string& refresh_token,
    SourceForRefreshTokenOperation source) {
  base::AutoReset<SourceForRefreshTokenOperation> auto_reset(
      &update_refresh_token_source_, source);
  GetDelegate()->UpdateCredentials(account_id, refresh_token);
}

void ProfileOAuth2TokenService::RevokeCredentials(
    const CoreAccountId& account_id,
    SourceForRefreshTokenOperation source) {
  base::AutoReset<SourceForRefreshTokenOperation> auto_reset(
      &update_refresh_token_source_, source);
  GetDelegate()->RevokeCredentials(account_id);
}

void ProfileOAuth2TokenService::RevokeAllCredentials(
    SourceForRefreshTokenOperation source) {
  base::AutoReset<SourceForRefreshTokenOperation> auto_reset(
      &update_refresh_token_source_, source);
  token_manager_->CancelAllRequests();
  token_manager_->ClearCache();
  GetDelegate()->RevokeAllCredentials();
}

const net::BackoffEntry* ProfileOAuth2TokenService::GetDelegateBackoffEntry() {
  return GetDelegate()->BackoffEntry();
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
void ProfileOAuth2TokenService::ExtractCredentials(
    ProfileOAuth2TokenService* to_service,
    const CoreAccountId& account_id) {
  base::AutoReset<SourceForRefreshTokenOperation> auto_reset(
      &update_refresh_token_source_,
      SourceForRefreshTokenOperation::kTokenService_ExtractCredentials);
  GetDelegate()->ExtractCredentials(to_service, account_id);
}
#endif

bool ProfileOAuth2TokenService::AreAllCredentialsLoaded() const {
  return all_credentials_loaded_;
}

std::vector<CoreAccountId> ProfileOAuth2TokenService::GetAccounts() const {
  if (!AreAllCredentialsLoaded())
    return std::vector<CoreAccountId>();

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
  // Check if the newly-updated token is valid (invalid tokens are inserted when
  // the user signs out on the web with DICE enabled).
  bool is_valid = true;
  GoogleServiceAuthError token_error = GetAuthError(account_id);
  if (token_error == GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
                         GoogleServiceAuthError::InvalidGaiaCredentialsReason::
                             CREDENTIALS_REJECTED_BY_CLIENT)) {
    is_valid = false;
  }

  token_manager_->CancelRequestsForAccount(account_id);
  token_manager_->ClearCacheForAccount(account_id);

  signin_metrics::RecordRefreshTokenUpdatedFromSource(
      is_valid, update_refresh_token_source_);

  std::string source_string = SourceToString(update_refresh_token_source_);
  if (on_refresh_token_available_callback_)
    on_refresh_token_available_callback_.Run(account_id, is_valid,
                                             source_string);
}

void ProfileOAuth2TokenService::OnRefreshTokenRevoked(
    const CoreAccountId& account_id) {
  // If this was the last token, recreate the device ID.
  RecreateDeviceIdIfNeeded();

  token_manager_->CancelRequestsForAccount(account_id);
  token_manager_->ClearCacheForAccount(account_id);

  signin_metrics::RecordRefreshTokenRevokedFromSource(
      update_refresh_token_source_);
  std::string source_string = SourceToString(update_refresh_token_source_);
  if (on_refresh_token_revoked_callback_)
    on_refresh_token_revoked_callback_.Run(account_id, source_string);
}

void ProfileOAuth2TokenService::OnRefreshTokensLoaded() {
  all_credentials_loaded_ = true;

  DCHECK_NE(signin::LoadCredentialsState::LOAD_CREDENTIALS_NOT_STARTED,
            GetDelegate()->load_credentials_state());
  DCHECK_NE(signin::LoadCredentialsState::LOAD_CREDENTIALS_IN_PROGRESS,
            GetDelegate()->load_credentials_state());

  // Reset the state for update refresh token operations to Unknown as this
  // was the original state before LoadCredentials was called.
  update_refresh_token_source_ = SourceForRefreshTokenOperation::kUnknown;

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
#if !defined(OS_CHROMEOS)
  if (AreAllCredentialsLoaded() && HasLoadCredentialsFinishedWithNoErrors() &&
      GetAccounts().empty()) {
    signin::RecreateSigninScopedDeviceId(user_prefs_);
  }
#endif
}
