// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_OAUTH_MULTILOGIN_HELPER_H_
#define COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_OAUTH_MULTILOGIN_HELPER_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/signin/internal/identity_manager/gaia_cookie_manager_service.h"
#include "components/signin/internal/identity_manager/oauth_multilogin_token_fetcher.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"

class GaiaAuthFetcher;
class GoogleServiceAuthError;
class ProfileOAuth2TokenService;
class SigninClient;

namespace signin {

enum class SetAccountsInCookieResult;

// This is a helper class that drives the OAuth multilogin process.
// The main steps are:
// - fetch access tokens with login scope,
// - call the oauth multilogin endpoint,
// - get the cookies from the response body, and set them in the cookie manager.
// It is safe to delete this object from within the callbacks.
class OAuthMultiloginHelper : public GaiaAuthConsumer {
 public:
  OAuthMultiloginHelper(
      SigninClient* signin_client,
      ProfileOAuth2TokenService* token_service,
      gaia::MultiloginMode mode,
      const std::vector<GaiaCookieManagerService::AccountIdGaiaIdPair>&
          accounts,
      const std::string& external_cc_result,
      base::OnceCallback<void(SetAccountsInCookieResult)> callback);

  ~OAuthMultiloginHelper() override;

 private:
  // Starts fetching tokens with OAuthMultiloginTokenFetcher.
  void StartFetchingTokens();

  // Callbacks for OAuthMultiloginTokenFetcher.
  void OnAccessTokensSuccess(
      const std::vector<OAuthMultiloginTokenFetcher::AccountIdTokenPair>&
          account_token_pairs);
  void OnAccessTokensFailure(const GoogleServiceAuthError& error);

  // Actual call to the multilogin endpoint.
  void StartFetchingMultiLogin();

  // Overridden from GaiaAuthConsumer.
  void OnOAuthMultiloginFinished(const OAuthMultiloginResult& result) override;

  // Starts setting parsed cookies in browser.
  void StartSettingCookies(const OAuthMultiloginResult& result);

  // Callback for CookieManager::SetCanonicalCookie.
  void OnCookieSet(const std::string& cookie_name,
                   const std::string& cookie_domain,
                   net::CanonicalCookie::CookieInclusionStatus status);

  SigninClient* signin_client_;
  ProfileOAuth2TokenService* token_service_;

  int fetcher_retries_ = 0;

  gaia::MultiloginMode mode_;
  // Account ids to set in the cookie.
  const std::vector<GaiaCookieManagerService::AccountIdGaiaIdPair> accounts_;
  // See GaiaCookieManagerService::ExternalCcResultFetcher for details.
  const std::string external_cc_result_;
  // Access tokens, in the same order as the account ids.
  std::vector<GaiaAuthFetcher::MultiloginTokenIDPair> gaia_id_token_pairs_;

  base::OnceCallback<void(SetAccountsInCookieResult)> callback_;
  std::unique_ptr<GaiaAuthFetcher> gaia_auth_fetcher_;
  std::unique_ptr<OAuthMultiloginTokenFetcher> token_fetcher_;

  // List of pairs (cookie name and cookie domain) that have to be set in
  // cookie jar.
  std::set<std::pair<std::string, std::string>> cookies_to_set_;

  base::WeakPtrFactory<OAuthMultiloginHelper> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(OAuthMultiloginHelper);
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_OAUTH_MULTILOGIN_HELPER_H_
