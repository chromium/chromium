// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_OAUTH_MULTILOGIN_TOKEN_REQUEST_H_
#define COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_OAUTH_MULTILOGIN_TOKEN_REQUEST_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/oauth2_access_token_manager.h"

namespace signin {

class OAuthMultiloginTokenResponse;

// Holds data required for performing an OAuth token request for a single
// account. Returned OAuth token might be either an access token or a refresh
// token.
// Destroying an instance will cancel the request.
class OAuthMultiloginTokenRequest : public OAuth2AccessTokenManager::Consumer {
 public:
  using Result =
      base::expected<OAuthMultiloginTokenResponse, GoogleServiceAuthError>;
  using Callback =
      base::OnceCallback<void(const OAuthMultiloginTokenRequest*, Result)>;

  OAuthMultiloginTokenRequest(const CoreAccountId& account_id,
                              Callback callback);

  OAuthMultiloginTokenRequest(const OAuthMultiloginTokenRequest&) = delete;
  OAuthMultiloginTokenRequest& operator=(const OAuthMultiloginTokenRequest&) =
      delete;

  ~OAuthMultiloginTokenRequest() override;

  const CoreAccountId& account_id() const { return account_id_; }

  // Starts fetching an access token with `scopes` from `manager` and invokes
  // `callback_` on completion.
  void StartAccessTokenRequest(
      OAuth2AccessTokenManager& manager,
      const OAuth2AccessTokenManager::ScopeSet& scopes);

  void InvokeCallbackWithResult(Result result);

  // OAuth2AccessTokenManager::Consumer:
  void OnGetTokenSuccess(
      const OAuth2AccessTokenManager::Request* request,
      const OAuth2AccessTokenConsumer::TokenResponse& token_response) override;
  void OnGetTokenFailure(const OAuth2AccessTokenManager::Request* request,
                         const GoogleServiceAuthError& error) override;

  base::WeakPtr<OAuthMultiloginTokenRequest> AsWeakPtr();

 private:
  const CoreAccountId account_id_;
  Callback callback_;

  std::unique_ptr<OAuth2AccessTokenManager::Request> access_token_request_;

  base::WeakPtrFactory<OAuthMultiloginTokenRequest> weak_ptr_factory_{this};
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_OAUTH_MULTILOGIN_TOKEN_REQUEST_H_
