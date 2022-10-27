// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>
#include <vector>

#include "chrome/browser/ui/startup/credential_provider_signin_info_fetcher_win.h"

#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/syslog_logging.h"
#include "chrome/credential_provider/common/gcp_strings.h"
#include "google_apis/gaia/gaia_access_token_fetcher.h"
#include "google_apis/gaia/gaia_oauth_client.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/oauth2_access_token_fetcher.h"
#include "google_apis/google_api_keys.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

CredentialProviderSigninInfoFetcher::CredentialProviderSigninInfoFetcher(
    const std::string& refresh_token,
    const std::string& consumer_name,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : consumer_name_(consumer_name),
      scoped_access_token_fetcher_(
          GaiaAccessTokenFetcher::
              CreateExchangeRefreshTokenForAccessTokenInstance(
                  this,
                  url_loader_factory,
                  refresh_token)),
      user_info_fetcher_(
          std::make_unique<gaia::GaiaOAuthClient>(url_loader_factory)),
      token_handle_fetcher_(
          std::make_unique<gaia::GaiaOAuthClient>(url_loader_factory)) {}

CredentialProviderSigninInfoFetcher::~CredentialProviderSigninInfoFetcher() =
    default;

void CredentialProviderSigninInfoFetcher::SetCompletionCallbackAndStart(
    const std::string& access_token,
    const std::string& additional_mdm_oauth_scopes,
    FetchCompletionCallback completion_callback) {
  DCHECK(!completion_callback.is_null());
  completion_callback_ = std::move(completion_callback);
  GaiaUrls* gaia_urls = GaiaUrls::GetInstance();

  // Scopes needed to fetch the full name of the user as well as an id token
  // used for MDM registration.
  std::vector<std::string> access_scopes = {"email", "profile", "openid"};

  std::vector<std::string> extra_scopes(
      base::SplitString(additional_mdm_oauth_scopes, ",", base::TRIM_WHITESPACE,
                        base::SPLIT_WANT_NONEMPTY));
  access_scopes.insert(access_scopes.end(), extra_scopes.begin(),
                       extra_scopes.end());

  scoped_access_token_fetcher_->Start(gaia_urls->oauth2_chrome_client_id(),
                                      gaia_urls->oauth2_chrome_client_secret(),
                                      access_scopes);
  token_handle_fetcher_->GetTokenInfo(access_token, 0, this);
}

void CredentialProviderSigninInfoFetcher::OnGetTokenInfoResponse(
    const base::Value::Dict& token_info) {
  DCHECK(token_handle_.empty());
  if (const std::string* token_handle = token_info.FindString("token_handle");
      token_handle) {
    token_handle_ = *token_handle;
  }
  bool has_error = token_handle_.empty();
  WriteResultsIfFinished(has_error);
}

void CredentialProviderSigninInfoFetcher::OnGetUserInfoResponse(
    const base::Value::Dict& user_info) {
  DCHECK(!mdm_access_token_.empty());
  DCHECK(!mdm_id_token_.empty());
  DCHECK(full_name_.empty());
  if (const std::string* full_name = user_info.FindString("name"); full_name) {
    full_name_ = *full_name;
  }
  if (const std::string* picture_url = user_info.FindString("picture");
      picture_url) {
    picture_url_ = *picture_url;
  }
  bool has_error = full_name_.empty();
  WriteResultsIfFinished(has_error);
}

void CredentialProviderSigninInfoFetcher::OnOAuthError() {
  WriteResultsIfFinished(true);
}

void CredentialProviderSigninInfoFetcher::OnNetworkError(int response_code) {
  SYSLOG(ERROR) << "Network error occured while fetching token handle: "
                << response_code;
  WriteResultsIfFinished(true);
}

void CredentialProviderSigninInfoFetcher::OnGetTokenSuccess(
    const TokenResponse& token_response) {
  DCHECK(mdm_access_token_.empty());
  DCHECK(mdm_id_token_.empty());
  DCHECK(full_name_.empty());
  mdm_access_token_ = token_response.access_token;
  mdm_id_token_ = token_response.id_token;

  if (mdm_access_token_.empty() || mdm_id_token_.empty()) {
    WriteResultsIfFinished(true);
    return;
  }

  // Once a valid access token is given for fetching the user's full name, fire
  // off a request to get the user's info.
  RequestUserInfoFromAccessToken(token_response.access_token);
}

void CredentialProviderSigninInfoFetcher::OnGetTokenFailure(
    const GoogleServiceAuthError& error) {
  WriteResultsIfFinished(true);
}

std::string CredentialProviderSigninInfoFetcher::GetConsumerName() const {
  return consumer_name_;
}

void CredentialProviderSigninInfoFetcher::RequestUserInfoFromAccessToken(
    const std::string& access_token) {
  user_info_fetcher_->GetUserInfo(access_token, 0, this);
}

void CredentialProviderSigninInfoFetcher::WriteResultsIfFinished(
    bool has_error) {
  DCHECK(completion_callback_);

  if (!has_error && (mdm_access_token_.empty() || mdm_id_token_.empty() ||
                     full_name_.empty() || token_handle_.empty())) {
    return;
  }

  base::Value::Dict fetch_result;
  if (!has_error) {
    fetch_result.Set(credential_provider::kKeyMdmAccessToken,
                     mdm_access_token_);
    fetch_result.Set(credential_provider::kKeyMdmIdToken, mdm_id_token_);
    fetch_result.Set(credential_provider::kKeyFullname, full_name_);
    if (!picture_url_.empty()) {
      fetch_result.Set(credential_provider::kKeyPicture, picture_url_);
    }
    fetch_result.Set(credential_provider::kKeyTokenHandle, token_handle_);
  }

  std::move(completion_callback_).Run(std::move(fetch_result));

  scoped_access_token_fetcher_.reset();
  user_info_fetcher_.reset();
  token_handle_fetcher_.reset();
}
