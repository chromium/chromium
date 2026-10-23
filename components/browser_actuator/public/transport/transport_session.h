// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_ACTUATOR_PUBLIC_TRANSPORT_TRANSPORT_SESSION_H_
#define COMPONENTS_BROWSER_ACTUATOR_PUBLIC_TRANSPORT_TRANSPORT_SESSION_H_

#include <string_view>

#include "base/types/expected.h"
#include "components/browser_actuator/public/transport/common.h"

namespace google::protobuf {
class MessageLite;
}  // namespace google::protobuf

namespace browser_actuator::transport {

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

}  // namespace browser_actuator::transport

#endif  // COMPONENTS_BROWSER_ACTUATOR_PUBLIC_TRANSPORT_TRANSPORT_SESSION_H_
