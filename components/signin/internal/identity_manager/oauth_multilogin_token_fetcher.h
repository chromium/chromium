// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_OAUTH_MULTILOGIN_TOKEN_FETCHER_H_
#define COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_OAUTH_MULTILOGIN_TOKEN_FETCHER_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_access_token_manager.h"

class SigninClient;
class ProfileOAuth2TokenService;

namespace signin {

// Fetches multilogin access tokens in parallel for multiple accounts.
// It is safe to delete this object from within the callbacks.
class OAuthMultiloginTokenFetcher : public OAuth2AccessTokenManager::Consumer {
 public:
  struct AccountIdTokenPair {
    CoreAccountId account_id;
    std::string token;

    AccountIdTokenPair(const CoreAccountId& account_id,
                       const std::string& token)
        : account_id(account_id), token(token) {}
  };
  using SuccessCallback =
      base::OnceCallback<void(const std::vector<AccountIdTokenPair>&)>;
  using FailureCallback =
      base::OnceCallback<void(const GoogleServiceAuthError&)>;

  OAuthMultiloginTokenFetcher(SigninClient* signin_client,
                              ProfileOAuth2TokenService* token_service,
                              const std::vector<CoreAccountId>& account_ids,
                              SuccessCallback success_callback,
                              FailureCallback failure_callback);

  OAuthMultiloginTokenFetcher(const OAuthMultiloginTokenFetcher&) = delete;
  OAuthMultiloginTokenFetcher& operator=(const OAuthMultiloginTokenFetcher&) =
      delete;

  ~OAuthMultiloginTokenFetcher() override;

 private:
  void StartFetchingToken(const CoreAccountId& account_id);

  // Overridden from OAuth2AccessTokenManager::Consumer.
  void OnGetTokenSuccess(
      const OAuth2AccessTokenManager::Request* request,
      const OAuth2AccessTokenConsumer::TokenResponse& token_response) override;
  void OnGetTokenFailure(const OAuth2AccessTokenManager::Request* request,
                         const GoogleServiceAuthError& error) override;

  // Helper function to remove a request from token_requests_.
  void EraseRequest(const OAuth2AccessTokenManager::Request* request);

  raw_ptr<SigninClient> signin_client_;
  raw_ptr<ProfileOAuth2TokenService> token_service_;
  const std::vector<CoreAccountId> account_ids_;

  SuccessCallback success_callback_;
  FailureCallback failure_callback_;

  std::vector<std::unique_ptr<OAuth2AccessTokenManager::Request>>
      token_requests_;
  std::map<CoreAccountId, std::string> access_tokens_;
  std::set<CoreAccountId> retried_requests_;  // Requests are retried once.

  base::WeakPtrFactory<OAuthMultiloginTokenFetcher> weak_ptr_factory_{this};
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_OAUTH_MULTILOGIN_TOKEN_FETCHER_H_
