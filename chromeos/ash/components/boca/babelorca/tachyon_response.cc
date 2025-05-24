// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/babelorca/tachyon_response.h"

#include <memory>
#include <string>
#include <utility>

#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/header_util.h"

namespace ash::babelorca {

TachyonResponse::TachyonResponse(Status status) : status_(status) {}

TachyonResponse::TachyonResponse(int rpc_code,
                                 const std::string& error_message) {
  constexpr int kOkCode = 0;
  constexpr int kUnauthenticatedCode = 16;
  switch (rpc_code) {
    case kOkCode:
      status_ = Status::kOk;
      break;
    case kUnauthenticatedCode:
      status_ = Status::kAuthError;
      break;
    default:
      status_ = Status::kHttpError;
  }
  error_message_ = error_message;
}

TachyonResponse::TachyonResponse(int net_error,
                                 std::optional<int> http_status_code,
                                 std::unique_ptr<std::string> response_body) {
  if (net_error != net::OK &&
      net_error != net::ERR_HTTP_RESPONSE_CODE_FAILURE) {
    status_ = Status::kNetworkError;
    return;
  }
  if (!http_status_code.has_value()) {
    status_ = Status::kInternalError;
    return;
  }
  if (http_status_code == net::HttpStatusCode::HTTP_UNAUTHORIZED) {
    status_ = Status::kAuthError;
    return;
  }
  if (!network::IsSuccessfulStatus(http_status_code.value())) {
    status_ = Status::kHttpError;
    return;
  }
  response_body_ = response_body ? std::move(*response_body) : "";
  status_ = Status::kOk;
}

TachyonResponse::~TachyonResponse() = default;

bool TachyonResponse::ok() const {
  return status_ == Status::kOk;
}

}  // namespace ash::babelorca
