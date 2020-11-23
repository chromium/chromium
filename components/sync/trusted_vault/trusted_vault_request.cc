// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/trusted_vault/trusted_vault_request.h"

#include <utility>

#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/sync/trusted_vault/trusted_vault_access_token_fetcher.h"
#include "net/base/url_util.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace syncer {

namespace {

const char kAuthorizationHeader[] = "Authorization";
const char kProtobufContentType[] = "application/x-protobuf";
const char kQueryParameterAlternateOutputKey[] = "alt";
const char kQueryParameterAlternateOutputProto[] = "proto";

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

}  // namespace

TrustedVaultRequest::TrustedVaultRequest(
    HttpMethod http_method,
    const GURL& request_url,
    const base::Optional<std::string>& serialized_request_proto)
    : http_method_(http_method),
      request_url_(request_url),
      serialized_request_proto_(serialized_request_proto) {
  DCHECK(http_method == HttpMethod::kPost ||
         !serialized_request_proto.has_value());
}

TrustedVaultRequest::~TrustedVaultRequest() = default;

void TrustedVaultRequest::FetchAccessTokenAndSendRequest(
    const CoreAccountId& account_id,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    TrustedVaultAccessTokenFetcher* access_token_fetcher,
    CompletionCallback callback) {
  DCHECK(url_loader_factory);
  DCHECK(access_token_fetcher);
  completion_callback_ = std::move(callback);
  access_token_fetcher->FetchAccessToken(
      account_id,
      base::BindOnce(&TrustedVaultRequest::OnAccessTokenFetched,
                     weak_ptr_factory_.GetWeakPtr(), url_loader_factory));
}

void TrustedVaultRequest::OnAccessTokenFetched(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    base::Optional<signin::AccessTokenInfo> access_token_info) {
  if (!access_token_info.has_value()) {
    RunCompletionCallbackAndMaybeDestroySelf(HttpStatus::kOtherError,
                                             /*response_body=*/std::string());
    return;
  }

  url_loader_ = CreateURLLoader(url_loader_factory, access_token_info->token);
  // Destroying |this| object will cancel the request, so use of Unretained() is
  // safe here.
  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory.get(),
      base::BindOnce(&TrustedVaultRequest::OnURLLoadComplete,
                     base::Unretained(this)));
}

void TrustedVaultRequest::OnURLLoadComplete(
    std::unique_ptr<std::string> response_body) {
  int http_response_code = 0;
  if (url_loader_->ResponseInfo() && url_loader_->ResponseInfo()->headers) {
    http_response_code = url_loader_->ResponseInfo()->headers->response_code();
  }
  if (http_response_code == net::HTTP_BAD_REQUEST) {
    // Bad request can indicate client-side data being obsolete, distinguish it
    // to allow API users to decide how to handle.
    RunCompletionCallbackAndMaybeDestroySelf(HttpStatus::kBadRequest,
                                             std::string());
    return;
  }
  if (http_response_code != net::HTTP_OK &&
      http_response_code != net::HTTP_NO_CONTENT) {
    RunCompletionCallbackAndMaybeDestroySelf(HttpStatus::kOtherError,
                                             std::string());
    return;
  }
  RunCompletionCallbackAndMaybeDestroySelf(HttpStatus::kSuccess,
                                           *response_body);
}

std::unique_ptr<network::SimpleURLLoader> TrustedVaultRequest::CreateURLLoader(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& access_token) const {
  auto request = std::make_unique<network::ResourceRequest>();
  // Specify that the server's response body should be formatted as a
  // serialized proto.
  request->url =
      net::AppendQueryParameter(request_url_, kQueryParameterAlternateOutputKey,
                                kQueryParameterAlternateOutputProto);
  request->method = GetHttpMethodString(http_method_);
  request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  request->headers.SetHeader(
      kAuthorizationHeader,
      /*value=*/base::StringPrintf("Bearer %s", access_token.c_str()));

  std::unique_ptr<network::SimpleURLLoader> url_loader =
      network::SimpleURLLoader::Create(std::move(request),
                                       CreateTrafficAnnotationTag());

  // TODO(crbug.com/1113598): do we need to set retry options? (in particular
  // RETRY_ON_NETWORK_CHANGE).

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
