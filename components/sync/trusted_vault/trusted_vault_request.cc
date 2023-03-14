// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/trusted_vault/trusted_vault_request.h"

#include <utility>

#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/sync/driver/trusted_vault_histograms.h"
#include "components/sync/trusted_vault/trusted_vault_access_token_fetcher.h"
#include "components/sync/trusted_vault/trusted_vault_server_constants.h"
#include "google_apis/credentials_mode.h"
#include "net/base/url_util.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace syncer {

namespace {

const char kAuthorizationHeader[] = "Authorization";
const char kProtobufContentType[] = "application/x-protobuf";

net::NetworkTrafficAnnotationTag CreateTrafficAnnotationTag() {
  return net::DefineNetworkTrafficAnnotation("trusted_vault_request",
                                             R"(
      semantics {
        sender: "Chrome Sync"
        description:
          "Request to vault service in order to retrieve, change or support "
          "future retrieval or change of Sync encryption keys."
        trigger:
          "Periodically/upon certain non-user controlled events after user "
          "signs in Chrome profile."
        data:
          "An OAuth2 access token, sync metadata associated with encryption "
          "keys: encrypted encryption keys, public counterpart of encryption "
          "keys."
        destination: GOOGLE_OWNED_SERVICE
      }
      policy {
        cookies_allowed: NO
        setting:
          "Users can disable Chrome Sync by going into the profile settings "
          "and choosing to sign out."
        chrome_policy {
            SyncDisabled {
               policy_options {mode: MANDATORY}
               SyncDisabled: false
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
  }
  NOTREACHED();
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
  NOTREACHED();
  return TrustedVaultRequest::HttpStatus::kTransientAccessTokenFetchError;
}

}  // namespace

TrustedVaultRequest::TrustedVaultRequest(
    HttpMethod http_method,
    const GURL& request_url,
    const absl::optional<std::string>& serialized_request_proto,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    TrustedVaultURLFetchReasonForUMA reason_for_uma)
    : http_method_(http_method),
      request_url_(request_url),
      serialized_request_proto_(serialized_request_proto),
      url_loader_factory_(std::move(url_loader_factory)),
      reason_for_uma_(reason_for_uma) {
  DCHECK(url_loader_factory_);
  DCHECK(http_method == HttpMethod::kPost ||
         !serialized_request_proto.has_value());
}

TrustedVaultRequest::~TrustedVaultRequest() = default;

void TrustedVaultRequest::FetchAccessTokenAndSendRequest(
    const CoreAccountId& account_id,
    TrustedVaultAccessTokenFetcher* access_token_fetcher,
    CompletionCallback callback) {
  DCHECK(access_token_fetcher);
  completion_callback_ = std::move(callback);
  access_token_fetcher->FetchAccessToken(
      account_id, base::BindOnce(&TrustedVaultRequest::OnAccessTokenFetched,
                                 weak_ptr_factory_.GetWeakPtr()));
}

void TrustedVaultRequest::OnAccessTokenFetched(
    TrustedVaultAccessTokenFetcher::AccessTokenInfoOrError
        access_token_info_or_error) {
  base::UmaHistogramBoolean("Sync.TrustedVaultAccessTokenFetchSuccess",
                            access_token_info_or_error.has_value());

  if (!access_token_info_or_error.has_value()) {
    // TODO(crbug.com/1413179): expose persistent auth errors to the higher
    // level as a dedicated status.
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

  RecordTrustedVaultURLFetchResponse(/*http_response_code=*/http_response_code,
                                     /*net_error=*/url_loader_->NetError(),
                                     reason_for_uma_);

  std::string response_content = response_body ? *response_body : std::string();
  if (http_response_code == 0) {
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
  // especially at startup and after sign-in on ChromeOS.
  url_loader->SetRetryOptions(
      3, network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);
  url_loader->SetAllowHttpErrorResults(true);

  if (serialized_request_proto_.has_value()) {
    url_loader->AttachStringForUpload(*serialized_request_proto_,
                                      kProtobufContentType);
  }
  return url_loader;
}

void TrustedVaultRequest::RunCompletionCallbackAndMaybeDestroySelf(
    HttpStatus status,
    const std::string& response_body) {
  std::move(completion_callback_).Run(status, response_body);
}

}  // namespace syncer
