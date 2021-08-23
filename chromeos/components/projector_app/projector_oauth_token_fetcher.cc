// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/projector_app/projector_oauth_token_fetcher.h"

#include "ash/public/cpp/projector/projector_controller.h"
#include "base/containers/contains.h"
#include "base/time/time.h"
#include "chromeos/components/projector_app/projector_app_client.h"
#include "components/signin/public/identity_manager/access_token_fetcher.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/oauth2_access_token_manager.h"

namespace {

// The cached OAuth token needs to be valid at least until base::Time::Now()
// + `kBufferTime`. The buffer time will be useful to ensure that we don't send
// soon to expire tokens to the Projector app.
const base::TimeDelta kBufferTime = base::TimeDelta::FromSeconds(4);

signin::IdentityManager* GetIdentityManager() {
  return chromeos::ProjectorAppClient::Get()->GetIdentityManager();
}

}  // namespace

namespace chromeos {

AccessTokenRequests::AccessTokenRequests() = default;

AccessTokenRequests::AccessTokenRequests(AccessTokenRequests&&) = default;

AccessTokenRequests& AccessTokenRequests::operator=(AccessTokenRequests&&) =
    default;

AccessTokenRequests::~AccessTokenRequests() = default;

ProjectorOAuthTokenFetcher::ProjectorOAuthTokenFetcher() = default;

ProjectorOAuthTokenFetcher::~ProjectorOAuthTokenFetcher() = default;

std::vector<AccountInfo> ProjectorOAuthTokenFetcher::GetAccounts() const {
  return GetIdentityManager()
      ->GetExtendedAccountInfoForAccountsWithRefreshToken();
}

CoreAccountInfo ProjectorOAuthTokenFetcher::GetPrimaryAccountInfo() const {
  return GetIdentityManager()->GetPrimaryAccountInfo(
      signin::ConsentLevel::kSignin);
}

void ProjectorOAuthTokenFetcher::GetAccessTokenFor(
    const std::string& gaia_id,
    AccessTokenRequestCallback callback) {
  if (base::Contains(fetched_access_tokens_, gaia_id)) {
    const auto& access_token_info = fetched_access_tokens_[gaia_id];
    if (base::Time::Now() + kBufferTime < access_token_info.expiration_time) {
      std::move(callback).Run(
          gaia_id, GoogleServiceAuthError(GoogleServiceAuthError::NONE),
          access_token_info);
      return;
    }

    // Else the stored value is expired. Let's remove its entry.
    fetched_access_tokens_.erase(gaia_id);
  }

  // If there is a pending fetch for the gaia_id, then append the callback to
  // the pending callbacks.
  if (base::Contains(pending_oauth_token_fetch_, gaia_id)) {
    pending_oauth_token_fetch_[gaia_id].callbacks.push_back(
        std::move(callback));
    return;
  }

  InitiateAccessTokenFetchFor(gaia_id, std::move(callback));
}

void ProjectorOAuthTokenFetcher::InitiateAccessTokenFetchFor(
    const std::string& gaia_id,
    AccessTokenRequestCallback callback) {
  // There is no pending fetch for the gaia_id. Let's create a new fetch.
  // Let's start creating the oauth2 access token request.
  OAuth2AccessTokenManager::ScopeSet scopes;
  scopes.insert(GaiaConstants::kDriveOAuth2Scope);
  scopes.insert(GaiaConstants::kDriveReadOnlyOAuth2Scope);
  scopes.insert(GaiaConstants::kCloudTranslationOAuth2Scope);

  // kImmediate makes a one-shot immediate request.
  const auto mode = signin::AccessTokenFetcher::Mode::kImmediate;

  // Create the fetcher via |identity_manager|.
  auto* identity_manager = GetIdentityManager();
  std::unique_ptr<signin::AccessTokenFetcher> access_token_fetcher =
      identity_manager->CreateAccessTokenFetcherForAccount(
          identity_manager->FindExtendedAccountInfoByGaiaId(gaia_id).account_id,
          /*oauth_consumer_name=*/"ProjectorOAuthTokenFetcher", scopes,
          base::BindOnce(
              &ProjectorOAuthTokenFetcher::OnAccessTokenRequestCompleted,
              // It is safe to use base::Unretained as |this| owns
              // |access_token_fetcher_|.
              base::Unretained(this), gaia_id),
          mode);
  AccessTokenRequests& entry = pending_oauth_token_fetch_[gaia_id];
  entry.access_token_fetcher = std::move(access_token_fetcher);
  entry.callbacks.push_back(std::move(callback));
}

void ProjectorOAuthTokenFetcher::OnAccessTokenRequestCompleted(
    const std::string& gaia_id,
    GoogleServiceAuthError error,
    signin::AccessTokenInfo info) {
  if (!base::Contains(pending_oauth_token_fetch_, gaia_id))
    return;

  for (auto& callback : pending_oauth_token_fetch_[gaia_id].callbacks)
    std::move(callback).Run(gaia_id, error, info);

  if (error.state() == GoogleServiceAuthError::State::NONE)
    fetched_access_tokens_[gaia_id] = std::move(info);

  pending_oauth_token_fetch_.erase(gaia_id);
}

}  // namespace chromeos
