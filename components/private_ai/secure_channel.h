// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_AI_SECURE_CHANNEL_H_
#define COMPONENTS_PRIVATE_AI_SECURE_CHANNEL_H_

#include "base/functional/callback.h"
#include "base/types/expected.h"
#include "components/private_ai/error_code.h"
#include "components/private_ai/private_ai_common.h"

namespace private_ai {

// Interface for the Secure Channel Layer.
// This layer is responsible for handling the secure communication
// with the service, likely wrapping the Backend client logic
// and using the WebSocketClient for transport.
class SecureChannel {
 public:
  using ResponseCallback =
      base::RepeatingCallback<void(base::expected<Response, ErrorCode>)>;

  class Factory {
   public:
    virtual ~Factory() = default;
    virtual std::unique_ptr<SecureChannel> Create(
        ResponseCallback callback) = 0;
  };

  virtual ~SecureChannel() = default;

  // Asynchronously performs the operation over the secure channel.
  // Returns false if the channel is in a permanent failure state.
  virtual bool Write(const Request& request) = 0;
};

}  // namespace private_ai

#endif  // COMPONENTS_PRIVATE_AI_SECURE_CHANNEL_H_
