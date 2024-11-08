// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/oauth_multilogin_token_request.h"

#include "base/types/expected.h"
#include "components/signin/internal/identity_manager/oauth_multilogin_token_response.h"
#include "google_apis/gaia/oauth2_access_token_manager.h"

namespace signin {

OAuthMultiloginTokenRequest::OAuthMultiloginTokenRequest(
    const CoreAccountId& account_id,
    Callback callback)
    : OAuth2AccessTokenManager::Consumer("oauth_multilogin_token_request"),
      account_id_(account_id),
      callback_(std::move(callback)) {
  CHECK(callback_);
}

OAuthMultiloginTokenRequest::~OAuthMultiloginTokenRequest() = default;

void OAuthMultiloginTokenRequest::StartAccessTokenRequest(
    OAuth2AccessTokenManager& manager,
    const OAuth2AccessTokenManager::ScopeSet& scopes) {
  access_token_request_ = manager.StartRequest(account_id_, scopes, this);
}

void OAuthMultiloginTokenRequest::InvokeCallbackWithResult(Result result) {
  std::move(callback_).Run(this, std::move(result));
}

void OAuthMultiloginTokenRequest::OnGetTokenSuccess(
    const OAuth2AccessTokenManager::Request* request,
    const OAuth2AccessTokenConsumer::TokenResponse& token_response) {
  InvokeCallbackWithResult(
      OAuthMultiloginTokenResponse(token_response.access_token));
}

void OAuthMultiloginTokenRequest::OnGetTokenFailure(
    const OAuth2AccessTokenManager::Request* request,
    const GoogleServiceAuthError& error) {
  InvokeCallbackWithResult(base::unexpected(error));
}

base::WeakPtr<OAuthMultiloginTokenRequest>
OAuthMultiloginTokenRequest::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace signin
