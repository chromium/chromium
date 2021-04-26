// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_UBERTOKEN_FETCHER_IMPL_H_
#define COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_UBERTOKEN_FETCHER_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "base/timer/timer.h"
#include "components/signin/internal/identity_manager/profile_oauth2_token_service.h"
#include "components/signin/public/identity_manager/ubertoken_fetcher.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"

// Allow to retrieves an uber-auth token for the user. This class uses the
// |ProfileOAuth2TokenService| and considers that the user is already logged in.
// It will use the OAuth2 access token to generate the uber-auth token.
//
// This class should be used on a single thread, but it can be whichever thread
// that you like.
//
// This class can handle one request at a time.

class GoogleServiceAuthError;

namespace network {
class SharedURLLoaderFactory;
}

namespace signin {

using GaiaAuthFetcherFactory =
    base::RepeatingCallback<std::unique_ptr<GaiaAuthFetcher>(
        GaiaAuthConsumer*)>;

// Allows to retrieve an uber-auth token.
class UbertokenFetcherImpl : public UbertokenFetcher,
                             public GaiaAuthConsumer,
                             public OAuth2AccessTokenManager::Consumer {
 public:
  // Maximum number of retries to get the uber-auth token before giving up.
  static const int kMaxRetries;

  using CompletionCallback =
      base::OnceCallback<void(GoogleServiceAuthError error,
                              const std::string& token)>;

  // Constructs an instance and starts fetching the access token and ubertoken
  // sequentially for |account_id|. Uses a default GaiaAuthFetcherFactory which
  // returns base GaiaAuthFetcher instances.
  UbertokenFetcherImpl(
      const CoreAccountId& account_id,
      ProfileOAuth2TokenService* token_service,
      CompletionCallback ubertoken_callback,
      gaia::GaiaSource source,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  // Constructs an instance and starts fetching the ubertoken for |account_id|.
  UbertokenFetcherImpl(const CoreAccountId& account_id,
                       const std::string& access_token,
                       ProfileOAuth2TokenService* token_service,
                       CompletionCallback ubertoken_callback,
                       GaiaAuthFetcherFactory factory);
  ~UbertokenFetcherImpl() override;

  // Overridden from GaiaAuthConsumer
  void OnUberAuthTokenSuccess(const std::string& token) override;
  void OnUberAuthTokenFailure(const GoogleServiceAuthError& error) override;

  // Overridden from OAuth2AccessTokenManager::Consumer:
  void OnGetTokenSuccess(
      const OAuth2AccessTokenManager::Request* request,
      const OAuth2AccessTokenConsumer::TokenResponse& token_response) override;
  void OnGetTokenFailure(const OAuth2AccessTokenManager::Request* request,
                         const GoogleServiceAuthError& error) override;

 private:
  // Request a login-scoped access token from the token service.
  void RequestAccessToken();

  // Exchanges an oauth2 access token for an uber-auth token.
  void ExchangeTokens();

  ProfileOAuth2TokenService* token_service_;
  CompletionCallback ubertoken_callback_;
  GaiaAuthFetcherFactory gaia_auth_fetcher_factory_;
  std::unique_ptr<GaiaAuthFetcher> gaia_auth_fetcher_;
  std::unique_ptr<OAuth2AccessTokenManager::Request> access_token_request_;
  CoreAccountId account_id_;
  std::string access_token_;
  int retry_number_;
  base::OneShotTimer retry_timer_;
  bool second_access_token_request_;

  DISALLOW_COPY_AND_ASSIGN(UbertokenFetcherImpl);
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_UBERTOKEN_FETCHER_IMPL_H_
