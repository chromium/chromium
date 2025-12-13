// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/babelorca/tachyon_client.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "chromeos/ash/components/boca/babelorca/request_data_wrapper.h"
#include "chromeos/ash/components/boca/babelorca/tachyon_response.h"
#include "services/network/public/cpp/header_util.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace ash::babelorca {

// static
void TachyonClient::HandleResponse(
    std::unique_ptr<network::SimpleURLLoader> url_loader,
    std::unique_ptr<RequestDataWrapper> request_data,
    AuthFailureCallback auth_failure_cb,
    std::optional<std::string> response_body) {
  std::optional<int> http_status_code =
      url_loader->ResponseInfo() && url_loader->ResponseInfo()->headers
          ? std::make_optional(
                url_loader->ResponseInfo()->headers->response_code())
          : std::nullopt;
  TachyonResponse response(url_loader->NetError(), http_status_code,
                           std::move(response_body));
  VLOG_IF(1, !response.ok())
      << "Request failed with net error: " << url_loader->NetError()
      << ", and http status code: " << http_status_code.value_or(-1);
  if (response.status() == TachyonResponse::Status::kAuthError) {
    std::move(auth_failure_cb).Run(std::move(request_data));
    return;
  }
  std::move(request_data->response_cb).Run(std::move(response));
}

// static
void TachyonClient::MaybeRecordUma(const network::SimpleURLLoader* url_loader,
                                   const RequestDataWrapper* request_data) {
  if (!request_data->uma_name.has_value()) {
    return;
  }
  constexpr char kUmaPathTemplate[] =
      "Ash.Boca.Babelorca.$1.HttpResponseCodeOrNetError";
  std::optional<int> http_status_code =
      url_loader->ResponseInfo() && url_loader->ResponseInfo()->headers
          ? std::make_optional(
                url_loader->ResponseInfo()->headers->response_code())
          : std::nullopt;
  if (url_loader->NetError() == net::OK && !http_status_code.has_value()) {
    VLOG(1) << "Cannot record response metrics for "
            << request_data->uma_name.value()
            << ", net error is ok but http status code is not present.";
    return;
  }
  bool is_net_error =
      url_loader->NetError() != net::OK &&
      url_loader->NetError() != net::ERR_HTTP_RESPONSE_CODE_FAILURE;
  base::UmaHistogramSparse(
      base::ReplaceStringPlaceholders(kUmaPathTemplate,
                                      {request_data->uma_name.value()},
                                      /*=offsets*/ nullptr),
      is_net_error ? url_loader->NetError() : http_status_code.value());
}

}  // namespace ash::babelorca
