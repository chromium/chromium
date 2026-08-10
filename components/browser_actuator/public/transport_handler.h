// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_ACTUATOR_PUBLIC_TRANSPORT_HANDLER_H_
#define COMPONENTS_BROWSER_ACTUATOR_PUBLIC_TRANSPORT_HANDLER_H_

#include <string_view>

namespace google::protobuf {
class MessageLite;
}  // namespace google::protobuf

namespace browser_actuator {

// Interface that feature clients implement to receive messages for a specific
// PayloadType from the TransportChannel.
class TransportHandler {
 public:
  virtual ~TransportHandler() = default;

  // Process incoming downstream or wake-up message.
  virtual void OnMessage(const google::protobuf::MessageLite& message) = 0;
};

}  // namespace browser_actuator

#endif  // COMPONENTS_BROWSER_ACTUATOR_PUBLIC_TRANSPORT_HANDLER_H_
