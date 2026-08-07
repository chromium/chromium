// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_ACTUATOR_PUBLIC_TRANSPORT_SESSION_H_
#define COMPONENTS_BROWSER_ACTUATOR_PUBLIC_TRANSPORT_SESSION_H_

#include <string_view>

#include "base/types/expected.h"
#include "components/browser_actuator/public/common.h"

namespace google::protobuf {
class MessageLite;
}  // namespace google::protobuf

namespace browser_actuator {

enum class SendMessageError {
  kChannelDisconnected,
};

// Represents an active session for a task shared between the browser and
// server.
class TransportSession {
 public:
  virtual ~TransportSession() = default;

  virtual std::string_view GetSessionId() const = 0;

  // Sends a message upstream.
  virtual base::expected<void, SendMessageError> SendMessage(
      PayloadType payload_type,
      const google::protobuf::MessageLite& message) = 0;
};

}  // namespace browser_actuator

#endif  // COMPONENTS_BROWSER_ACTUATOR_PUBLIC_TRANSPORT_SESSION_H_
