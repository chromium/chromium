// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/trusted_vault/vault_service_api_call_flow.h"

#include <utility>

#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/sync/trusted_vault/trusted_vault_access_token_fetcher.h"
#include "net/base/url_util.h"
#include "net/http/http_request_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace syncer {

namespace {

const char kProtobufContentType[] = "application/x-protobuf";
const char kQueryParameterAlternateOutputKey[] = "alt";
const char kQueryParameterAlternateOutputProto[] = "proto";

}  // namespace

VaultServiceApiCallFlow::VaultServiceApiCallFlow(
    HttpMethod http_method,
    const GURL& request_url,
    const net::PartialNetworkTrafficAnnotationTag partial_annotation_tag,
    const base::Optional<std::string>& serialized_request_proto)
    : http_method_(http_method),
      request_url_(request_url),
      serialized_request_proto_(serialized_request_proto),
      partial_annotation_tag_(partial_annotation_tag) {
  DCHECK(http_method == HttpMethod::kPost ||
         !serialized_request_proto.has_value());
}

VaultServiceApiCallFlow::~VaultServiceApiCallFlow() = default;

void VaultServiceApiCallFlow::FetchAccessTokenAndStartFlow(
    const CoreAccountId& account_id,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    TrustedVaultAccessTokenFetcher* access_token_fetcher,
    CompletionCallback callback) {
  DCHECK(url_loader_factory);
  DCHECK(access_token_fetcher);
  completion_callback_ = std::move(callback);
  access_token_fetcher->FetchAccessToken(
      account_id,
      base::BindOnce(&VaultServiceApiCallFlow::OnAccessTokenFetched,
                     weak_ptr_factory_.GetWeakPtr(), url_loader_factory));
}

void VaultServiceApiCallFlow::Start(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& access_token) {
  OAuth2ApiCallFlow::Start(url_loader_factory, access_token);
}

GURL VaultServiceApiCallFlow::CreateApiCallUrl() {
  // Specify that the server's response body should be formatted as a
  // serialized proto.
  return net::AppendQueryParameter(request_url_,
                                   kQueryParameterAlternateOutputKey,
                                   kQueryParameterAlternateOutputProto);
}

std::string VaultServiceApiCallFlow::CreateApiCallBody() {
  return serialized_request_proto_.value_or(std::string());
}

std::string VaultServiceApiCallFlow::CreateApiCallBodyContentType() {
  return serialized_request_proto_.has_value() ? kProtobufContentType
                                               : std::string();
}

std::string VaultServiceApiCallFlow::GetRequestTypeForBody(
    const std::string& body) {
  switch (http_method_) {
    case HttpMethod::kGet:
      return "GET";
    case HttpMethod::kPost:
      return "POST";
  }
  NOTREACHED();
  return std::string();
}

void VaultServiceApiCallFlow::ProcessApiCallSuccess(
    const network::mojom::URLResponseHead* head,
    std::unique_ptr<std::string> body) {
  // Response proto can be filled with default values and be not presented as
  // |body|.
  RunCompletionCallbackAndMaybeDestroySelf(/*success=*/true,
                                           body ? *body : std::string());
}

void VaultServiceApiCallFlow::ProcessApiCallFailure(
    int net_error,
    const network::mojom::URLResponseHead* head,
    std::unique_ptr<std::string> body) {
  RunCompletionCallbackAndMaybeDestroySelf(/*success=*/false,
                                           /*response_body=*/std::string());
}

net::PartialNetworkTrafficAnnotationTag
VaultServiceApiCallFlow::GetNetworkTrafficAnnotationTag() {
  return partial_annotation_tag_;
}

void VaultServiceApiCallFlow::OnAccessTokenFetched(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    base::Optional<signin::AccessTokenInfo> access_token_info) {
  if (access_token_info.has_value()) {
    Start(url_loader_factory, access_token_info->token);
    return;
  }
  RunCompletionCallbackAndMaybeDestroySelf(/*success=*/false,
                                           /*response_body=*/std::string());
}

void VaultServiceApiCallFlow::RunCompletionCallbackAndMaybeDestroySelf(
    bool success,
    const std::string& response_body) {
  std::move(completion_callback_).Run(success, response_body);
}

}  // namespace syncer
