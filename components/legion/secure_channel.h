// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEGION_SECURE_CHANNEL_H_
#define COMPONENTS_LEGION_SECURE_CHANNEL_H_

#include "base/functional/callback.h"
#include "base/types/expected.h"
#include "components/legion/error_code.h"
#include "components/legion/legion_common.h"

namespace legion {

// Interface for the Secure Channel Layer.
// This layer is responsible for handling the secure communication
// with the service, likely wrapping the Backend client logic
// and using the WebSocketClient for transport.
class SecureChannel {
 public:
  using OnResponseReceivedCallback =
      base::OnceCallback<void(base::expected<Response, ErrorCode>)>;

  virtual ~SecureChannel() = default;

  // Asynchronously performs the operation over the secure channel.
  virtual void Write(Request request, OnResponseReceivedCallback callback) = 0;
};

}  // namespace legion

#endif  // COMPONENTS_LEGION_SECURE_CHANNEL_H_
