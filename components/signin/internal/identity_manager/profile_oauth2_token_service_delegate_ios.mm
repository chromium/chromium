// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/profile_oauth2_token_service_delegate_ios.h"

#import <Foundation/Foundation.h>

#include <set>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/values.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/signin/internal/identity_manager/account_tracker_service.h"
#include "components/signin/public/base/signin_client.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/ios/device_accounts_provider.h"
#include "google_apis/gaia/oauth2_access_token_fetcher.h"
#include "net/url_request/url_request_status.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Match the way Chromium handles authentication errors in
// google_apis/gaia/oauth2_access_token_fetcher.cc:
GoogleServiceAuthError GetGoogleServiceAuthErrorFromNSError(
    DeviceAccountsProvider* provider,
    const std::string& gaia_id,
    NSError* error) {
  if (!error)
    return GoogleServiceAuthError::AuthErrorNone();

  AuthenticationErrorCategory errorCategory =
      provider->GetAuthenticationErrorCategory(gaia_id, error);
  switch (errorCategory) {
    case kAuthenticationErrorCategoryUnknownErrors:
      // Treat all unknown error as unexpected service response errors.
      // This may be too general and may require a finer grain filtering.
      return GoogleServiceAuthError(
          GoogleServiceAuthError::UNEXPECTED_SERVICE_RESPONSE);
    case kAuthenticationErrorCategoryAuthorizationErrors:
      return GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::
              CREDENTIALS_REJECTED_BY_SERVER);
    case kAuthenticationErrorCategoryAuthorizationForbiddenErrors:
      // HTTP_FORBIDDEN (403) is treated as temporary error, because it may be
      // '403 Rate Limit Exceeded.' (for more details, see
      // google_apis/gaia/oauth2_access_token_fetcher.cc).
      return GoogleServiceAuthError(
          GoogleServiceAuthError::SERVICE_UNAVAILABLE);
    case kAuthenticationErrorCategoryNetworkServerErrors:
      // Just set the connection error state to FAILED.
      return GoogleServiceAuthError::FromConnectionError(
          net::URLRequestStatus::FAILED);
    case kAuthenticationErrorCategoryUserCancellationErrors:
      return GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED);
    case kAuthenticationErrorCategoryUnknownIdentityErrors:
      return GoogleServiceAuthError(GoogleServiceAuthError::USER_NOT_SIGNED_UP);
  }
}

// Converts a DeviceAccountsProvider::AccountInfo to an AccountInfo.
AccountInfo AccountInfoFromDeviceAccount(
    const DeviceAccountsProvider::AccountInfo& account) {
  AccountInfo account_info;
  account_info.email = account.email;
  account_info.gaia = account.gaia;
  account_info.hosted_domain = account.hosted_domain;
  return account_info;
}

class SSOAccessTokenFetcher : public OAuth2AccessTokenFetcher {
 public:
  SSOAccessTokenFetcher(OAuth2AccessTokenConsumer* consumer,
                        DeviceAccountsProvider* provider,
                        const AccountInfo& account);
  ~SSOAccessTokenFetcher() override;

  void Start(const std::string& client_id,
             const std::string& client_secret,
             const std::vector<std::string>& scopes) override;

  void CancelRequest() override;

  // Handles an access token response.
  void OnAccessTokenResponse(NSString* token,
                             NSDate* expiration,
                             NSError* error);

 private:
  DeviceAccountsProvider* provider_;  // weak
  AccountInfo account_;
  bool request_was_cancelled_;
  base::WeakPtrFactory<SSOAccessTokenFetcher> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SSOAccessTokenFetcher);
};

SSOAccessTokenFetcher::SSOAccessTokenFetcher(
    OAuth2AccessTokenConsumer* consumer,
    DeviceAccountsProvider* provider,
    const AccountInfo& account)
    : OAuth2AccessTokenFetcher(consumer),
      provider_(provider),
      account_(account),
      request_was_cancelled_(false),
      weak_factory_(this) {
  DCHECK(provider_);
}

SSOAccessTokenFetcher::~SSOAccessTokenFetcher() {}

void SSOAccessTokenFetcher::Start(const std::string& client_id,
                                  const std::string& client_secret_unused,
                                  const std::vector<std::string>& scopes) {
  std::set<std::string> scopes_set(scopes.begin(), scopes.end());
  provider_->GetAccessToken(
      account_.gaia, client_id, scopes_set,
      base::BindOnce(&SSOAccessTokenFetcher::OnAccessTokenResponse,
                     weak_factory_.GetWeakPtr()));
}

