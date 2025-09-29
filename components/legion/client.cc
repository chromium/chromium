// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/legion/client.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/logging.h"

namespace legion {

Client::Client(
    std::unique_ptr<SecureChannel> secure_channel,
    const std::string& api_key)
    : secure_channel_(std::move(secure_channel)),
      api_key_(api_key) {
  CHECK(secure_channel_);
}

Client::~Client() = default;

bool Client::Authenticate() {
  // This is a placeholder. API key will be added to requests
  // within the SecureChannel or its transport.
  DVLOG(1) << "Performing Authentication (API Key)...";
  if (api_key_.empty()) {
    LOG(ERROR) << "API Key is empty.";
    return false;
  }
  // In a real scenario, the API key would be used in network requests,
  // so this function might just validate the key format or presence.
  return true;
}

void Client::SendRequest(
    Request request,
    OnRequestCompletedCallback callback) {
  DVLOG(1) << "SendRequest started.";

  // Authentication Step (currently placeholder for API key)
  if (!Authenticate()) {
    LOG(ERROR) << "Authentication failed.";
    std::move(callback).Run(ResultCode::kAuthenticationFailed,
                              std::nullopt);
    return;
  }
  DVLOG(1) << "Authentication successful.";

  DVLOG(1) << "Calling SecureChannelClient to execute the request.";
  // The SecureChannel is responsible for using the underlying
  // transport (WebSocketClient) to communicate with the service,
  // including adding the api_key_ to the request headers/parameters.
  secure_channel_->Write(
      std::move(request),
      base::BindOnce(
          [](OnRequestCompletedCallback cb, ResultCode result,
             std::optional<Response> response) {
            if (result == ResultCode::kSuccess) {
              // A success result should always be accompanied by a response.
              CHECK(response.has_value());
            }
            std::move(cb).Run(result, std::move(response));
          },
          std::move(callback)));
}

}  // namespace legion
