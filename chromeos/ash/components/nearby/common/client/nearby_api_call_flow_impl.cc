// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/nearby/common/client/nearby_api_call_flow_impl.h"

#include "base/strings/string_number_conversions.h"
#include "chromeos/ash/components/nearby/common/client/nearby_http_result.h"
#include "net/base/net_errors.h"
#include "net/base/url_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace {

const char kGet[] = "GET";
const char kPatch[] = "PATCH";
const char kPost[] = "POST";
const char kProtobufContentType[] = "application/x-protobuf";
const char kQueryParameterAlternateOutputKey[] = "alt";
const char kQueryParameterAlternateOutputProto[] = "proto";
const char kPlatformTypeHeaderName[] = "X-Sharing-Platform-Type";
const char kPlatformTypeHeaderValue[] = "OSType.CHROME_OS";

}  // namespace

namespace ash::nearby {

NearbyApiCallFlowImpl::NearbyApiCallFlowImpl() = default;
NearbyApiCallFlowImpl::~NearbyApiCallFlowImpl() = default;

void NearbyApiCallFlowImpl::StartPostRequest(
    const GURL& request_url,
    const std::string& serialized_request,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& access_token,
    ResultCallback&& result_callback,
    ErrorCallback&& error_callback) {
  request_url_ = request_url;
  request_http_method_ = kPost;
  serialized_request_ = serialized_request;
  result_callback_ = std::move(result_callback);
  error_callback_ = std::move(error_callback);
  OAuth2ApiCallFlow::Start(std::move(url_loader_factory), access_token);
}

void NearbyApiCallFlowImpl::StartPatchRequest(
    const GURL& request_url,
    const std::string& serialized_request,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& access_token,
    ResultCallback&& result_callback,
    ErrorCallback&& error_callback) {
  request_url_ = request_url;
  request_http_method_ = kPatch;
  serialized_request_ = serialized_request;
  result_callback_ = std::move(result_callback);
  error_callback_ = std::move(error_callback);
  OAuth2ApiCallFlow::Start(std::move(url_loader_factory), access_token);
}

void NearbyApiCallFlowImpl::StartGetRequest(
    const GURL& request_url,
    const QueryParameters& request_as_query_parameters,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& access_token,
    ResultCallback&& result_callback,
    ErrorCallback&& error_callback) {
  request_url_ = request_url;
  request_http_method_ = kGet;
  request_as_query_parameters_ = request_as_query_parameters;
  result_callback_ = std::move(result_callback);
  error_callback_ = std::move(error_callback);
  OAuth2ApiCallFlow::Start(std::move(url_loader_factory), access_token);
}

void NearbyApiCallFlowImpl::SetPartialNetworkTrafficAnnotation(
    const net::PartialNetworkTrafficAnnotationTag& partial_traffic_annotation) {
  partial_network_annotation_ =
      std::make_unique<net::PartialNetworkTrafficAnnotationTag>(
          partial_traffic_annotation);
}

GURL NearbyApiCallFlowImpl::CreateApiCallUrl() {
  // Specifies that the server's response body should be formatted as a
  // serialized proto.
  request_url_ =
      net::AppendQueryParameter(request_url_, kQueryParameterAlternateOutputKey,
                                kQueryParameterAlternateOutputProto);

  // GET requests encode the request proto as query parameters.
  if (request_as_query_parameters_) {
    for (const auto& key_value_pair : *request_as_query_parameters_) {
      request_url_ = net::AppendQueryParameter(
          request_url_, key_value_pair.first, key_value_pair.second);
    }
  }

  // TODO (b/274978630): Re-add logging once CD_LOG is implemented
  // (see go/np-plumbing).
  return request_url_;
}

net::HttpRequestHeaders NearbyApiCallFlowImpl::CreateApiCallHeaders() {
  // Inform the server that Chrome OS is making the request; this helps with
  // diagnostics.
  net::HttpRequestHeaders headers;
  headers.SetHeader(kPlatformTypeHeaderName, kPlatformTypeHeaderValue);
  return headers;
}

std::string NearbyApiCallFlowImpl::CreateApiCallBody() {
  return serialized_request_.value_or(std::string());
}

std::string NearbyApiCallFlowImpl::CreateApiCallBodyContentType() {
  return serialized_request_ ? kProtobufContentType : std::string();
}

// Note: Unlike OAuth2ApiCallFlow, we do *not* determine the request type
// based on whether or not the body is empty.
std::string NearbyApiCallFlowImpl::GetRequestTypeForBody(
    const std::string& body) {
  DCHECK(!request_http_method_.empty());
  return request_http_method_;
}

void NearbyApiCallFlowImpl::ProcessApiCallSuccess(
    const network::mojom::URLResponseHead* head,
    std::unique_ptr<std::string> body) {
  if (!body) {
    std::move(error_callback_).Run(NearbyHttpError::kResponseMalformed);
    return;
  }
  std::move(result_callback_).Run(std::move(*body));
}

void NearbyApiCallFlowImpl::ProcessApiCallFailure(
    int net_error,
    const network::mojom::URLResponseHead* head,
    std::unique_ptr<std::string> body) {
  std::optional<NearbyHttpError> error;
  std::string error_message;
  if (net_error == net::OK) {
    int response_code = -1;
    if (head && head->headers) {
      response_code = head->headers->response_code();
    }
    error = NearbyHttpErrorForHttpResponseCode(response_code);
  } else {
    error = NearbyHttpError::kOffline;
  }

  // TODO (b/274978630): Re-add logging once CD_LOG is implemented
  // (see go/np-plumbing).

  std::move(error_callback_).Run(*error);
}
net::PartialNetworkTrafficAnnotationTag
NearbyApiCallFlowImpl::GetNetworkTrafficAnnotationTag() {
  DCHECK(partial_network_annotation_);
  return *partial_network_annotation_;
}

}  // namespace ash::nearby
