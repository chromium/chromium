// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/legion/secure_client.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/logging.h"

namespace legion {

SecureClient::SecureClient(
    std::unique_ptr<SecureChannelClient> secure_channel_client,
    const std::string& api_key)
    : secure_channel_client_(std::move(secure_channel_client)),
      api_key_(api_key) {
  CHECK(secure_channel_client_);
}

SecureClient::~SecureClient() = default;

bool SecureClient::Authenticate() {
  // TODO(nikhiljakhar): Implement API Key based Authentication.
  // This is a placeholder. API key will be added to requests
  // within the SecureChannelClient or its transport.
  DVLOG(1) << "Performing Authentication (API Key)...";
  if (api_key_.empty()) {
    LOG(ERROR) << "API Key is empty.";
    return false;
  }
  // In a real scenario, the API key would be used in network requests,
  // so this function might just validate the key format or presence.
  return true;
}

void SecureClient::SendRequest(
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
  // The SecureChannelClient is responsible for using the underlying
  // transport (WebSocketClient) to communicate with the service,
  // including adding the api_key_ to the request headers/parameters.
  secure_channel_client_->Write(
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
