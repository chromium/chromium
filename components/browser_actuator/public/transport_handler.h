// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_ACTUATOR_PUBLIC_TRANSPORT_HANDLER_H_
#define COMPONENTS_BROWSER_ACTUATOR_PUBLIC_TRANSPORT_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "base/types/expected.h"
#include "components/browser_actuator/public/common.h"

namespace google::protobuf {
class MessageLite;
}  // namespace google::protobuf

namespace browser_actuator {

class TransportSession;

// Interface that feature clients implement to receive and send messages
// for a specific PayloadType from/to the TransportChannel.
class TransportHandler {
 public:
  explicit TransportHandler(TransportSession* session = nullptr);
  virtual ~TransportHandler();

  TransportHandler(const TransportHandler&) = delete;
  TransportHandler& operator=(const TransportHandler&) = delete;

  // Process incoming downstream or wake-up message.
  virtual void OnMessage(const google::protobuf::MessageLite& message) = 0;

 protected:
  // Send message upstream to the server for this session.
  base::expected<void, SendUpstreamMessageError> SendUpstreamMessage(
      PayloadType payload_type,
      const google::protobuf::MessageLite& message);

  TransportSession* session() const { return session_.get(); }

 private:
  const raw_ptr<TransportSession> session_;
};

}  // namespace browser_actuator

#endif  // COMPONENTS_BROWSER_ACTUATOR_PUBLIC_TRANSPORT_HANDLER_H_
