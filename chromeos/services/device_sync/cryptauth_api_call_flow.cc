// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/cryptauth_api_call_flow.h"

#include "base/strings/string_number_conversions.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "net/base/url_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace chromeos {

namespace device_sync {

namespace {

const char kPost[] = "POST";
const char kGet[] = "GET";
const char kProtobufContentType[] = "application/x-protobuf";
const char kQueryParameterAlternateOutputKey[] = "alt";
const char kQueryParameterAlternateOutputProto[] = "proto";

NetworkRequestError GetErrorForHttpResponseCode(int response_code) {
  if (response_code == 400)
    return NetworkRequestError::kBadRequest;

  if (response_code == 403)
    return NetworkRequestError::kAuthenticationError;

  if (response_code == 404)
    return NetworkRequestError::kEndpointNotFound;

  if (response_code >= 500 && response_code < 600)
    return NetworkRequestError::kInternalServerError;

  return NetworkRequestError::kUnknown;
}

}  // namespace

CryptAuthApiCallFlow::CryptAuthApiCallFlow() {}

CryptAuthApiCallFlow::~CryptAuthApiCallFlow() {}

void CryptAuthApiCallFlow::StartPostRequest(
    const GURL& request_url,
    const std::string& serialized_request,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& access_token,
    const ResultCallback& result_callback,
    const ErrorCallback& error_callback) {
  request_url_ = request_url;
  serialized_request_ = serialized_request;
  result_callback_ = result_callback;
  error_callback_ = error_callback;
  OAuth2ApiCallFlow::Start(std::move(url_loader_factory), access_token);
}

void CryptAuthApiCallFlow::StartGetRequest(
    const GURL& request_url,
    const std::vector<std::pair<std::string, std::string>>&
        request_as_query_parameters,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& access_token,
    const ResultCallback& result_callback,
    const ErrorCallback& error_callback) {
  request_url_ = request_url;
  request_as_query_parameters_ = request_as_query_parameters;
  result_callback_ = result_callback;
  error_callback_ = error_callback;
  OAuth2ApiCallFlow::Start(std::move(url_loader_factory), access_token);
}

GURL CryptAuthApiCallFlow::CreateApiCallUrl() {
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

  return request_url_;
}

std::string CryptAuthApiCallFlow::CreateApiCallBody() {
  return serialized_request_.value_or(std::string());
}

std::string CryptAuthApiCallFlow::CreateApiCallBodyContentType() {
  return serialized_request_ ? kProtobufContentType : std::string();
}

// Note: Unlike OAuth2ApiCallFlow, we do *not* determine the request type
// based on whether or not the body is empty. It is possible to send a POST
// request with an empty body because a proto with default parameters is
// encoded as an empty string.
std::string CryptAuthApiCallFlow::GetRequestTypeForBody(
    const std::string& body) {
  if (serialized_request_) {
    DCHECK(!request_as_query_parameters_);
    return kPost;
  }

  DCHECK(request_as_query_parameters_);
  return kGet;
}

void CryptAuthApiCallFlow::ProcessApiCallSuccess(
    const network::mojom::URLResponseHead* head,
    std::unique_ptr<std::string> body) {
  if (!body) {
    error_callback_.Run(NetworkRequestError::kResponseMalformed);
    return;
  }
  result_callback_.Run(std::move(*body));
}

void CryptAuthApiCallFlow::ProcessApiCallFailure(
    int net_error,
    const network::mojom::URLResponseHead* head,
    std::unique_ptr<std::string> body) {
  base::Optional<NetworkRequestError> error;
  std::string error_message;
  if (net_error == net::OK) {
    int response_code = -1;
    if (head && head->headers)
      response_code = head->headers->response_code();
    error = GetErrorForHttpResponseCode(response_code);
  } else {
    error = NetworkRequestError::kOffline;
  }

  PA_LOG(ERROR) << "API call failed, error code: "
                << (error ? *error : NetworkRequestError::kUnknown);
  if (body)
    PA_LOG(VERBOSE) << "API failure response body:\n" << *body;

  error_callback_.Run(*error);
}

net::PartialNetworkTrafficAnnotationTag
CryptAuthApiCallFlow::GetNetworkTrafficAnnotationTag() {
  DCHECK(partial_network_annotation_ != nullptr);
  return *partial_network_annotation_.get();
}

}  // namespace device_sync

}  // namespace chromeos
