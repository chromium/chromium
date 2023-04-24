// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/trusted_vault_access_token_fetcher_frontend.h"

#include <utility>

#include "base/types/expected.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "components/trusted_vault/trusted_vault_access_token_fetcher.h"

namespace trusted_vault {

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
    // Although |account_id| can be in persistent auth error state, it's not
    // relevant here since primary account change is the main reason for
    // returning nullopt, so passing false is fine.
    std::move(callback).Run(base::unexpected(
        TrustedVaultAccessTokenFetcher::FetchingError::kNotPrimaryAccount));
    return;
  }

  pending_requests_.emplace_back(std::move(callback));
  if (ongoing_access_token_fetch_ == nullptr) {
    StartAccessTokenFetch();
  }
}

void TrustedVaultAccessTokenFetcherFrontend::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event) {
  UpdatePrimaryAccountIfNeeded();
}

void TrustedVaultAccessTokenFetcherFrontend::UpdatePrimaryAccountIfNeeded() {
  CoreAccountInfo primary_account_info =
      identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  if (primary_account_info.account_id == primary_account_) {
    return;
  }

  // Fulfill |pending_requests_| since they belong to the previous
  // |primary_account_|.
  FulfillPendingRequests(base::unexpected(
      TrustedVaultAccessTokenFetcher::FetchingError::kNotPrimaryAccount));
  ongoing_access_token_fetch_ = nullptr;
  primary_account_ = primary_account_info.account_id;
}

void TrustedVaultAccessTokenFetcherFrontend::StartAccessTokenFetch() {
  DCHECK(!ongoing_access_token_fetch_);
  // Use kWaitUntilAvailable to avoid failed fetches while refresh tokens are
  // still loading. Note, that it doesn't cause infinite waits for persistent
  // auth errors.
  ongoing_access_token_fetch_ = std::make_unique<
      signin::PrimaryAccountAccessTokenFetcher>(
      /*ouath_consumer_name=*/"TrustedVaultAccessTokenFetcherFrontend",
      identity_manager_, signin::ScopeSet{kCryptAuthOAuth2Scope},
      base::BindOnce(
          &TrustedVaultAccessTokenFetcherFrontend::OnAccessTokenFetchCompleted,
          base::Unretained(this)),
      signin::PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable,
      signin::ConsentLevel::kSignin);
}

void TrustedVaultAccessTokenFetcherFrontend::OnAccessTokenFetchCompleted(
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  ongoing_access_token_fetch_ = nullptr;
  if (error.state() == GoogleServiceAuthError::NONE) {
    FulfillPendingRequests(access_token_info);
  } else if (error.IsPersistentError()) {
    FulfillPendingRequests(base::unexpected(
        TrustedVaultAccessTokenFetcher::FetchingError::kPersistentAuthError));
  } else {
    FulfillPendingRequests(base::unexpected(
        TrustedVaultAccessTokenFetcher::FetchingError::kTransientAuthError));
  }
}

void TrustedVaultAccessTokenFetcherFrontend::FulfillPendingRequests(
    TrustedVaultAccessTokenFetcher::AccessTokenInfoOrError
        access_token_or_error) {
  for (TrustedVaultAccessTokenFetcher::TokenCallback& pending_request :
       pending_requests_) {
    std::move(pending_request).Run(access_token_or_error);
  }
  pending_requests_.clear();
}

}  // namespace trusted_vault
