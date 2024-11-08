// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/oauth_multilogin_token_fetcher.h"

#include <set>
#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "components/signin/internal/identity_manager/oauth_multilogin_token_request.h"
#include "components/signin/internal/identity_manager/oauth_multilogin_token_response.h"
#include "components/signin/internal/identity_manager/profile_oauth2_token_service.h"
#include "components/signin/public/base/signin_client.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace {

void RecordGetAccessTokenFinished(GoogleServiceAuthError error) {
  UMA_HISTOGRAM_ENUMERATION("Signin.GetAccessTokenFinished", error.state(),
                            GoogleServiceAuthError::NUM_STATES);
}

}  // namespace

namespace signin {

OAuthMultiloginTokenFetcher::OAuthMultiloginTokenFetcher(
    SigninClient* signin_client,
    ProfileOAuth2TokenService* token_service,
    const std::vector<CoreAccountId>& account_ids,
    SuccessCallback success_callback,
    FailureCallback failure_callback)
    : signin_client_(signin_client),
      token_service_(token_service),
      account_ids_(account_ids),
      success_callback_(std::move(success_callback)),
      failure_callback_(std::move(failure_callback)) {
  DCHECK(signin_client_);
  DCHECK(token_service_);
  DCHECK(!account_ids_.empty());
  DCHECK(success_callback_);
  DCHECK(failure_callback_);

#ifndef NDEBUG
  // Check that there is no duplicate accounts.
  std::set<CoreAccountId> accounts_no_duplicates(account_ids_.begin(),
                                                 account_ids_.end());
  DCHECK_EQ(account_ids_.size(), accounts_no_duplicates.size());
#endif

  for (const CoreAccountId& account_id : account_ids_) {
    StartFetchingToken(account_id);
  }
}

OAuthMultiloginTokenFetcher::~OAuthMultiloginTokenFetcher() = default;

void OAuthMultiloginTokenFetcher::StartFetchingToken(
    const CoreAccountId& account_id) {
  DCHECK(!account_id.empty());
  // Add a request to `token_requests_` before calling `token_service_` to
  // ensure that a request cannot complete before it's added to
  // `token_requests_`.
  // base::Unretained(this) is safe because `this` owns
  // `OAuthMultiloginTokenRequest` which owns the callback.
  token_requests_.push_back(std::make_unique<OAuthMultiloginTokenRequest>(
      account_id,
      base::BindOnce(&OAuthMultiloginTokenFetcher::OnTokenRequestComplete,
                     base::Unretained(this))));
  token_service_->StartRequestForMultilogin(*token_requests_.back());
}

void OAuthMultiloginTokenFetcher::OnTokenRequestComplete(
    const OAuthMultiloginTokenRequest* request,
    OAuthMultiloginTokenRequest::Result result) {
  CoreAccountId account_id = request->account_id();
  CHECK(base::Contains(account_ids_, account_id));
  size_t num_erased = std::erase_if(
      token_requests_,
      [request](const auto& element) { return element.get() == request; });
  CHECK_EQ(num_erased, 1U);
  // Do not use `request` below this line, as it is deleted.
  if (result.has_value()) {
    TokenRequestSucceeded(account_id, std::move(result).value());
  } else {
    TokenRequestFailed(account_id, std::move(result).error());
  }
}

void OAuthMultiloginTokenFetcher::TokenRequestSucceeded(
    const CoreAccountId& account_id,
    OAuthMultiloginTokenResponse response) {
  CHECK(!response.oauth_token().empty());
  auto [_, inserted] =
      access_tokens_.insert({account_id, response.oauth_token()});
  DCHECK(inserted);  // If this fires, we have a duplicate account.

  if (access_tokens_.size() != account_ids_.size()) {
    return;
  }

  // We've received access tokens for all accounts, return the result.
  std::vector<AccountIdTokenPair> account_token_pairs;
  for (const auto& id : account_ids_) {
    const auto it = access_tokens_.find(id);
    DCHECK(!it->second.empty());
    account_token_pairs.emplace_back(id, it->second);
  }
  RecordGetAccessTokenFinished(GoogleServiceAuthError::AuthErrorNone());
  std::move(success_callback_).Run(account_token_pairs);
  // Do not add anything below this line, as `this` may be deleted.
}

void OAuthMultiloginTokenFetcher::TokenRequestFailed(
    const CoreAccountId& account_id,
    GoogleServiceAuthError error) {
  VLOG(1) << "Failed to retrieve a token for multilogin. account=" << account_id
          << " error=" << error.ToString();
  if (error.IsTransientError() &&
      retried_requests_.find(account_id) == retried_requests_.end()) {
    retried_requests_.insert(account_id);
    // Fetching fresh access tokens requires network.
    signin_client_->DelayNetworkCall(
        base::BindOnce(&OAuthMultiloginTokenFetcher::StartFetchingToken,
                       weak_ptr_factory_.GetWeakPtr(), account_id));
    return;
  }
  RecordGetAccessTokenFinished(error);
  token_requests_.clear();  // Cancel pending requests.
  std::move(failure_callback_).Run(error);
  // Do not add anything below this line, as `this` may be deleted.
}

}  // namespace signin
