// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/ubertoken_fetcher_impl.h"

#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/time/time.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/network/public/cpp/wrapper_shared_url_loader_factory.h"

namespace {
std::unique_ptr<GaiaAuthFetcher> CreateGaiaAuthFetcher(
    gaia::GaiaSource source,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    GaiaAuthConsumer* consumer) {
  return std::make_unique<GaiaAuthFetcher>(consumer, source,
                                           url_loader_factory);
}
}  // namespace

namespace signin {

const int UbertokenFetcherImpl::kMaxRetries = 3;

UbertokenFetcherImpl::UbertokenFetcherImpl(
    const CoreAccountId& account_id,
    ProfileOAuth2TokenService* token_service,
    CompletionCallback ubertoken_callback,
    gaia::GaiaSource source,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : UbertokenFetcherImpl(account_id,
                           /*access_token=*/"",
                           token_service,
                           std::move(ubertoken_callback),
                           base::BindRepeating(CreateGaiaAuthFetcher,
                                               source,
                                               url_loader_factory)) {}

UbertokenFetcherImpl::UbertokenFetcherImpl(
    const CoreAccountId& account_id,
    const std::string& access_token,
    ProfileOAuth2TokenService* token_service,
    CompletionCallback ubertoken_callback,
    GaiaAuthFetcherFactory factory)
    : OAuth2AccessTokenManager::Consumer("uber_token_fetcher"),
      token_service_(token_service),
      ubertoken_callback_(std::move(ubertoken_callback)),
      gaia_auth_fetcher_factory_(factory),
      account_id_(account_id),
      access_token_(access_token),
      retry_number_(0),
      second_access_token_request_(false) {
  DCHECK(!account_id.empty());
  DCHECK(token_service);
  DCHECK(!ubertoken_callback_.is_null());

  if (access_token_.empty()) {
    RequestAccessToken();
    return;
  }

  ExchangeTokens();
}

UbertokenFetcherImpl::~UbertokenFetcherImpl() {}

void UbertokenFetcherImpl::OnUberAuthTokenSuccess(const std::string& token) {
  std::move(ubertoken_callback_)
      .Run(GoogleServiceAuthError::AuthErrorNone(), token);
}

void UbertokenFetcherImpl::OnUberAuthTokenFailure(
    const GoogleServiceAuthError& error) {
  // Retry only transient errors.
  bool should_retry =
      error.state() == GoogleServiceAuthError::CONNECTION_FAILED ||
      error.state() == GoogleServiceAuthError::SERVICE_UNAVAILABLE;
  if (should_retry) {
    if (retry_number_ < kMaxRetries) {
      // Calculate an exponential backoff with randomness of less than 1 sec.
      double backoff = base::RandDouble() + (1 << retry_number_);
      ++retry_number_;
      UMA_HISTOGRAM_ENUMERATION("Signin.UberTokenRetry", error.state(),
                                GoogleServiceAuthError::NUM_STATES);
      retry_timer_.Stop();
      retry_timer_.Start(FROM_HERE, base::TimeDelta::FromSecondsD(backoff),
                         this, &UbertokenFetcherImpl::ExchangeTokens);
      return;
    }
  } else {
    // The access token is invalid.  Tell the token service.
    OAuth2AccessTokenManager::ScopeSet scopes;
    scopes.insert(GaiaConstants::kOAuth1LoginScope);
    token_service_->InvalidateAccessToken(account_id_, scopes, access_token_);

    // In case the access was just stale, try one more time.
    if (!second_access_token_request_) {
      second_access_token_request_ = true;
      RequestAccessToken();
      return;
    }
  }

  UMA_HISTOGRAM_ENUMERATION("Signin.UberTokenFailure", error.state(),
                            GoogleServiceAuthError::NUM_STATES);
  std::move(ubertoken_callback_).Run(error, /*access_token=*/std::string());
}

void UbertokenFetcherImpl::OnGetTokenSuccess(
    const OAuth2AccessTokenManager::Request* request,
    const OAuth2AccessTokenConsumer::TokenResponse& token_response) {
  DCHECK(!token_response.access_token.empty());
  access_token_ = token_response.access_token;
  access_token_request_.reset();
  ExchangeTokens();
}

void UbertokenFetcherImpl::OnGetTokenFailure(
    const OAuth2AccessTokenManager::Request* request,
    const GoogleServiceAuthError& error) {
  access_token_request_.reset();
  std::move(ubertoken_callback_).Run(error, /*access_token=*/std::string());
}

void UbertokenFetcherImpl::RequestAccessToken() {
  retry_number_ = 0;
  gaia_auth_fetcher_.reset();
  retry_timer_.Stop();

  OAuth2AccessTokenManager::ScopeSet scopes;
  scopes.insert(GaiaConstants::kOAuth1LoginScope);
  access_token_request_ =
      token_service_->StartRequest(account_id_, scopes, this);
}

void UbertokenFetcherImpl::ExchangeTokens() {
  gaia_auth_fetcher_ = gaia_auth_fetcher_factory_.Run(this);
  gaia_auth_fetcher_->StartTokenFetchForUberAuthExchange(access_token_);
}

}  // namespace signin
