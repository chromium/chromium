// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/nearby/common/client/nearby_http_result.h"

#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/header_util.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace ash::nearby {

NearbyHttpError NearbyHttpErrorForHttpResponseCode(int response_code) {
  if (response_code == 400) {
    return NearbyHttpError::kBadRequest;
  }

  if (response_code == 403) {
    return NearbyHttpError::kAuthenticationError;
  }

  if (response_code == 404) {
    return NearbyHttpError::kEndpointNotFound;
  }

  if (response_code >= 500 && response_code < 600) {
    return NearbyHttpError::kInternalServerError;
  }

  return NearbyHttpError::kUnknown;
}

NearbyHttpResult NearbyHttpErrorToResult(NearbyHttpError error) {
  switch (error) {
    case NearbyHttpError::kOffline:
      return NearbyHttpResult::kHttpErrorOffline;
    case NearbyHttpError::kEndpointNotFound:
      return NearbyHttpResult::kHttpErrorEndpointNotFound;
    case NearbyHttpError::kAuthenticationError:
      return NearbyHttpResult::kHttpErrorAuthenticationError;
    case NearbyHttpError::kBadRequest:
      return NearbyHttpResult::kHttpErrorBadRequest;
    case NearbyHttpError::kResponseMalformed:
      return NearbyHttpResult::kHttpErrorResponseMalformed;
    case NearbyHttpError::kInternalServerError:
      return NearbyHttpResult::kHttpErrorInternalServerError;
    case NearbyHttpError::kUnknown:
      return NearbyHttpResult::kHttpErrorUnknown;
  }
}

NearbyHttpStatus::NearbyHttpStatus(const int net_error,
                                   const network::mojom::URLResponseHead* head)
    : net_error_code_(net_error) {
  if (head && head->headers) {
    http_response_code_ = head->headers->response_code();
  }

  bool net_success = (net_error_code_ == net::OK ||
                      net_error_code_ == net::ERR_HTTP_RESPONSE_CODE_FAILURE) &&
                     http_response_code_;
  bool http_success =
      net_success && network::IsSuccessfulStatus(*http_response_code_);

  if (http_success) {
    status_ = Status::kSuccess;
  } else if (net_success) {
    status_ = Status::kHttpFailure;
  } else {
    status_ = Status::kNetworkFailure;
  }
}

NearbyHttpStatus::NearbyHttpStatus(const NearbyHttpStatus& status) = default;

NearbyHttpStatus::~NearbyHttpStatus() = default;

bool NearbyHttpStatus::IsSuccess() const {
  return status_ == Status::kSuccess;
}

int NearbyHttpStatus::GetResultCodeForMetrics() const {
  switch (status_) {
    case Status::kNetworkFailure:
      return net_error_code_;
    case Status::kSuccess:
    case Status::kHttpFailure:
      return *http_response_code_;
  }
}

std::string NearbyHttpStatus::ToString() const {
  std::string status;
  switch (status_) {
    case Status::kSuccess:
      status = "kSuccess";
      break;
    case Status::kNetworkFailure:
      status = "kNetworkFailure";
      break;
    case Status::kHttpFailure:
      status = "kHttpFailure";
      break;
  }
  std::string net_code = net::ErrorToString(net_error_code_);
  std::string response_code =
      http_response_code_.has_value()
          ? net::GetHttpReasonPhrase(
                static_cast<net::HttpStatusCode>(*http_response_code_))
          : "[null]";

  return "status=" + status + ", net_code=" + net_code +
         ", response_code=" + response_code;
}

std::ostream& operator<<(std::ostream& stream, const NearbyHttpResult& result) {
  switch (result) {
    case NearbyHttpResult::kSuccess:
      stream << "[Success]";
      break;
    case NearbyHttpResult::kTimeout:
      stream << "[Timeout]";
      break;
    case NearbyHttpResult::kHttpErrorOffline:
      stream << "[HTTP Error: Offline]";
      break;
    case NearbyHttpResult::kHttpErrorEndpointNotFound:
      stream << "[HTTP Error: Endpoint not found]";
      break;
    case NearbyHttpResult::kHttpErrorAuthenticationError:
      stream << "[HTTP Error: Authentication error]";
      break;
    case NearbyHttpResult::kHttpErrorBadRequest:
      stream << "[HTTP Error: Bad request]";
      break;
    case NearbyHttpResult::kHttpErrorResponseMalformed:
      stream << "[HTTP Error: Response malformed]";
      break;
    case NearbyHttpResult::kHttpErrorInternalServerError:
      stream << "[HTTP Error: Internal server error]";
      break;
    case NearbyHttpResult::kHttpErrorUnknown:
      stream << "[HTTP Error: Unknown]";
      break;
  }
  return stream;
}

std::ostream& operator<<(std::ostream& stream, const NearbyHttpError& error) {
  stream << NearbyHttpErrorToResult(error);
  return stream;
}

std::ostream& operator<<(std::ostream& stream, const NearbyHttpStatus& status) {
  stream << status.ToString();
  return stream;
}

}  // namespace ash::nearby
