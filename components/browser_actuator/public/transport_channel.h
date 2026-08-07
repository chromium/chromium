// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_ACTUATOR_PUBLIC_TRANSPORT_CHANNEL_H_
#define COMPONENTS_BROWSER_ACTUATOR_PUBLIC_TRANSPORT_CHANNEL_H_

#include <string_view>

#include "components/browser_actuator/public/common.h"

namespace google::protobuf {
class MessageLite;
}  // namespace google::protobuf

namespace browser_actuator {

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
  virtual void SendUpstreamMessage(
      std::string_view session_id,
      PayloadType payload_type,
      const google::protobuf::MessageLite& message) = 0;
};

}  // namespace browser_actuator

#endif  // COMPONENTS_BROWSER_ACTUATOR_PUBLIC_TRANSPORT_CHANNEL_H_
