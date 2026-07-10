// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/one_time_tokens/core/browser/email_one_time_token_fetcher.h"

#include <optional>
#include <string_view>

#include "base/base64url.h"
#include "base/metrics/histogram_functions.h"
#include "components/one_time_tokens/core/browser/email_one_time_token_fetch_error_converter.h"
#include "components/one_time_tokens/core/browser/fetch_email_one_time_token_error_details.pb.h"
#include "components/one_time_tokens/core/browser/fetch_email_one_time_token_request.pb.h"
#include "components/one_time_tokens/core/browser/fetch_email_one_time_token_response.pb.h"
#include "components/one_time_tokens/core/browser/one_time_token.h"
#include "components/one_time_tokens/core/browser/one_time_token_retrieval_error.h"
#include "components/one_time_tokens/core/browser/one_time_token_type.h"
#include "components/one_time_tokens/core/common/one_time_token_features.h"
#include "components/signin/public/base/oauth_consumer_id.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace one_time_tokens {

namespace {

using FetchEmailOneTimeTokenErrorDetails = ::google::internal::chrome::
    passwords::onetimetoken::v1::FetchEmailOneTimeTokenErrorDetails;
using RpcStatus =
    ::google::internal::chrome::passwords::onetimetoken::v1::RpcStatus;
using FetchEmailOneTimeTokenResponse = ::google::internal::chrome::passwords::
    onetimetoken::v1::FetchEmailOneTimeTokenResponse;
using Proto3Any =
    ::google::internal::chrome::passwords::onetimetoken::v1::Proto3Any;

constexpr std::string_view kFetchEmailOneTimeTokenErrorDetailsTypeUrl =
    "type.googleapis.com/google.internal.chrome.passwords.onetimetoken.v1."
    "FetchEmailOneTimeTokenErrorDetails";

std::optional<FetchEmailOneTimeTokenErrorDetails>
GetFirstFoundOneTimeTokenErrorDetails(const RpcStatus& rpc_status) {
  for (const Proto3Any& detail : rpc_status.details()) {
    if (detail.type_url() != kFetchEmailOneTimeTokenErrorDetailsTypeUrl) {
      continue;
    }

    FetchEmailOneTimeTokenErrorDetails error_details;
    if (error_details.ParseFromString(detail.value()) &&
        error_details.reason_code_size() > 0) {
      return error_details;
    }
  }
  return std::nullopt;
}

base::expected<OneTimeToken, OneTimeTokenRetrievalError>
ExtractOneTimeTokenValueFromResponse(const std::string& response_body) {
  FetchEmailOneTimeTokenResponse response;
  if (!response.ParseFromString(response_body)) {
    return base::unexpected(
        OneTimeTokenRetrievalError::kGmailOtpBackendInvalidResponse);
  }
  if (!response.has_one_time_password()) {
    return base::unexpected(
        OneTimeTokenRetrievalError::kGmailOtpBackendInvalidResponse);
  }
  if (response.sender_address().empty()) {
    return base::unexpected(
        OneTimeTokenRetrievalError::kGmailOtpBackendInvalidResponse);
  }
  return base::ok(OneTimeToken(OneTimeTokenType::kGmail,
                               response.one_time_password().one_time_password(),
                               base::TimeTicks::Now(),
                               response.sender_address()));
}

std::optional<FetchEmailOneTimeTokenErrorDetails> ExtractBackendErrorDetails(
    const std::string& response_body) {
  RpcStatus rpc_status;

  if (!rpc_status.ParseFromString(response_body)) {
    return std::nullopt;
  }

  return GetFirstFoundOneTimeTokenErrorDetails(rpc_status);
}

}  // namespace

EmailOneTimeTokenFetcher::EmailOneTimeTokenFetcher(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager& identity_manager,
    std::string encrypted_message_reference)
    : url_loader_factory_(std::move(url_loader_factory)),
      identity_manager_(identity_manager),
      encrypted_message_reference_(std::move(encrypted_message_reference)) {}

EmailOneTimeTokenFetcher::~EmailOneTimeTokenFetcher() = default;

void EmailOneTimeTokenFetcher::Start(
    EmailOneTimeTokenFetcher::ServerResponseCallback callback) {
  callback_ = std::move(callback);
  StartAccessTokenFetch();
}

void EmailOneTimeTokenFetcher::InvokeCallbackAndDestroySelf(
    base::expected<OneTimeToken, OneTimeTokenRetrievalError> result) {
  std::move(callback_).Run(std::move(result));
}

void EmailOneTimeTokenFetcher::StartAccessTokenFetch() {
  access_token_fetcher_ =
      std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
          signin::OAuthConsumerId::kOneTimeTokenService, &*identity_manager_,
          base::BindOnce(&EmailOneTimeTokenFetcher::OnAccessTokenFetched,
                         weakptr_factory_.GetWeakPtr(), base::TimeTicks::Now()),
          signin::PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable,
          signin::ConsentLevel::kSignin);
}

