// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_CREDENTIAL_PROVIDER_SIGNIN_INFO_FETCHER_WIN_H_
#define CHROME_BROWSER_UI_STARTUP_CREDENTIAL_PROVIDER_SIGNIN_INFO_FETCHER_WIN_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/values.h"
#include "google_apis/gaia/gaia_oauth_client.h"
#include "google_apis/gaia/oauth2_access_token_fetcher.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

// Helper class used to query additional information needed to sign in or
// create a new user through the Google Credential Provider for Windows. The
// additional information needed is:
// - User's full name.
// - ID token used for Mobile Device Management (MDM) registration.
// - A token handle for the user's refresh token.
// - Scoped down access token from login scoped access token.
// A separate OAuth request is required for each piece of information and
// each result arrives asynchronously so to gather all the results until they
// have all been fetched or there is an error. Once one of the two conditions
// are met notify the callback of the results.
class CredentialProviderSigninInfoFetcher
    : public gaia::GaiaOAuthClient::Delegate,
      public OAuth2AccessTokenConsumer {
 public:
  // Callback signalled when the fetch of all necessary information for the GCPW
  // is finished successfully or with an error.
  // The single argument should always be a dictionary value and will be empty
  // if there was an error during the fetch.
  using FetchCompletionCallback = base::OnceCallback<void(base::Value::Dict)>;

  CredentialProviderSigninInfoFetcher(
      const std::string& refresh_token,
      const std::string& consumer_name,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  CredentialProviderSigninInfoFetcher(
      const CredentialProviderSigninInfoFetcher&) = delete;
  CredentialProviderSigninInfoFetcher& operator=(
      const CredentialProviderSigninInfoFetcher&) = delete;
  ~CredentialProviderSigninInfoFetcher() override;

  void SetCompletionCallbackAndStart(
      const std::string& access_token,
      const std::string& additional_mdm_oauth_scopes,
      FetchCompletionCallback completion_callback);

  // gaia::GaiaOAuthClient::Delegate:
  void OnGetTokenInfoResponse(const base::Value::Dict& token_info) override;
  void OnGetUserInfoResponse(const base::Value::Dict& user_info) override;
  void OnOAuthError() override;
  void OnNetworkError(int response_code) override;

  // OAuth2AccessTokenConsumer:
  void OnGetTokenSuccess(const TokenResponse& token_response) override;
  void OnGetTokenFailure(const GoogleServiceAuthError& error) override;
  std::string GetConsumerName() const override;

 protected:
  void RequestUserInfoFromAccessToken(const std::string& access_token);
  void WriteResultsIfFinished(bool has_error);

  // This callback is triggered once all fetch requests have completed
  // successfully or there was an error in one of the fetch requests.
  FetchCompletionCallback completion_callback_;

  std::string token_handle_;
  std::string full_name_;
  std::string picture_url_;
  std::string mdm_id_token_;
  std::string mdm_access_token_;
  const std::string consumer_name_;

  std::unique_ptr<OAuth2AccessTokenFetcher> scoped_access_token_fetcher_;
  std::unique_ptr<gaia::GaiaOAuthClient> user_info_fetcher_;
  std::unique_ptr<gaia::GaiaOAuthClient> token_handle_fetcher_;
};

#endif  // CHROME_BROWSER_UI_STARTUP_CREDENTIAL_PROVIDER_SIGNIN_INFO_FETCHER_WIN_H_
