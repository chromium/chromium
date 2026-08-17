// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/one_time_tokens/core/browser/user_data_processing_consent_fetcher.h"

#include <optional>
#include <string>

#include "base/check.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "components/one_time_tokens/core/browser/fetch_user_data_processing_consent_response.pb.h"
#include "components/one_time_tokens/core/browser/one_time_token_log_sink.h"
#include "components/one_time_tokens/core/browser/one_time_token_service_constants.h"
#include "components/one_time_tokens/core/common/one_time_token_switches.h"
#include "components/signin/public/base/oauth_consumer_id.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "net/base/url_util.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace one_time_tokens {

namespace {

namespace proto = ::google::internal::chrome::passwords::onetimetoken::v1;

ConsentState MapConsentState(proto::UserDataProcessingConsentState state) {
  switch (state) {
    case proto::USER_DATA_PROCESSING_CONSENT_STATE_UNDEFINED:
      return ConsentState::kUndefined;
    case proto::USER_DATA_PROCESSING_CONSENT_STATE_UNKNOWN:
      return ConsentState::kUnknown;
    case proto::USER_DATA_PROCESSING_CONSENT_STATE_ENABLED:
      return ConsentState::kEnabled;
    case proto::USER_DATA_PROCESSING_CONSENT_STATE_DISABLED:
      return ConsentState::kDisabled;
    default:
      return ConsentState::kUnknown;
  }
}

std::optional<UserDataProcessingConsentStates> ParseResponse(
    const std::string& response_body) {
  proto::FetchUserDataProcessingConsentResponse response;
  if (!response.ParseFromString(response_body)) {
    return std::nullopt;
  }
  return UserDataProcessingConsentStates{
      .comms_apps = MapConsentState(response.comms_apps()),
      .google_apps = MapConsentState(response.google_apps()),
  };
}

}  // namespace

UserDataProcessingConsentFetcher::UserDataProcessingConsentFetcher(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager& identity_manager,
    OneTimeTokenLogSink* log_sink)
    : url_loader_factory_(std::move(url_loader_factory)),
      identity_manager_(identity_manager),
      log_sink_(log_sink) {}

UserDataProcessingConsentFetcher::~UserDataProcessingConsentFetcher() = default;

void UserDataProcessingConsentFetcher::Start(Callback callback) {
  DCHECK(!callback_);
  LOG_OTT(log_sink_) << "Starting UserDataProcessingConsentFetcher";
  callback_ = std::move(callback);
  StartAccessTokenFetch();
}

void UserDataProcessingConsentFetcher::StartAccessTokenFetch() {
  access_token_fetcher_ =
      std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
          signin::OAuthConsumerId::kOneTimeTokenService, &*identity_manager_,
          base::BindOnce(
              &UserDataProcessingConsentFetcher::OnAccessTokenFetched,
              weakptr_factory_.GetWeakPtr()),
          signin::PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable,
          signin::ConsentLevel::kSignin);
}

void UserDataProcessingConsentFetcher::OnAccessTokenFetched(
    GoogleServiceAuthError error,
    signin::AccessTokenInfo info) {
  access_token_fetcher_.reset();
  if (error.state() == GoogleServiceAuthError::NONE) {
    LOG_OTT(log_sink_) << "Access token fetched successfully.";
    StartNetworkRequest(std::move(info));
    return;
  }
  LOG_OTT(log_sink_) << "Failed to fetch access token: " << error.ToString();
  std::move(callback_).Run(std::nullopt);
}

void UserDataProcessingConsentFetcher::StartNetworkRequest(
    signin::AccessTokenInfo info) {
  auto resource_request = std::make_unique<network::ResourceRequest>();

  GURL url = GetOneTimeTokenServiceUrl(
      "v1/onetimetokens:fetchUserDataProcessingConsent");

  url = net::AppendQueryParameter(url, "alt", "proto");

  LOG_OTT(log_sink_) << "Sending consent request to: " << url.spec();

  resource_request->url = url;
  resource_request->method = "GET";
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kAuthorization,
                                      "Bearer " + info.token);
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kAccept,
                                      "application/x-protobuf");
  // Set user-facing criticality header.
  resource_request->headers.SetHeader(
      kOneTimeTokenServiceCriticalityHeaderName,
      kOneTimeTokenServiceCriticalityHeaderValue);

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("fetch_user_data_processing_consent",
                                          R"(
        semantics {
          sender: "Gmail OTP filling by Gemini Live in Chrome."
          description:
            "Fetches the user's consent state to determine if Gemini Live is "
            "allowed to retrieve and fill One-Time Passwords (OTPs) from the "
            "user's Gmail."
          trigger:
            "When a one-time password (OTP) input field is detected on a "
            "form, and Chrome needs to verify if the user has consented to "
            "OTP data processing."
          data:
            "An OAuth2 access token used to authenticate the request. No other "
            "user data is sent."
          destination: GOOGLE_OWNED_SERVICE
          last_reviewed: "2026-08-04"
          internal {
            contacts {
              owners: "//components/one_time_tokens/OWNERS"
            }
          }
          user_data {
            type: ACCESS_TOKEN
          }
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature can be disabled by turning off Gemini in "
            "chrome://settings/ai/gemini."
          chrome_policy {
            GeminiActOnWebSettings {
              GeminiActOnWebSettings: 1
            }
          }
        })");

  simple_url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);

  simple_url_loader_->SetTimeoutDuration(base::Seconds(3));
  simple_url_loader_->SetRetryOptions(
      2, network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE |
             network::SimpleURLLoader::RETRY_ON_5XX);
  simple_url_loader_->SetAllowHttpErrorResults(true);

  simple_url_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&UserDataProcessingConsentFetcher::OnResponseBytesReceived,
                     weakptr_factory_.GetWeakPtr()),
      network::SimpleURLLoader::kMaxBoundedStringDownloadSize);
}

void UserDataProcessingConsentFetcher::OnResponseBytesReceived(
    std::optional<std::string> response_body) {
  std::optional<int> response_code;
  if (simple_url_loader_->ResponseInfo() &&
      simple_url_loader_->ResponseInfo()->headers) {
    response_code =
        simple_url_loader_->ResponseInfo()->headers->response_code();
  }
  simple_url_loader_.reset();

  if (response_code == net::HTTP_OK && response_body.has_value()) {
    std::optional<UserDataProcessingConsentStates> states =
        ParseResponse(*response_body);
    if (states.has_value()) {
      LOG_OTT(log_sink_) << "Consent request succeeded.";
      std::move(callback_).Run(std::move(states));
      return;
    }
    LOG_OTT(log_sink_) << "Consent request failed: Invalid response body.";
    std::move(callback_).Run(std::nullopt);
    return;
  }

  LOG_OTT(log_sink_) << "Consent request failed. Response code: "
                     << (response_code ? base::NumberToString(*response_code)
                                       : "N/A");
  std::move(callback_).Run(std::nullopt);
}

}  // namespace one_time_tokens
