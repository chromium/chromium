// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_ACCOUNT_INFO_FETCHER_GAIA_H_
#define COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_ACCOUNT_INFO_FETCHER_GAIA_H_

#include <memory>
#include <optional>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "components/signin/internal/identity_manager/account_info_fetcher.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/gaia_oauth_client.h"
#include "google_apis/gaia/oauth2_access_token_manager.h"

namespace network {
class SharedURLLoaderFactory;
}

class ProfileOAuth2TokenService;

// An account information fetcher that gets an OAuth token of appropriate
// scope and uses it to fetch account information. This does not handle
// refreshing the information and is meant to be used in a one shot fashion.
// Fetching is started automatically as soon as an instance is created.
class AccountInfoFetcherGaia : public AccountInfoFetcher,
                               public OAuth2AccessTokenManager::Consumer,
                               public gaia::GaiaOAuthClient::Delegate {
 public:
  AccountInfoFetcherGaia(
      ProfileOAuth2TokenService* token_service,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const CoreAccountId& account_id,
      base::OnceCallback<void(std::optional<AccountInfo>)> callback);

  AccountInfoFetcherGaia(const AccountInfoFetcherGaia&) = delete;
  AccountInfoFetcherGaia& operator=(const AccountInfoFetcherGaia&) = delete;

  ~AccountInfoFetcherGaia() override;

  // OAuth2AccessTokenManager::Consumer implementation.
  void OnGetTokenSuccess(
      const OAuth2AccessTokenManager::Request* request,
      const OAuth2AccessTokenConsumer::TokenResponse& token_response) override;
  void OnGetTokenFailure(const OAuth2AccessTokenManager::Request* request,
                         const GoogleServiceAuthError& error) override;

  // gaia::GaiaOAuthClient::Delegate implementation.
  void OnGetUserInfoResponse(const base::DictValue& user_info) override;
  void OnOAuthError() override;
  void OnNetworkError(int response_code) override;

 private:
  // Start fetching the account information.
  void Start();

  raw_ptr<ProfileOAuth2TokenService> token_service_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  const CoreAccountId account_id_;
  base::OnceCallback<void(std::optional<AccountInfo>)> callback_;

  std::unique_ptr<OAuth2AccessTokenManager::Request> login_token_request_;
  std::unique_ptr<gaia::GaiaOAuthClient> gaia_oauth_client_;
};

#endif  // COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_ACCOUNT_INFO_FETCHER_GAIA_H_
