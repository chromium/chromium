// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_INFERENCE_SECURE_CHANNEL_CLIENT_H_
#define COMPONENTS_PRIVATE_INFERENCE_SECURE_CHANNEL_CLIENT_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "base/functional/callback.h"

namespace private_inference {

// Placeholder for the request data structure. Likely a serialized proto.
using Request = std::vector<uint8_t>;
// Placeholder for the response data structure. Likely a deserialized proto.
using Response = std::vector<uint8_t>;

// Represents the result of an operation.
enum class ResultCode {
  // Operation completed successfully.
  kSuccess,
  // A non-transient error occurred. The client should not retry the request.
  kError,
  // Authentication failed, e.g., due to an invalid API key.
  kAuthenticationFailed,
  // A transient network error occurred. The client may retry the request.
  kNetworkError,
};

// Interface for the Secure Channel Layer.
// This layer is responsible for handling the secure communication
// with the service, likely wrapping the Backend client logic
// and using the WebSocketClient for transport.
class SecureChannelClient {
 public:
  using OnWriteCompletedCallback =
      base::OnceCallback<void(ResultCode, std::optional<Response>)>;

  virtual ~SecureChannelClient() = default;

  // Asynchronously performs the operation over the secure channel.
  virtual void Write(Request request, OnWriteCompletedCallback callback) = 0;
};

}  // namespace private_inference

#endif  // COMPONENTS_PRIVATE_INFERENCE_SECURE_CHANNEL_CLIENT_H_
