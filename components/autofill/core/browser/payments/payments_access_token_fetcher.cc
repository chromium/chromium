// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_access_token_fetcher.h"

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "components/signin/public/base/oauth_consumer_id.h"
#include "components/signin/public/identity_manager/access_token_fetcher.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace autofill::payments {
PaymentsAccessTokenFetcher::PaymentsAccessTokenFetcher(
    signin::IdentityManager& identity_manager)
    : identity_manager_(identity_manager) {}

PaymentsAccessTokenFetcher::~PaymentsAccessTokenFetcher() = default;

void PaymentsAccessTokenFetcher::GetAccessToken(bool invalidate_old,
                                                FinishCallback callback) {
  if (!invalidate_old) {
    if (token_fetcher_) {
      // We're still waiting for the last request to come back.
      return;
    }
    if (!access_token_.empty()) {
      // If there is an access_token ready for use, return it directly.
      std::move(callback).Run(access_token_);
      return;
    }
  }

  // Otherwise starts fetching a new access token.
  CoreAccountId account_id =
      identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
          .account_id;

  // Invalidate old token before fetching, if necessary.
  if (invalidate_old && !access_token_.empty()) {
    identity_manager_->RemoveAccessTokenFromCache(
        account_id, signin::OAuthConsumerId::kPaymentsAccessTokenFetcher,
        access_token_);
  }

  access_token_.clear();
  callback_ = std::move(callback);
  token_fetcher_ = identity_manager_->CreateAccessTokenFetcherForAccount(
      account_id, signin::OAuthConsumerId::kPaymentsAccessTokenFetcher,
      base::BindOnce(&PaymentsAccessTokenFetcher::AccessTokenFetchFinished,
                     weak_ptr_factory_.GetWeakPtr()),
      signin::AccessTokenFetcher::Mode::kImmediate);
}

void PaymentsAccessTokenFetcher::AccessTokenFetchFinished(
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  token_fetcher_.reset();
  if (error.state() != GoogleServiceAuthError::NONE) {
    std::move(callback_).Run(error);
  } else {
    std::move(callback_).Run(access_token_info.token);
  }
}

}  // namespace autofill::payments
