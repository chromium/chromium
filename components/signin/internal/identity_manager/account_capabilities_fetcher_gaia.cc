// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/account_capabilities_fetcher_gaia.h"

#include <optional>

#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "components/signin/internal/identity_manager/account_capabilities_constants.h"
#include "components/signin/internal/identity_manager/account_info_util.h"
#include "components/signin/internal/identity_manager/profile_oauth2_token_service.h"
#include "components/signin/public/identity_manager/account_capabilities.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_access_token_consumer.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace {

constexpr std::string_view kAccountCapabilitiesFetcherHistogramBaseName =
    "Signin.AccountCapabilities";

std::string_view ToUmaToken(
    AccountCapabilitiesFetcher::FetchPriority priority) {
  switch (priority) {
    case AccountCapabilitiesFetcher::FetchPriority::kForeground:
      return "Foreground";
    case AccountCapabilitiesFetcher::FetchPriority::kBackground:
      return "Background";
  }
  NOTREACHED() << "Unknown priority: " << static_cast<int>(priority);
}

std::string_view ToUmaToken(
    AccountCapabilitiesFetcherGaia::FetchResult result) {
  if (result == AccountCapabilitiesFetcherGaia::FetchResult::kSuccess) {
    return "Success";
  } else {
    return "Failure";
  }
}

}  // namespace

AccountCapabilitiesFetcherGaia::AccountCapabilitiesFetcherGaia(
    ProfileOAuth2TokenService* token_service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const CoreAccountInfo& account_info,
    AccountCapabilitiesFetcher::FetchPriority fetch_priority,
    AccountCapabilitiesFetcher::OnCompleteCallback on_complete_callback)
    : AccountCapabilitiesFetcher(account_info,
                                 fetch_priority,
                                 std::move(on_complete_callback)),
      OAuth2AccessTokenManager::Consumer("account_capabilities_fetcher"),
      token_service_(token_service),
      url_loader_factory_(std::move(url_loader_factory)) {
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1("AccountFetcherService",
                                    "AccountCapabilitiesFetcherGaia", this,
                                    "account_id", account_id().ToString());
}

AccountCapabilitiesFetcherGaia::~AccountCapabilitiesFetcherGaia() {
  TRACE_EVENT_NESTABLE_ASYNC_END0("AccountFetcherService",
                                  "AccountCapabilitiesFetcherGaia", this);
  RecordFetchResultAndDuration(FetchResult::kCancelled);
}

void AccountCapabilitiesFetcherGaia::StartImpl() {
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("AccountFetcherService", "GetAccessToken",
                                    this);
  fetch_start_time_ = base::TimeTicks::Now();
  OAuth2AccessTokenManager::ScopeSet scopes;
  scopes.insert(GaiaConstants::kAccountCapabilitiesOAuth2Scope);
  login_token_request_ =
      token_service_->StartRequest(account_id(), scopes, this);
}

void AccountCapabilitiesFetcherGaia::OnGetTokenSuccess(
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
      AccountCapabilities::GetSupportedAccountCapabilityNames(), kMaxRetries,
      this);
}

void AccountCapabilitiesFetcherGaia::OnGetTokenFailure(
    const OAuth2AccessTokenManager::Request* request,
    const GoogleServiceAuthError& error) {
  TRACE_EVENT_NESTABLE_ASYNC_END1("AccountFetcherService", "GetAccessToken",
                                  this, "error", error.ToString());
  VLOG(1) << "OnGetTokenFailure: " << error.ToString();
  DCHECK_EQ(request, login_token_request_.get());
  login_token_request_.reset();
  RecordFetchResultAndDuration(FetchResult::kGetTokenFailure);
  CompleteFetchAndMaybeDestroySelf(std::nullopt);
}

void AccountCapabilitiesFetcherGaia::OnGetAccountCapabilitiesResponse(
    const base::Value::Dict& account_capabilities) {
  TRACE_EVENT_NESTABLE_ASYNC_END0("AccountFetcherService",
                                  "GetAccountCapabilities", this);
  std::optional<AccountCapabilities> parsed_capabilities =
      AccountCapabilitiesFromValue(account_capabilities);
  FetchResult result = FetchResult::kSuccess;
  if (!parsed_capabilities) {
    VLOG(1) << "Failed to parse account capabilities for " << account_id()
            << ". Response body: " << account_capabilities.DebugString();
    result = FetchResult::kParseResponseFailure;
  }

  RecordFetchResultAndDuration(result);
  CompleteFetchAndMaybeDestroySelf(parsed_capabilities);
}

void AccountCapabilitiesFetcherGaia::OnOAuthError() {
  TRACE_EVENT_NESTABLE_ASYNC_END1("AccountFetcherService",
                                  "GetAccountCapabilities", this, "error",
                                  "OAuthError");
  VLOG(1) << "OnOAuthError";
  RecordFetchResultAndDuration(FetchResult::kOAuthError);
  CompleteFetchAndMaybeDestroySelf(std::nullopt);
}

void AccountCapabilitiesFetcherGaia::OnNetworkError(int response_code) {
  TRACE_EVENT_NESTABLE_ASYNC_END2(
      "AccountFetcherService", "GetAccountCapabilities", this, "error",
      "NetworkError", "response_code", response_code);
  VLOG(1) << "OnNetworkError " << response_code;
  RecordFetchResultAndDuration(FetchResult::kNetworkError);
  CompleteFetchAndMaybeDestroySelf(std::nullopt);
}

void AccountCapabilitiesFetcherGaia::RecordFetchResultAndDuration(
    FetchResult result) {
  if (fetch_histograms_recorded_) {
    // Record histograms only once.
    return;
  }
  fetch_histograms_recorded_ = true;

  base::UmaHistogramEnumeration(
      base::JoinString({kAccountCapabilitiesFetcherHistogramBaseName,
                        ToUmaToken(fetch_priority()), "FetchResult"},
                       "."),
      result);

  if (fetch_start_time_.is_null()) {
    // Cannot record duration for a fetch that hasn't started.
    DCHECK_EQ(result, FetchResult::kCancelled);
    return;
  }
  base::TimeDelta duration = base::TimeTicks::Now() - fetch_start_time_;

  base::UmaHistogramMediumTimes(
      base::JoinString(
          {kAccountCapabilitiesFetcherHistogramBaseName,
           ToUmaToken(fetch_priority()), "FetchDuration", ToUmaToken(result)},
          "."),
      duration);
}
