// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEGION_TRANSPORT_H_
#define COMPONENTS_LEGION_TRANSPORT_H_

#include "base/functional/callback_forward.h"
#include "base/types/expected.h"
#include "components/legion/legion_common.h"

namespace oak::session::v1 {
class SessionResponse;
class SessionRequest;
}  // namespace oak::session::v1

namespace legion {

// Interface for the Transport Layer.
// Responsible for raw connection and data transfer.
class Transport {
 public:
  enum class TransportError {
    // Socket was closed by the server.
    kSocketClosed,
    // Request could not be serialized.
    kSerializationError,
    // Response could not be parsed.
    kDeserializationError,
    // An error occurred on the client. Socket is now closed.
    kError,
  };

  // Callback for when a response is received for a request.
  using ResponseCallback = base::RepeatingCallback<void(
      base::expected<oak::session::v1::SessionResponse, TransportError>)>;

  virtual ~Transport() = default;

  // Sets a callback that will be invoked for each response from the server.
  virtual void SetResponseCallback(ResponseCallback callback) = 0;

  // Asynchronously sends data to the server.
  // The transport implementation will handle connection management.
  virtual void Send(const oak::session::v1::SessionRequest& request) = 0;
};

}  // namespace legion

#endif  // COMPONENTS_LEGION_TRANSPORT_H_
