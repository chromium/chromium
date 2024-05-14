// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/trusted_vault_request.h"

#include <utility>

#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/trusted_vault/trusted_vault_access_token_fetcher.h"
#include "components/trusted_vault/trusted_vault_server_constants.h"
#include "google_apis/credentials_mode.h"
#include "google_apis/gaia/core_account_id.h"
#include "net/base/backoff_entry.h"
#include "net/base/url_util.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace trusted_vault {

namespace {

const char kAuthorizationHeader[] = "Authorization";
const char kProtobufContentType[] = "application/x-protobuf";

constexpr net::BackoffEntry::Policy kRetryPolicy = {
    // Number of initial errors to ignore before starting to back off.
    0,

    // Initial delay in ms: 10 second.
    10000,

    // Factor by which the waiting time is multiplied.
    10,

    // Fuzzing percentage; this spreads delays randomly between 80% and 100%
    // of the calculated time.
    0.20,

    // Maximum amount of time we are willing to delay our request: 25 minutes.
    1000 * 60 * 25,

    // When to discard an entry: never.
    -1,

    // |always_use_initial_delay|; false means that the initial delay is
    // applied after the first error, and starts backing off from there.
    false,
};

net::NetworkTrafficAnnotationTag CreateTrafficAnnotationTag() {
  return net::DefineNetworkTrafficAnnotation("trusted_vault_request",
                                             R"(
      semantics {
        sender: "Trusted Vault Service"
        description:
          "Request to vault service in order to retrieve, change or support "
          "future retrieval or change of encryption keys for Chrome or "
          "Chrome OS features (such as Chrome Sync)."
        trigger:
          "Periodically/upon certain non-user controlled events after user "
          "signs in Chrome profile."
        data:
          "An OAuth2 access token, metadata associated with encryption keys: "
          "encrypted encryption keys, public counterpart of encryption keys."
        destination: GOOGLE_OWNED_SERVICE
      }
      policy {
        cookies_allowed: NO
        setting:
          "This feature cannot be disabled in settings, but if user signs "
          "out of Chrome, this request would not be made."
        chrome_policy {
          SigninAllowed {
            policy_options {mode: MANDATORY}
            SigninAllowed: false
          }
        }
      })");
}

std::string GetHttpMethodString(TrustedVaultRequest::HttpMethod http_method) {
  switch (http_method) {
    case TrustedVaultRequest::HttpMethod::kGet:
      return "GET";
    case TrustedVaultRequest::HttpMethod::kPost:
      return "POST";
    case TrustedVaultRequest::HttpMethod::kPatch:
      return "PATCH";
  }
  NOTREACHED_IN_MIGRATION();
  return std::string();
}

TrustedVaultRequest::HttpStatus AccessTokenFetchingErrorToRequestHttpStatus(
    TrustedVaultAccessTokenFetcher::FetchingError access_token_error) {
  switch (access_token_error) {
    case TrustedVaultAccessTokenFetcher::FetchingError::kTransientAuthError:
      return TrustedVaultRequest::HttpStatus::kTransientAccessTokenFetchError;
    case TrustedVaultAccessTokenFetcher::FetchingError::kPersistentAuthError:
      return TrustedVaultRequest::HttpStatus::kPersistentAccessTokenFetchError;
    case TrustedVaultAccessTokenFetcher::FetchingError::kNotPrimaryAccount:
      return TrustedVaultRequest::HttpStatus::
          kPrimaryAccountChangeAccessTokenFetchError;
  }
  NOTREACHED_IN_MIGRATION();
  return TrustedVaultRequest::HttpStatus::kTransientAccessTokenFetchError;
}

}  // namespace

TrustedVaultRequest::TrustedVaultRequest(
    const CoreAccountId& account_id,
    HttpMethod http_method,
    const GURL& request_url,
    const std::optional<std::string>& serialized_request_proto,
    base::TimeDelta max_retry_duration,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::unique_ptr<TrustedVaultAccessTokenFetcher> access_token_fetcher,
    RecordFetchStatusCallback record_fetch_status_callback)
    : account_id_(account_id),
      http_method_(http_method),
      request_url_(request_url),
      serialized_request_proto_(serialized_request_proto),
      url_loader_factory_(std::move(url_loader_factory)),
      access_token_fetcher_(std::move(access_token_fetcher)),
      record_fetch_status_callback_(record_fetch_status_callback),
      max_retry_time_(base::TimeTicks::Now() + max_retry_duration),
      backoff_entry_(&kRetryPolicy) {
  DCHECK(url_loader_factory_);
  DCHECK(http_method == HttpMethod::kPost ||
         http_method == HttpMethod::kPatch ||
         !serialized_request_proto.has_value());
  DCHECK(access_token_fetcher_);
}

TrustedVaultRequest::~TrustedVaultRequest() = default;

void TrustedVaultRequest::FetchAccessTokenAndSendRequest(
    CompletionCallback callback) {
  completion_callback_ = std::move(callback);
  access_token_fetcher_->FetchAccessToken(
      account_id_, base::BindOnce(&TrustedVaultRequest::OnAccessTokenFetched,
                                  weak_ptr_factory_.GetWeakPtr()));
}

