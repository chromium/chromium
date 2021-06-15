// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/account_capabilities_fetcher.h"

#include "base/trace_event/trace_event.h"
#include "components/signin/internal/identity_manager/account_capabilities_constants.h"
#include "components/signin/internal/identity_manager/account_fetcher_service.h"
#include "components/signin/internal/identity_manager/account_info_util.h"
#include "components/signin/internal/identity_manager/profile_oauth2_token_service.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_access_token_consumer.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

AccountCapabilitiesFetcher::AccountCapabilitiesFetcher(
    ProfileOAuth2TokenService* token_service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    AccountFetcherService* service,
    const CoreAccountId& account_id)
    : OAuth2AccessTokenManager::Consumer("account_capabilities_fetcher"),
      token_service_(token_service),
      url_loader_factory_(std::move(url_loader_factory)),
      service_(service),
      account_id_(account_id) {
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1("AccountFetcherService",
                                    "AccountCapabilitiesFetcher", this,
                                    "account_id", account_id.ToString());
}

AccountCapabilitiesFetcher::~AccountCapabilitiesFetcher() {
  TRACE_EVENT_NESTABLE_ASYNC_END0("AccountFetcherService",
                                  "AccountCapabilitiesFetcher", this);
}

void AccountCapabilitiesFetcher::Start() {
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("AccountFetcherService", "GetAccessToken",
                                    this);
  OAuth2AccessTokenManager::ScopeSet scopes;
  scopes.insert(GaiaConstants::kAccountCapabilitiesOAuth2Scope);
  login_token_request_ =
      token_service_->StartRequest(account_id_, scopes, this);
}

void AccountCapabilitiesFetcher::OnGetTokenSuccess(
    const OAuth2AccessTokenManager::Request* request,
    const OAuth2AccessTokenConsumer::TokenResponse& token_response) {
  TRACE_EVENT_NESTABLE_ASYNC_END0("AccountFetcherService", "GetAccessToken",
                                  this);
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("AccountFetcherService",
                                    "GetAccountCapabilities", this);
  DCHECK_EQ(request, login_token_request_.get());
  login_token_request_.reset();

  gaia_oauth_client_ =
      std::make_unique<gaia::GaiaOAuthClient>(url_loader_factory_);
  const int kMaxRetries = 3;
  gaia_oauth_client_->GetAccountCapabilities(
      token_response.access_token,
      {kCanOfferExtendedChromeSyncPromosCapabilityName}, kMaxRetries, this);
}

void AccountCapabilitiesFetcher::OnGetTokenFailure(
    const OAuth2AccessTokenManager::Request* request,
    const GoogleServiceAuthError& error) {
  TRACE_EVENT_NESTABLE_ASYNC_END1("AccountFetcherService", "GetAccessToken",
                                  this, "error", error.ToString());
  VLOG(1) << "OnGetTokenFailure: " << error.ToString();
  DCHECK_EQ(request, login_token_request_.get());
  login_token_request_.reset();
  service_->OnAccountCapabilitiesFetchFailure(account_id_);
}

void AccountCapabilitiesFetcher::OnGetAccountCapabilitiesResponse(
    std::unique_ptr<base::Value> account_capabilities) {
  TRACE_EVENT_NESTABLE_ASYNC_END0("AccountFetcherService",
                                  "GetAccountCapabilities", this);
  absl::optional<AccountCapabilities> parsed_capabilities =
      AccountCapabilitiesFromValue(*account_capabilities);
  if (!parsed_capabilities) {
    VLOG(1) << "Failed to parse account capabilities for " << account_id_
            << ". Response body: " << account_capabilities->DebugString();
    service_->OnAccountCapabilitiesFetchFailure(account_id_);
    return;
  }

  service_->OnAccountCapabilitiesFetchSuccess(account_id_,
                                              parsed_capabilities.value());
}

void AccountCapabilitiesFetcher::OnOAuthError() {
  TRACE_EVENT_NESTABLE_ASYNC_END1("AccountFetcherService",
                                  "GetAccountCapabilities", this, "error",
                                  "OAuthError");
  VLOG(1) << "OnOAuthError";
  service_->OnAccountCapabilitiesFetchFailure(account_id_);
}

void AccountCapabilitiesFetcher::OnNetworkError(int response_code) {
  TRACE_EVENT_NESTABLE_ASYNC_END2(
      "AccountFetcherService", "GetAccountCapabilities", this, "error",
      "NetworkError", "response_code", response_code);
  VLOG(1) << "OnNetworkError " << response_code;
  service_->OnAccountCapabilitiesFetchFailure(account_id_);
}
