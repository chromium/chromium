// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/account_info_fetcher_gaia.h"

#include <memory>
#include <utility>

#include "base/trace_event/trace_event.h"
#include "components/signin/internal/identity_manager/account_info_util.h"
#include "components/signin/internal/identity_manager/profile_oauth2_token_service.h"
#include "google_apis/gaia/gaia_constants.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"

AccountInfoFetcherGaia::AccountInfoFetcherGaia(
    ProfileOAuth2TokenService* token_service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const CoreAccountId& account_id,
    base::OnceCallback<void(std::optional<AccountInfo>)> callback)
    : OAuth2AccessTokenManager::Consumer("gaia_account_tracker"),
      token_service_(token_service),
      url_loader_factory_(std::move(url_loader_factory)),
      account_id_(account_id),
      callback_(std::move(callback)) {
  TRACE_EVENT_BEGIN("AccountFetcherService", "AccountIdFetcher",
                    perfetto::Track::FromPointer(this), "account_id",
                    account_id.ToString());

  Start();
}

AccountInfoFetcherGaia::~AccountInfoFetcherGaia() {
  TRACE_EVENT_END("AccountFetcherService",
                  /* AccountIdFetcher */ perfetto::Track::FromPointer(this));
}

void AccountInfoFetcherGaia::Start() {
  OAuth2AccessTokenManager::ScopeSet scopes;
  scopes.insert(GaiaConstants::kGoogleUserInfoEmail);
  scopes.insert(GaiaConstants::kGoogleUserInfoProfile);
  login_token_request_ =
      token_service_->StartRequest(account_id_, scopes, this);
}

void AccountInfoFetcherGaia::OnGetTokenSuccess(
    const OAuth2AccessTokenManager::Request* request,
    const OAuth2AccessTokenConsumer::TokenResponse& token_response) {
  TRACE_EVENT_INSTANT("AccountFetcherService", "OnGetTokenSuccess",
                      perfetto::Track::FromPointer(this));
  DCHECK_EQ(request, login_token_request_.get());

  gaia_oauth_client_ =
      std::make_unique<gaia::GaiaOAuthClient>(url_loader_factory_);
  const int kMaxRetries = 3;
  gaia_oauth_client_->GetUserInfo(token_response.access_token, kMaxRetries,
                                  this);
}

void AccountInfoFetcherGaia::OnGetTokenFailure(
    const OAuth2AccessTokenManager::Request* request,
    const GoogleServiceAuthError& error) {
  TRACE_EVENT_INSTANT("AccountFetcherService", "OnGetTokenFailure",
                      perfetto::Track::FromPointer(this),
                      "google_service_auth_error", error.ToString());
  LOG(ERROR) << "OnGetTokenFailure: " << error.ToString();
  DCHECK_EQ(request, login_token_request_.get());
  std::move(callback_).Run(std::nullopt);
}

void AccountInfoFetcherGaia::OnGetUserInfoResponse(
    const base::DictValue& user_info) {
  TRACE_EVENT_INSTANT("AccountFetcherService", "OnGetUserInfoResponse",
                      perfetto::Track::FromPointer(this), "account_id",
                      account_id_.ToString());
  std::move(callback_).Run(signin::AccountInfoFromUserInfo(user_info));
}

void AccountInfoFetcherGaia::OnOAuthError() {
  TRACE_EVENT_INSTANT("AccountFetcherService", "OnOAuthError",
                      perfetto::Track::FromPointer(this));
  LOG(ERROR) << "OnOAuthError";
  std::move(callback_).Run(std::nullopt);
}

void AccountInfoFetcherGaia::OnNetworkError(int response_code) {
  TRACE_EVENT_INSTANT("AccountFetcherService", "OnNetworkError",
                      perfetto::Track::FromPointer(this), "response_code",
                      response_code);
  LOG(ERROR) << "OnNetworkError " << response_code;
  std::move(callback_).Run(std::nullopt);
}
