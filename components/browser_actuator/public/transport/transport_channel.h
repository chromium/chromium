// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_ACTUATOR_PUBLIC_TRANSPORT_TRANSPORT_CHANNEL_H_
#define COMPONENTS_BROWSER_ACTUATOR_PUBLIC_TRANSPORT_TRANSPORT_CHANNEL_H_

#include <cstdint>
#include <optional>
#include <string_view>

#include "components/browser_actuator/public/transport/common.h"

namespace google::protobuf {
class MessageLite;
}  // namespace google::protobuf

namespace browser_actuator::transport {

class TransportHandlerFactoryRegistry;
class TransportSessionRegistry;

// Creates the physical connection to send and receive messages for all
// transport sessions between the browser and the server.
class TransportChannel {
 public:
  virtual ~TransportChannel() = default;

  virtual TransportHandlerFactoryRegistry* GetHandlerFactoryRegistry() = 0;
  virtual TransportSessionRegistry* GetSessionRegistry() = 0;

  // Sends an upstream message through the TransportChannel for a session.
  // `responding_to_sequence_number` identifies the downstream message sequence
  // number being replied to, or `std::nullopt` for unsolicited messages.
  virtual void SendUpstreamMessage(
      std::string_view session_id,
      PayloadType payload_type,
      const google::protobuf::MessageLite& message,
      std::optional<int64_t> responding_to_sequence_number) = 0;
};

}  // namespace browser_actuator::transport

#endif  // COMPONENTS_BROWSER_ACTUATOR_PUBLIC_TRANSPORT_TRANSPORT_CHANNEL_H_
