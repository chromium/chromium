// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/test_profile_oauth2_token_service_delegate_chromeos.h"

#include <limits>

#include "base/functional/callback_helpers.h"
#include "components/account_manager_core/account_manager_facade_impl.h"
#include "components/account_manager_core/chromeos/account_manager_mojo_service.h"
#include "google_apis/gaia/oauth2_access_token_fetcher.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace signin {

TestProfileOAuth2TokenServiceDelegateChromeOS::
    TestProfileOAuth2TokenServiceDelegateChromeOS(
        SigninClient* client,
        AccountTrackerService* account_tracker_service,
        crosapi::AccountManagerMojoService* account_manager_mojo_service,
        bool is_regular_profile)
    : ProfileOAuth2TokenServiceDelegate(/*use_backoff=*/true) {
  if (!network::TestNetworkConnectionTracker::HasInstance()) {
    owned_tracker_ = network::TestNetworkConnectionTracker::CreateInstance();
  }

  mojo::Remote<crosapi::mojom::AccountManager> remote;
  account_manager_mojo_service->BindReceiver(
      remote.BindNewPipeAndPassReceiver());
  account_manager_facade_ =
      std::make_unique<account_manager::AccountManagerFacadeImpl>(
          std::move(remote),
          /*remote_version=*/std::numeric_limits<uint32_t>::max(),
          /*account_manager_for_tests=*/nullptr);

  delegate_ = std::make_unique<ProfileOAuth2TokenServiceDelegateChromeOS>(
      client, account_tracker_service,
      network::TestNetworkConnectionTracker::GetInstance(),
      account_manager_facade_.get(), is_regular_profile);
  // This still mimics in product behavior as the `delegate_` 's only
  // observer is this class. When `OnRefreshTokenRevoked()` is called, `This`
  // calls `FireRefreshTokenAvailable()` which has the callback set correctly.
  delegate_->SetOnRefreshTokenRevokedNotified(base::DoNothing());
  token_service_observation_.Observe(delegate_.get());
}

TestProfileOAuth2TokenServiceDelegateChromeOS::
    ~TestProfileOAuth2TokenServiceDelegateChromeOS() = default;

std::unique_ptr<OAuth2AccessTokenFetcher>
TestProfileOAuth2TokenServiceDelegateChromeOS::CreateAccessTokenFetcher(
    const CoreAccountId& account_id,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    OAuth2AccessTokenConsumer* consumer,
    const std::string& token_binding_challenge) {
  return delegate_->CreateAccessTokenFetcher(account_id, url_loader_factory,
                                             consumer, token_binding_challenge);
}

bool TestProfileOAuth2TokenServiceDelegateChromeOS::RefreshTokenIsAvailable(
    const CoreAccountId& account_id) const {
  return delegate_->RefreshTokenIsAvailable(account_id);
}

void TestProfileOAuth2TokenServiceDelegateChromeOS::UpdateAuthError(
    const CoreAccountId& account_id,
    const GoogleServiceAuthError& error,
    bool fire_auth_error_changed) {
  delegate_->UpdateAuthError(account_id, error, fire_auth_error_changed);
}

GoogleServiceAuthError
TestProfileOAuth2TokenServiceDelegateChromeOS::GetAuthError(
    const CoreAccountId& account_id) const {
  return delegate_->GetAuthError(account_id);
}

std::vector<CoreAccountId>
TestProfileOAuth2TokenServiceDelegateChromeOS::GetAccounts() const {
  return delegate_->GetAccounts();
}

void TestProfileOAuth2TokenServiceDelegateChromeOS::ClearAuthError(
    const std::optional<CoreAccountId>& account_id) {
  delegate_->ClearAuthError(account_id);
}

GoogleServiceAuthError
TestProfileOAuth2TokenServiceDelegateChromeOS::BackOffError() const {
  return delegate_->BackOffError();
}

void TestProfileOAuth2TokenServiceDelegateChromeOS::ResetBackOffEntry() {
  delegate_->ResetBackOffEntry();
}

void TestProfileOAuth2TokenServiceDelegateChromeOS::LoadCredentialsInternal(
    const CoreAccountId& primary_account_id,
    bool is_syncing) {
  // In tests |LoadCredentials| may be called twice, in this case we call
  // |FireRefreshTokensLoaded| again to notify that credentials are loaded.
  if (load_credentials_state() ==
      signin::LoadCredentialsState::LOAD_CREDENTIALS_FINISHED_WITH_SUCCESS) {
    FireRefreshTokensLoaded();
    return;
  }

  if (load_credentials_state() !=
      signin::LoadCredentialsState::LOAD_CREDENTIALS_NOT_STARTED) {
    return;
  }

  set_load_credentials_state(
      signin::LoadCredentialsState::LOAD_CREDENTIALS_IN_PROGRESS);
  delegate_->LoadCredentials(primary_account_id, is_syncing);
}

void TestProfileOAuth2TokenServiceDelegateChromeOS::UpdateCredentialsInternal(
    const CoreAccountId& account_id,
    const std::string& refresh_token) {
  delegate_->UpdateCredentials(account_id, refresh_token);
}

scoped_refptr<network::SharedURLLoaderFactory>
TestProfileOAuth2TokenServiceDelegateChromeOS::GetURLLoaderFactory() const {
  return delegate_->GetURLLoaderFactory();
}

void TestProfileOAuth2TokenServiceDelegateChromeOS::RevokeCredentialsInternal(
    const CoreAccountId& account_id) {
  delegate_->RevokeCredentials(account_id);
}

void TestProfileOAuth2TokenServiceDelegateChromeOS::
    RevokeAllCredentialsInternal(
        signin_metrics::SourceForRefreshTokenOperation source) {
  delegate_->RevokeAllCredentials(source);
}

const net::BackoffEntry*
TestProfileOAuth2TokenServiceDelegateChromeOS::BackoffEntry() const {
  return delegate_->BackoffEntry();
}

void TestProfileOAuth2TokenServiceDelegateChromeOS::OnRefreshTokenAvailable(
    const CoreAccountId& account_id) {
  FireRefreshTokenAvailable(account_id);
}

void TestProfileOAuth2TokenServiceDelegateChromeOS::OnRefreshTokenRevoked(
    const CoreAccountId& account_id) {
  FireRefreshTokenRevoked(account_id);
}

void TestProfileOAuth2TokenServiceDelegateChromeOS::OnEndBatchChanges() {
  FireEndBatchChanges();
}

void TestProfileOAuth2TokenServiceDelegateChromeOS::OnRefreshTokensLoaded() {
  set_load_credentials_state(
      signin::LoadCredentialsState::LOAD_CREDENTIALS_FINISHED_WITH_SUCCESS);
  FireRefreshTokensLoaded();
}

void TestProfileOAuth2TokenServiceDelegateChromeOS::OnAuthErrorChanged(
    const CoreAccountId& account_id,
    const GoogleServiceAuthError& auth_error,
    signin_metrics::SourceForRefreshTokenOperation source) {
  FireAuthErrorChanged(account_id, auth_error);
}

}  // namespace signin
