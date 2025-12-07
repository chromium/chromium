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

#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/signin/internal/identity_manager/oauth_multilogin_token_request.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/google_service_auth_error.h"

class SigninClient;
class ProfileOAuth2TokenService;

namespace signin {

class OAuthMultiloginTokenResponse;

// Fetches multilogin access tokens in parallel for multiple accounts.
// It is safe to delete this object from within the callbacks.
class OAuthMultiloginTokenFetcher {
 public:
  using SuccessCallback = base::OnceCallback<void(
      base::flat_map<CoreAccountId, OAuthMultiloginTokenResponse>)>;
  using FailureCallback =
      base::OnceCallback<void(const GoogleServiceAuthError&)>;

  struct AccountParams {
    CoreAccountId account_id;
    std::string token_binding_challenge;
  };

  OAuthMultiloginTokenFetcher(SigninClient* signin_client,
                              ProfileOAuth2TokenService* token_service,
                              std::vector<AccountParams> account_params,
                              std::string ephemeral_public_key,
                              SuccessCallback success_callback,
                              FailureCallback failure_callback,
                              bool retry_waits_on_connectivity = true);

  OAuthMultiloginTokenFetcher(const OAuthMultiloginTokenFetcher&) = delete;
  OAuthMultiloginTokenFetcher& operator=(const OAuthMultiloginTokenFetcher&) =
      delete;

  ~OAuthMultiloginTokenFetcher();

  void OnTokenRequestComplete(const OAuthMultiloginTokenRequest* request,
                              OAuthMultiloginTokenRequest::Result result);

 private:
  void StartFetchingToken(const AccountParams& account);

  void TokenRequestSucceeded(const CoreAccountId& account_id,
                             OAuthMultiloginTokenResponse response);
  void TokenRequestFailed(const CoreAccountId& account_id,
                          GoogleServiceAuthError error);

  raw_ptr<SigninClient> signin_client_;
  raw_ptr<ProfileOAuth2TokenService> token_service_;
  const std::vector<AccountParams> account_params_;
  const std::string ephemeral_public_key_;

  SuccessCallback success_callback_;
  FailureCallback failure_callback_;

  std::vector<std::unique_ptr<OAuthMultiloginTokenRequest>> token_requests_;
  base::flat_map<CoreAccountId, OAuthMultiloginTokenResponse> token_responses_;
  std::set<CoreAccountId> retried_requests_;  // Requests are retried once.
  const bool retry_waits_on_connectivity_ = true;

  base::WeakPtrFactory<OAuthMultiloginTokenFetcher> weak_ptr_factory_{this};
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_OAUTH_MULTILOGIN_TOKEN_FETCHER_H_
