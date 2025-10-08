// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEGION_TRANSPORT_H_
#define COMPONENTS_LEGION_TRANSPORT_H_

#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "base/types/expected.h"
#include "components/legion/legion_common.h"

namespace legion {

// Interface for the Transport Layer.
// Responsible for raw connection and data transfer.
class Transport {
 public:
  enum class TransportError {
    // Socket was closed by the server.
    kSocketClosed,
    // An error occurred on the client. Socket is now closed.
    kError,
  };

  // Callback for when a send operation completes or the status of the
  // connection changes.
  using MessageCallback =
      base::RepeatingCallback<void(base::expected<Response, TransportError>)>;

  virtual ~Transport() = default;

  // Asynchronously sends data to the server.
  // The transport implementation will handle connection management.
  // Results, responses, and status updates related to this send or the
  // connection itself will be delivered via the MessageCallback
  // that the concrete implementation was constructed with.
  virtual void Send(Request request) = 0;
};

}  // namespace legion

#endif  // COMPONENTS_LEGION_TRANSPORT_H_