void TrustedVaultRequest::OnAccessTokenFetched(
    TrustedVaultAccessTokenFetcher::AccessTokenInfoOrError
        access_token_info_or_error) {
  base::UmaHistogramBoolean("Sync.TrustedVaultAccessTokenFetchSuccess",
                            access_token_info_or_error.has_value());

  if (!access_token_info_or_error.has_value()) {
    backoff_entry_.InformOfRequest(/*succeeded=*/false);
    if (access_token_info_or_error.error() ==
            TrustedVaultAccessTokenFetcher::FetchingError::
                kTransientAuthError &&
        CanRetry()) {
      ScheduleRetry();
      return;
    }
    RunCompletionCallbackAndMaybeDestroySelf(
        /*status=*/AccessTokenFetchingErrorToRequestHttpStatus(
            access_token_info_or_error.error()),
        /*response_body=*/std::string());
    return;
  }

  url_loader_ = CreateURLLoader(access_token_info_or_error->token);
  // Destroying |this| object will cancel the request, so use of Unretained() is
  // safe here.
  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&TrustedVaultRequest::OnURLLoadComplete,
                     base::Unretained(this)));
}

void TrustedVaultRequest::OnURLLoadComplete(
    std::unique_ptr<std::string> response_body) {
  int http_response_code = 0;

  if (url_loader_->ResponseInfo() && url_loader_->ResponseInfo()->headers) {
    http_response_code = url_loader_->ResponseInfo()->headers->response_code();
  }

  if (record_fetch_status_callback_) {
    record_fetch_status_callback_.Run(http_response_code,
                                      url_loader_->NetError());
  }

  std::string response_content = response_body ? *response_body : std::string();
  if (http_response_code == 0) {
    backoff_entry_.InformOfRequest(/*succeeded=*/false);
    if (CanRetry()) {
      ScheduleRetry();
      return;
    }
    RunCompletionCallbackAndMaybeDestroySelf(HttpStatus::kNetworkError,
                                             response_content);
    return;
  }
  if (http_response_code == net::HTTP_BAD_REQUEST) {
    RunCompletionCallbackAndMaybeDestroySelf(HttpStatus::kBadRequest,
                                             response_content);
    return;
  }
  if (http_response_code == net::HTTP_NOT_FOUND) {
    RunCompletionCallbackAndMaybeDestroySelf(HttpStatus::kNotFound,
                                             response_content);
    return;
  }
  if (http_response_code == net::HTTP_CONFLICT) {
    RunCompletionCallbackAndMaybeDestroySelf(HttpStatus::kConflict,
                                             response_content);
    return;
  }

  if (http_response_code != net::HTTP_OK &&
      http_response_code != net::HTTP_NO_CONTENT) {
    RunCompletionCallbackAndMaybeDestroySelf(HttpStatus::kOtherError,
                                             response_content);
    return;
  }
  RunCompletionCallbackAndMaybeDestroySelf(HttpStatus::kSuccess,
                                           response_content);
}

std::unique_ptr<network::SimpleURLLoader> TrustedVaultRequest::CreateURLLoader(
    const std::string& access_token) const {
  auto request = std::make_unique<network::ResourceRequest>();
  // Specify that the server's response body should be formatted as a
  // serialized proto.
  request->url =
      net::AppendQueryParameter(request_url_, kQueryParameterAlternateOutputKey,
                                kQueryParameterAlternateOutputProto);
  request->method = GetHttpMethodString(http_method_);
  request->credentials_mode =
      google_apis::GetOmitCredentialsModeForGaiaRequests();
  request->headers.SetHeader(
      kAuthorizationHeader,
      /*value=*/base::StringPrintf("Bearer %s", access_token.c_str()));

  std::unique_ptr<network::SimpleURLLoader> url_loader =
      network::SimpleURLLoader::Create(std::move(request),
                                       CreateTrafficAnnotationTag());

  // Fetchers are sometimes cancelled because a network change was detected,
  // especially at startup and after sign-in on ChromeOS. Still (despite of more
  // advanced retry logic) use basic retry option for network change errors.
  url_loader->SetRetryOptions(
      3, network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);
  url_loader->SetAllowHttpErrorResults(true);

  if (serialized_request_proto_.has_value()) {
    url_loader->AttachStringForUpload(*serialized_request_proto_,
                                      kProtobufContentType);
  }
  return url_loader;
}

bool TrustedVaultRequest::CanRetry() const {
  return backoff_entry_.GetReleaseTime() < max_retry_time_;
}

void TrustedVaultRequest::ScheduleRetry() {
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&TrustedVaultRequest::Retry,
                     weak_ptr_factory_.GetWeakPtr()),
      backoff_entry_.GetTimeUntilRelease());
}

void TrustedVaultRequest::Retry() {
  // Start over from access token fetching, since its fetching errors also
  // trigger retries.
  access_token_fetcher_->FetchAccessToken(
      account_id_, base::BindOnce(&TrustedVaultRequest::OnAccessTokenFetched,
                                  weak_ptr_factory_.GetWeakPtr()));
}

void TrustedVaultRequest::RunCompletionCallbackAndMaybeDestroySelf(
    HttpStatus status,
    const std::string& response_body) {
  std::move(completion_callback_).Run(status, response_body);
}

}  // namespace trusted_vault
