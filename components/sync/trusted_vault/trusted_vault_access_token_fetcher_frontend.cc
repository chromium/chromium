// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/trusted_vault/trusted_vault_access_token_fetcher_frontend.h"

#include <utility>

#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"

namespace syncer {

namespace {
const char kCryptAuthOAuth2Scope[] =
    "https://www.googleapis.com/auth/cryptauth";
}  // namespace

TrustedVaultAccessTokenFetcherFrontend::TrustedVaultAccessTokenFetcherFrontend(
    signin::IdentityManager* identity_manager)
    : identity_manager_(identity_manager) {
  DCHECK(identity_manager_);
  identity_manager_->AddObserver(this);
  UpdatePrimaryAccountIfNeeded();
}

TrustedVaultAccessTokenFetcherFrontend::
    ~TrustedVaultAccessTokenFetcherFrontend() {
  identity_manager_->RemoveObserver(this);
}

base::WeakPtr<TrustedVaultAccessTokenFetcherFrontend>
TrustedVaultAccessTokenFetcherFrontend::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void TrustedVaultAccessTokenFetcherFrontend::FetchAccessToken(
    const CoreAccountId& account_id,
    TrustedVaultAccessTokenFetcher::TokenCallback callback) {
  if (account_id != primary_account_) {
    // The requester is likely not aware of a recent change to the primary
    // account yet (this is possible because requests come from another
    // sequence). Run |callback| immediately without access token.
    std::move(callback).Run(base::nullopt);
    return;
  }

  pending_requests_.emplace_back(std::move(callback));
  if (ongoing_access_token_fetch_ == nullptr) {
    StartAccessTokenFetch();
  }
}

void TrustedVaultAccessTokenFetcherFrontend::OnPrimaryAccountSet(
    const CoreAccountInfo& primary_account_info) {
  UpdatePrimaryAccountIfNeeded();
}

void TrustedVaultAccessTokenFetcherFrontend::OnPrimaryAccountCleared(
    const CoreAccountInfo& previous_primary_account_info) {
  UpdatePrimaryAccountIfNeeded();
}

void TrustedVaultAccessTokenFetcherFrontend::OnUnconsentedPrimaryAccountChanged(
    const CoreAccountInfo& unconsented_primary_account_info) {
  UpdatePrimaryAccountIfNeeded();
}

void TrustedVaultAccessTokenFetcherFrontend::UpdatePrimaryAccountIfNeeded() {
  CoreAccountInfo primary_account_info =
      identity_manager_->GetPrimaryAccountInfo(
          signin::ConsentLevel::kNotRequired);
  if (primary_account_info.account_id == primary_account_) {
    return;
  }

  // Fulfill |pending_requests_| since they belong to the previous
  // |primary_account_|.
  FulfillPendingRequests(base::nullopt);
  ongoing_access_token_fetch_ = nullptr;
  primary_account_ = primary_account_info.account_id;
}

void TrustedVaultAccessTokenFetcherFrontend::StartAccessTokenFetch() {
  DCHECK(!ongoing_access_token_fetch_);
  ongoing_access_token_fetch_ = std::make_unique<
      signin::PrimaryAccountAccessTokenFetcher>(
      /*ouath_consumer_name=*/"TrustedVaultAccessTokenFetcherFrontend",
      identity_manager_, signin::ScopeSet{kCryptAuthOAuth2Scope},
      base::BindOnce(
          &TrustedVaultAccessTokenFetcherFrontend::OnAccessTokenFetchCompleted,
          base::Unretained(this)),
      signin::PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable,
      signin::ConsentLevel::kNotRequired);
}

void TrustedVaultAccessTokenFetcherFrontend::OnAccessTokenFetchCompleted(
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  ongoing_access_token_fetch_ = nullptr;
  if (error.state() == GoogleServiceAuthError::NONE) {
    FulfillPendingRequests(access_token_info);
  } else {
    FulfillPendingRequests(base::nullopt);
  }
}

void TrustedVaultAccessTokenFetcherFrontend::FulfillPendingRequests(
    base::Optional<signin::AccessTokenInfo> access_token_info) {
  for (auto& pending_request : pending_requests_) {
    std::move(pending_request).Run(access_token_info);
  }
  pending_requests_.clear();
}

}  // namespace syncer