void SSOAccessTokenFetcher::CancelRequest() {
  request_was_cancelled_ = true;
}

void SSOAccessTokenFetcher::OnAccessTokenResponse(NSString* token,
                                                  NSDate* expiration,
                                                  NSError* error) {
  if (request_was_cancelled_) {
    // Ignore the callback if the request was cancelled.
    return;
  }
  GoogleServiceAuthError auth_error =
      GetGoogleServiceAuthErrorFromNSError(provider_, account_.gaia, error);
  if (auth_error.state() == GoogleServiceAuthError::NONE) {
    base::Time expiration_date =
        base::Time::FromDoubleT([expiration timeIntervalSince1970]);
    FireOnGetTokenSuccess(OAuth2AccessTokenConsumer::TokenResponse(
        base::SysNSStringToUTF8(token), expiration_date, std::string()));
  } else {
    FireOnGetTokenFailure(auth_error);
  }
}

}  // namespace

ProfileOAuth2TokenServiceIOSDelegate::ProfileOAuth2TokenServiceIOSDelegate(
    SigninClient* client,
    std::unique_ptr<DeviceAccountsProvider> provider,
    AccountTrackerService* account_tracker_service)
    : client_(client),
      provider_(std::move(provider)),
      account_tracker_service_(account_tracker_service) {
  DCHECK(client_);
  DCHECK(provider_);
  DCHECK(account_tracker_service_);
}

ProfileOAuth2TokenServiceIOSDelegate::~ProfileOAuth2TokenServiceIOSDelegate() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void ProfileOAuth2TokenServiceIOSDelegate::Shutdown() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  accounts_.clear();
}

void ProfileOAuth2TokenServiceIOSDelegate::LoadCredentials(
    const CoreAccountId& primary_account_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  DCHECK_EQ(signin::LoadCredentialsState::LOAD_CREDENTIALS_NOT_STARTED,
            load_credentials_state());
  set_load_credentials_state(
      signin::LoadCredentialsState::LOAD_CREDENTIALS_IN_PROGRESS);

  // Clean-up stale data from prefs.
  ClearExcludedSecondaryAccounts();

  if (primary_account_id.empty()) {
    // On startup, always fire refresh token loaded even if there is nothing
    // to load (not authenticated).
    set_load_credentials_state(
        signin::LoadCredentialsState::LOAD_CREDENTIALS_FINISHED_WITH_SUCCESS);
    FireRefreshTokensLoaded();
    return;
  }

  ReloadCredentials();

  set_load_credentials_state(
      signin::LoadCredentialsState::LOAD_CREDENTIALS_FINISHED_WITH_SUCCESS);
  FireRefreshTokensLoaded();
}

void ProfileOAuth2TokenServiceIOSDelegate::ReloadCredentials() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Get the list of new account ids.
  std::set<CoreAccountId> new_account_ids;
  for (const auto& new_account : provider_->GetAllAccounts()) {
    DCHECK(!new_account.gaia.empty());
    DCHECK(!new_account.email.empty());

    // Account must to be seeded before adding an account to ensure that
    // the GAIA ID is available if any client of this token service starts
    // a fetch access token operation when it receives a
    // |OnRefreshTokenAvailable| notification.
    CoreAccountId account_id = account_tracker_service_->SeedAccountInfo(
        AccountInfoFromDeviceAccount(new_account));
    new_account_ids.insert(account_id);
  }

  // Get the list of existing account ids.
  std::vector<CoreAccountId> old_account_ids = GetAccounts();
  std::sort(old_account_ids.begin(), old_account_ids.end());

  std::set<CoreAccountId> accounts_to_add =
      base::STLSetDifference<std::set<CoreAccountId>>(new_account_ids,
                                                      old_account_ids);
  std::set<CoreAccountId> accounts_to_remove =
      base::STLSetDifference<std::set<CoreAccountId>>(old_account_ids,
                                                      new_account_ids);
  if (accounts_to_add.empty() && accounts_to_remove.empty())
    return;

  // Remove all old accounts that do not appear in |new_accounts| and then
  // load |new_accounts|.
  ScopedBatchChange batch(this);
  for (const auto& account_to_remove : accounts_to_remove) {
    RemoveAccount(account_to_remove);
  }

  // Load all new_accounts.
  for (const auto& account_to_add : accounts_to_add) {
    AddOrUpdateAccount(account_to_add);
  }
}

