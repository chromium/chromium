// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/babelorca/tachyon_client.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "chromeos/ash/components/boca/babelorca/request_data_wrapper.h"
#include "chromeos/ash/components/boca/babelorca/tachyon_response.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace ash::babelorca {

// static
void TachyonClient::HandleResponse(
    std::unique_ptr<network::SimpleURLLoader> url_loader,
    std::unique_ptr<RequestDataWrapper> request_data,
    AuthFailureCallback auth_failure_cb,
    std::unique_ptr<std::string> response_body) {
  std::optional<int> http_status_code =
      url_loader->ResponseInfo() && url_loader->ResponseInfo()->headers
          ? std::make_optional(
                url_loader->ResponseInfo()->headers->response_code())
          : std::nullopt;
  TachyonResponse response(url_loader->NetError(), http_status_code,
                           std::move(response_body));
  if (response.status() == TachyonResponse::Status::kAuthError) {
    std::move(auth_failure_cb).Run(std::move(request_data));
    return;
  }
  std::move(request_data->response_cb).Run(std::move(response));
}

}  // namespace ash::babelorca
