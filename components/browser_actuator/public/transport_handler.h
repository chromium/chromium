// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_ACTUATOR_PUBLIC_TRANSPORT_HANDLER_H_
#define COMPONENTS_BROWSER_ACTUATOR_PUBLIC_TRANSPORT_HANDLER_H_

#include <string_view>

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
//
// Downstream payloads arrive as a PayloadType plus serialized bytes, and each
// handler parses those bytes into its own concrete message type. The transport
// never names a feature's proto: doing so would make
// //components/browser_actuator depend on the proto of every feature it
// carries, including features in //chrome/browser that this component's DEPS
// disallows. There is also no generic alternative -- `MessageLite` is abstract
// and cannot be parsed into, and the `Any::UnpackTo()` escape hatch needs a
// descriptor pool that `LITE_RUNTIME` protos do not carry.
//
// The transport still narrows the payload before it arrives: it maps the wire
// enum to a PayloadType, routes only to handlers registered for that type, and
// validates the `Any.type_url` against the type expected for it.
//
// Upstream messages travel the other way, through SendUpstreamMessage(). That
// direction takes a concrete `MessageLite` because the caller already holds
// one and the transport only has to serialize it.
class TransportHandler {
 public:
  explicit TransportHandler(TransportSession* session = nullptr);
  virtual ~TransportHandler();

  TransportHandler(const TransportHandler&) = delete;
  TransportHandler& operator=(const TransportHandler&) = delete;

  // Processes an incoming downstream or wake-up message.
  // `serialized_payload` holds the `google.protobuf.Any.value` bytes of a
  // payload of `payload_type`. Implementations must parse it into their own
  // concrete message type and must tolerate a parse failure, which indicates a
  // malformed or unexpected payload.
  //
  // `payload_type` is always one this handler's factory declared support for
  // via GetSupportedPayloadTypes(). Handlers registered for a single type may
  // ignore it.
  virtual void OnMessage(PayloadType payload_type,
                         std::string_view serialized_payload) = 0;

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