void ProfileOAuth2TokenServiceIOSDelegate::UpdateCredentials(
    const CoreAccountId& account_id,
    const std::string& refresh_token) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  NOTREACHED() << "Unexpected call to UpdateCredentials when using shared "
                  "authentication.";
}

void ProfileOAuth2TokenServiceIOSDelegate::RevokeAllCredentials() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  ScopedBatchChange batch(this);
  AccountStatusMap toRemove = accounts_;
  for (auto& accountStatus : toRemove)
    RemoveAccount(accountStatus.first);

  DCHECK_EQ(0u, accounts_.size());
  ClearExcludedSecondaryAccounts();
}

void ProfileOAuth2TokenServiceIOSDelegate::ReloadAllAccountsFromSystem() {
  ReloadCredentials();
}

void ProfileOAuth2TokenServiceIOSDelegate::ReloadAccountFromSystem(
    const CoreAccountId& account_id) {
  AddOrUpdateAccount(account_id);
}

std::unique_ptr<OAuth2AccessTokenFetcher>
ProfileOAuth2TokenServiceIOSDelegate::CreateAccessTokenFetcher(
    const CoreAccountId& account_id,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    OAuth2AccessTokenConsumer* consumer) {
  AccountInfo account_info =
      account_tracker_service_->GetAccountInfo(account_id);
  return std::make_unique<SSOAccessTokenFetcher>(consumer, provider_.get(),
                                                 account_info);
}

std::vector<CoreAccountId> ProfileOAuth2TokenServiceIOSDelegate::GetAccounts()
    const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  std::vector<CoreAccountId> account_ids;
  for (const auto& account : accounts_)
    account_ids.push_back(account.first);
  return account_ids;
}

bool ProfileOAuth2TokenServiceIOSDelegate::RefreshTokenIsAvailable(
    const CoreAccountId& account_id) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  return accounts_.count(account_id) > 0;
}

GoogleServiceAuthError ProfileOAuth2TokenServiceIOSDelegate::GetAuthError(
    const CoreAccountId& account_id) const {
  auto it = accounts_.find(account_id);
  return (it == accounts_.end()) ? GoogleServiceAuthError::AuthErrorNone()
                                 : it->second.last_auth_error;
}

void ProfileOAuth2TokenServiceIOSDelegate::UpdateAuthError(
    const CoreAccountId& account_id,
    const GoogleServiceAuthError& error) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Do not report connection errors as these are not actually auth errors.
  // We also want to avoid masking a "real" auth error just because we
  // subsequently get a transient network error.
  if (error.IsTransientError())
    return;

  if (accounts_.count(account_id) == 0) {
    // Nothing to update as the account has already been removed.
    return;
  }

  AccountStatus* status = &accounts_[account_id];
  if (error.state() != status->last_auth_error.state()) {
    status->last_auth_error = error;
    FireAuthErrorChanged(account_id, error);
  }
}

// Clear the authentication error state and notify all observers that a new
// refresh token is available so that they request new access tokens.
void ProfileOAuth2TokenServiceIOSDelegate::AddOrUpdateAccount(
    const CoreAccountId& account_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Account must have been seeded before attempting to add it.
  DCHECK(!account_tracker_service_->GetAccountInfo(account_id).gaia.empty());
  DCHECK(!account_tracker_service_->GetAccountInfo(account_id).email.empty());

  bool account_present = accounts_.count(account_id) > 0;
  if (account_present && accounts_[account_id].last_auth_error.state() ==
                             GoogleServiceAuthError::NONE) {
    // No need to update the account if it is already a known account and if
    // there is no auth error.
    return;
  }

  accounts_[account_id].last_auth_error =
      GoogleServiceAuthError::AuthErrorNone();
  FireAuthErrorChanged(account_id, GoogleServiceAuthError::AuthErrorNone());
  FireRefreshTokenAvailable(account_id);
}

void ProfileOAuth2TokenServiceIOSDelegate::RemoveAccount(
    const CoreAccountId& account_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!account_id.empty());

  if (accounts_.count(account_id) > 0) {
    accounts_.erase(account_id);
    FireRefreshTokenRevoked(account_id);
  }
}

void ProfileOAuth2TokenServiceIOSDelegate::ClearExcludedSecondaryAccounts() {
  client_->GetPrefs()->ClearPref(
      prefs::kTokenServiceExcludeAllSecondaryAccounts);
  client_->GetPrefs()->ClearPref(prefs::kTokenServiceExcludedSecondaryAccounts);
}
