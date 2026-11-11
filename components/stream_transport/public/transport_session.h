// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_STREAM_TRANSPORT_PUBLIC_TRANSPORT_SESSION_H_
#define COMPONENTS_STREAM_TRANSPORT_PUBLIC_TRANSPORT_SESSION_H_

#include <string_view>

#include "base/types/expected.h"
#include "components/stream_transport/public/common.h"

namespace google::protobuf {
class MessageLite;
}  // namespace google::protobuf

namespace stream_transport {

// Represents an active session for a task shared between the browser and
// server.
class TransportSession {
 public:
  virtual ~TransportSession() = default;

  virtual std::string_view GetSessionId() const = 0;

  // Sends a message upstream.
  virtual base::expected<void, SendUpstreamMessageError> SendUpstreamMessage(
      PayloadType payload_type,
      const google::protobuf::MessageLite& message) = 0;

  // Dispatches an incoming message to handlers registered for payload_type.
  virtual void OnMessage(PayloadType payload_type,
                         std::string_view serialized_payload) = 0;

  // Closes and destroys this session locally.
  // WARNING: May destroy `this` and all `TransportHandler`s owned by this
  // session; callers must not access `this` after calling `Close()`.
  virtual void Close() = 0;
};

}  // namespace stream_transport

#endif  // COMPONENTS_STREAM_TRANSPORT_PUBLIC_TRANSPORT_SESSION_H_
