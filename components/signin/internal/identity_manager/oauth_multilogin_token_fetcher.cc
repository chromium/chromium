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
#include "components/signin/internal/identity_manager/profile_oauth2_token_service.h"
#include "components/signin/public/base/signin_client.h"

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
    : OAuth2AccessTokenManager::Consumer("oauth_multilogin_token_fetcher"),
      signin_client_(signin_client),
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

  for (const CoreAccountId& account_id : account_ids_)
    StartFetchingToken(account_id);
}

OAuthMultiloginTokenFetcher::~OAuthMultiloginTokenFetcher() = default;

void OAuthMultiloginTokenFetcher::StartFetchingToken(
    const CoreAccountId& account_id) {
  DCHECK(!account_id.empty());
  token_requests_.push_back(
      token_service_->StartRequestForMultilogin(account_id, this));
}

void OAuthMultiloginTokenFetcher::OnGetTokenSuccess(
    const OAuth2AccessTokenManager::Request* request,
    const OAuth2AccessTokenConsumer::TokenResponse& token_response) {
  CoreAccountId account_id = request->GetAccountId();
  DCHECK(base::Contains(account_ids_, account_id));

  const std::string token = token_response.access_token;
  DCHECK(!token.empty());
  // Do not use |request| and |token_response| below this line, as they are
  // deleted.
  EraseRequest(request);

  const auto& inserted =
      access_tokens_.insert(std::make_pair(account_id, token));
  DCHECK(inserted.second);  // If this fires, we have a duplicate account.

  if (access_tokens_.size() == account_ids_.size()) {
    std::vector<AccountIdTokenPair> account_token_pairs;
    for (const auto& id : account_ids_) {
      const auto& it = access_tokens_.find(id);
      DCHECK(!it->second.empty());
      account_token_pairs.emplace_back(id, it->second);
    }
    RecordGetAccessTokenFinished(GoogleServiceAuthError::AuthErrorNone());
    std::move(success_callback_).Run(account_token_pairs);
    // Do not add anything below this line, as this may be deleted.
  }
}

void OAuthMultiloginTokenFetcher::OnGetTokenFailure(
    const OAuth2AccessTokenManager::Request* request,
    const GoogleServiceAuthError& error) {
  CoreAccountId account_id = request->GetAccountId();
  VLOG(1) << "Failed to retrieve accesstoken account=" << account_id
          << " error=" << error.ToString();
  if (error.IsTransientError() &&
      retried_requests_.find(account_id) == retried_requests_.end()) {
    retried_requests_.insert(account_id);
    EraseRequest(request);
    // Fetching fresh access tokens requires network.
    signin_client_->DelayNetworkCall(
        base::BindOnce(&OAuthMultiloginTokenFetcher::StartFetchingToken,
                       weak_ptr_factory_.GetWeakPtr(), account_id));
    return;
  }
  RecordGetAccessTokenFinished(error);
  // Copy the error because it is a reference owned by token_requests_.
  GoogleServiceAuthError error_copy = error;
  token_requests_.clear();  // Cancel pending requests.
  std::move(failure_callback_).Run(error_copy);
  // Do not add anything below this line, as this may be deleted.
}

void OAuthMultiloginTokenFetcher::EraseRequest(
    const OAuth2AccessTokenManager::Request* request) {
  for (auto it = token_requests_.begin(); it != token_requests_.end(); ++it) {
    if (it->get() == request) {
      token_requests_.erase(it);
      return;
    }
  }
  NOTREACHED_IN_MIGRATION() << "Request not found";
}

}  // namespace signin
