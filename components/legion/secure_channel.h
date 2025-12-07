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
  using ResponseCallback =
      base::RepeatingCallback<void(base::expected<Response, ErrorCode>)>;
  using EstablishChannelCallback =
      base::OnceCallback<void(base::expected<void, ErrorCode>)>;

  virtual ~SecureChannel() = default;

  // Sets a callback that will be invoked for each response from the server.
  virtual void SetResponseCallback(ResponseCallback callback) = 0;

  // Establishes a secure channel without sending a request.
  virtual void EstablishChannel(EstablishChannelCallback callback) = 0;

  // Asynchronously performs the operation over the secure channel.
  // Returns false if the channel is in a permanent failure state.
  virtual bool Write(const Request& request) = 0;
};

}  // namespace legion

#endif  // COMPONENTS_LEGION_SECURE_CHANNEL_H_
