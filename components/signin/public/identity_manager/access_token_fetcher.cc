// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/identity_manager/access_token_fetcher.h"

#include <utility>

#include "base/logging.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace signin {

AccessTokenFetcher::AccessTokenFetcher(const CoreAccountId& account_id,
                                       const std::string& oauth_consumer_name,
                                       ProfileOAuth2TokenService* token_service,
                                       const identity::ScopeSet& scopes,
                                       TokenCallback callback,
                                       Mode mode)
    : AccessTokenFetcher(account_id,
                         oauth_consumer_name,
                         token_service,
                         /*url_loader_factory=*/nullptr,
                         scopes,
                         std::move(callback),
                         mode) {}

AccessTokenFetcher::AccessTokenFetcher(
    const CoreAccountId& account_id,
    const std::string& oauth_consumer_name,
    ProfileOAuth2TokenService* token_service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const identity::ScopeSet& scopes,
    TokenCallback callback,
    Mode mode)
    : AccessTokenFetcher(account_id,
                         /*client_id=*/std::string(),
                         /*client_secret=*/std::string(),
                         oauth_consumer_name,
                         token_service,
                         std::move(url_loader_factory),
                         scopes,
                         std::move(callback),
                         mode) {}

AccessTokenFetcher::AccessTokenFetcher(const CoreAccountId& account_id,
                                       const std::string client_id,
                                       const std::string client_secret,
                                       const std::string& oauth_consumer_name,
                                       ProfileOAuth2TokenService* token_service,
                                       const identity::ScopeSet& scopes,
                                       TokenCallback callback,
                                       Mode mode)
    : AccessTokenFetcher(account_id,
                         client_id,
                         client_secret,
                         oauth_consumer_name,
                         token_service,
                         /*url_loader_factory=*/nullptr,
                         scopes,
                         std::move(callback),
                         mode) {}

AccessTokenFetcher::AccessTokenFetcher(
    const CoreAccountId& account_id,
    const std::string client_id,
    const std::string client_secret,
    const std::string& oauth_consumer_name,
    ProfileOAuth2TokenService* token_service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const identity::ScopeSet& scopes,
    TokenCallback callback,
    Mode mode)
    : OAuth2AccessTokenManager::Consumer(oauth_consumer_name),
      account_id_(account_id),
      client_id_(client_id),
      client_secret_(client_secret),
      token_service_(token_service),
      url_loader_factory_(std::move(url_loader_factory)),
      scopes_(scopes),
      mode_(mode),
      callback_(std::move(callback)) {
  DCHECK(client_id_.empty() == client_secret_.empty());
  DCHECK(client_id_.empty() || !url_loader_factory);

  if (mode_ == Mode::kImmediate || IsRefreshTokenAvailable()) {
    StartAccessTokenRequest();
    return;
  }

  // Start observing the IdentityManager. This observer will be removed either
  // when a refresh token is obtained and an access token request is started or
  // when this object is destroyed.
  token_service_observer_.Add(token_service_);
}

AccessTokenFetcher::~AccessTokenFetcher() {}

bool AccessTokenFetcher::IsRefreshTokenAvailable() const {
  DCHECK_EQ(Mode::kWaitUntilRefreshTokenAvailable, mode_);

  return token_service_->RefreshTokenIsAvailable(account_id_);
}

void AccessTokenFetcher::StartAccessTokenRequest() {
  DCHECK(mode_ == Mode::kImmediate || IsRefreshTokenAvailable());

  // By the time of starting an access token request, we should no longer be
  // listening for signin-related events.
  DCHECK(!token_service_observer_.IsObserving(token_service_));

  // Note: We might get here even in cases where we know that there's no refresh
  // token. We're requesting an access token anyway, so that the token service
  // will generate an appropriate error code that we can return to the client.
  DCHECK(!access_token_request_);

  // TODO(843510): Consider making the request to ProfileOAuth2TokenService
  // asynchronously once there are no direct clients of PO2TS (i.e., PO2TS is
  // used only by this class and IdentityManager).
  if (!client_id_.empty()) {
    // Setting both the client ID/secret and the URL loader factory is not
    // currently supported.
    access_token_request_ = token_service_->StartRequestForClient(
        account_id_, client_id_, client_secret_, scopes_, this);
    return;
  }

  if (url_loader_factory_) {
    access_token_request_ = token_service_->StartRequestWithContext(
        account_id_, url_loader_factory_, scopes_, this);
    return;
  }

  access_token_request_ =
      token_service_->StartRequest(account_id_, scopes_, this);
}

void AccessTokenFetcher::OnRefreshTokenAvailable(
    const CoreAccountId& account_id) {
  DCHECK_EQ(Mode::kWaitUntilRefreshTokenAvailable, mode_);

  if (!IsRefreshTokenAvailable())
    return;

  token_service_observer_.Remove(token_service_);

  StartAccessTokenRequest();
}

void AccessTokenFetcher::OnGetTokenSuccess(
    const OAuth2AccessTokenManager::Request* request,
    const OAuth2AccessTokenConsumer::TokenResponse& token_response) {
  DCHECK_EQ(request, access_token_request_.get());
  std::unique_ptr<OAuth2AccessTokenManager::Request> request_deleter(
      std::move(access_token_request_));

  RunCallbackAndMaybeDie(
      GoogleServiceAuthError::AuthErrorNone(),
      AccessTokenInfo(token_response.access_token,
                      token_response.expiration_time, token_response.id_token));

  // Potentially dead after the above invocation; nothing to do except return.
}

void AccessTokenFetcher::OnGetTokenFailure(
    const OAuth2AccessTokenManager::Request* request,
    const GoogleServiceAuthError& error) {
  DCHECK_EQ(request, access_token_request_.get());
  std::unique_ptr<OAuth2AccessTokenManager::Request> request_deleter(
      std::move(access_token_request_));

  RunCallbackAndMaybeDie(error, AccessTokenInfo());

  // Potentially dead after the above invocation; nothing to do except return.
}

void AccessTokenFetcher::RunCallbackAndMaybeDie(
    GoogleServiceAuthError error,
    AccessTokenInfo access_token_info) {
  // Per the contract of this class, it is allowed for consumers to delete this
  // object from within the callback that is run below. Hence, it is not safe to
  // add any code below this call.
  std::move(callback_).Run(std::move(error), std::move(access_token_info));
}

}  // namespace signin
