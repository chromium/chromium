// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_ACCOUNT_CAPABILITIES_FETCHER_GAIA_H_
#define COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_ACCOUNT_CAPABILITIES_FETCHER_GAIA_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "components/signin/internal/identity_manager/account_capabilities_fetcher.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "google_apis/gaia/gaia_oauth_client.h"
#include "google_apis/gaia/oauth2_access_token_manager.h"

namespace network {
class SharedURLLoaderFactory;
}

class ProfileOAuth2TokenService;
class GoogleServiceAuthError;

class AccountCapabilitiesFetcherGaia
    : public AccountCapabilitiesFetcher,
      public OAuth2AccessTokenManager::Consumer,
      public gaia::GaiaOAuthClient::Delegate {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class FetchResult {
    kSuccess = 0,
    kGetTokenFailure = 1,
    kParseResponseFailure = 2,
    kOAuthError = 3,
    kNetworkError = 4,
    kCancelled = 5,
    kMaxValue = kCancelled
  };

  AccountCapabilitiesFetcherGaia(
      ProfileOAuth2TokenService* token_service,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const CoreAccountInfo& account_info,
      AccountCapabilitiesFetcher::FetchPriority fetch_priority,
      AccountCapabilitiesFetcher::OnCompleteCallback on_complete_callback);
  ~AccountCapabilitiesFetcherGaia() override;

  AccountCapabilitiesFetcherGaia(const AccountCapabilitiesFetcherGaia&) =
      delete;
  AccountCapabilitiesFetcherGaia& operator=(
      const AccountCapabilitiesFetcherGaia&) = delete;

  // OAuth2AccessTokenManager::Consumer:
  void OnGetTokenSuccess(
      const OAuth2AccessTokenManager::Request* request,
      const OAuth2AccessTokenConsumer::TokenResponse& token_response) override;
  void OnGetTokenFailure(const OAuth2AccessTokenManager::Request* request,
                         const GoogleServiceAuthError& error) override;

  // gaia::GaiaOAuthClient::Delegate:
  void OnGetAccountCapabilitiesResponse(
      const base::Value::Dict& account_capabilities) override;
  void OnOAuthError() override;
  void OnNetworkError(int response_code) override;

 protected:
  // AccountCapabilitiesFetcher:
  void StartImpl() override;

 private:
  void RecordFetchResultAndDuration(FetchResult result);

  raw_ptr<ProfileOAuth2TokenService> token_service_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  std::unique_ptr<OAuth2AccessTokenManager::Request> login_token_request_;
  std::unique_ptr<gaia::GaiaOAuthClient> gaia_oauth_client_;

  // Used for metrics:
  base::TimeTicks fetch_start_time_;
  bool fetch_histograms_recorded_ = false;
};

#endif  // COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_ACCOUNT_CAPABILITIES_FETCHER_GAIA_H_