void EmailOneTimeTokenFetcher::OnAccessTokenFetched(
    base::TimeTicks auth_start_time,
    GoogleServiceAuthError error,
    signin::AccessTokenInfo info) {
  base::UmaHistogramTimes("Autofill.OneTimeTokens.Backend.Gmail.AuthLatency",
                          base::TimeTicks::Now() - auth_start_time);
  access_token_fetcher_.reset();
  if (error.state() == GoogleServiceAuthError::NONE) {
    StartOneTimeTokenServiceCall(std::move(info));
    return;
  }
  InvokeCallbackAndDestroySelf(
      base::unexpected(OneTimeTokenRetrievalError::kGmailOtpBackendAuthError));
}

void EmailOneTimeTokenFetcher::StartOneTimeTokenServiceCall(
    signin::AccessTokenInfo info) {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  GURL url(features::kFetchEmailOneTimeTokenEndpointUrl.Get());

  // TODO(crbug.com/486806779): figure out correct encoding.
  std::string encoded_reference;
  base::Base64UrlEncode(encrypted_message_reference_,
                        base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                        &encoded_reference);
  url = net::AppendQueryParameter(url, "encryptedMessageReference",
                                  encoded_reference);
  url = net::AppendQueryParameter(url, "alt", "proto");
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
      net::DefineNetworkTrafficAnnotation("fetch_email_one_time_token", R"(
        semantics {
          sender: "Gmail OTP filling by Gemini Live in Chrome."
          description:
            "Sends a request to OneTimeTokenService to fetch an OTP that sits "
            "in user's email. At this point the OTP is already parsed on the "
            "backend and ready to be used."
          trigger:
            "A fillable OTP field is detected on a webpage and a notification "
            "about OTP is received."
          data:
            "An opaque (encrypted) reference to the message which contains the "
            "OTP. No user data is sent."
          destination: GOOGLE_OWNED_SERVICE
          last_reviewed: "2026-03-20"
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
            "These requests are sent only by Glic trying to fetch email OTP "
            "from user's Gmail. IT Admins can turn the feature off by "
            "disabling Glic using GeminiActOnWebSettings."
          chrome_policy {
            GeminiActOnWebSettings {
              GeminiActOnWebSettings: 1
            }
          }
        })");

  simple_url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);

  // Set timeout and retry options for user-facing traffic.
  simple_url_loader_->SetTimeoutDuration(base::Seconds(60));
  simple_url_loader_->SetRetryOptions(
      2, network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE |
             network::SimpleURLLoader::RETRY_ON_5XX);
  simple_url_loader_->SetAllowHttpErrorResults(true);

  simple_url_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(
          &EmailOneTimeTokenFetcher::OnResponseBytesFromOneTimeTokenService,
          weakptr_factory_.GetWeakPtr(), base::TimeTicks::Now()),
      network::SimpleURLLoader::kMaxBoundedStringDownloadSize);
}

void EmailOneTimeTokenFetcher::OnResponseBytesFromOneTimeTokenService(
    base::TimeTicks network_request_start_time,
    std::optional<std::string> response_body) {
  base::UmaHistogramTimes("Autofill.OneTimeTokens.Backend.Gmail.NetworkLatency",
                          base::TimeTicks::Now() - network_request_start_time);

  std::optional<int> response_code = GetHttpResponseCode();
  simple_url_loader_.reset();

  // Success case.
  if (response_code == net::HTTP_OK && response_body.has_value()) {
    base::expected<OneTimeToken, OneTimeTokenRetrievalError> result =
        ExtractOneTimeTokenValueFromResponse(*response_body);
    InvokeCallbackAndDestroySelf(std::move(result));
    return;
  }

  // Failure case: no response body.
  if (!response_body.has_value()) {
    InvokeCallbackAndDestroySelf(base::unexpected(
        OneTimeTokenRetrievalError::kGmailOtpBackendNetworkError));
    return;
  }

  std::optional<FetchEmailOneTimeTokenErrorDetails> error_details =
      ExtractBackendErrorDetails(*response_body);
  if (error_details.has_value()) {
    InvokeCallbackAndDestroySelf(base::unexpected(
        ConvertEmailOneTimeTokenFetchErrorDetails(error_details.value())));
  } else {
    InvokeCallbackAndDestroySelf(base::unexpected(
        OneTimeTokenRetrievalError::kGmailOtpBackendInvalidResponse));
  }
}

std::optional<int> EmailOneTimeTokenFetcher::GetHttpResponseCode() const {
  if (const auto* info = simple_url_loader_->ResponseInfo();
      info && info->headers) {
    return info->headers->response_code();
  }
  return std::nullopt;
}

}  // namespace one_time_tokens
