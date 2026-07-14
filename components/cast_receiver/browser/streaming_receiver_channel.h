// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_RECEIVER_BROWSER_STREAMING_RECEIVER_CHANNEL_H_
#define COMPONENTS_CAST_RECEIVER_BROWSER_STREAMING_RECEIVER_CHANNEL_H_

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "components/cast/message_port/message_port.h"

namespace google::protobuf {
class MessageLite;
}

namespace cast_receiver {

class InputCapabilities;
class InputEvent;
class MessagePortService;

// Represents a channel for transmitting various messages over the connected
// channels.
class StreamingReceiverChannel {
 public:
  // TODO(b/501522425): Use channel name from sender negotiation.
  static inline constexpr char kInputEventChannelNamespace[] =
      "cast.__platform__.input_event_temp";
  static inline constexpr char kInputCapabilitiesChannelNamespace[] =
      "cast.__platform__.input_capabilities_temp";

  explicit StreamingReceiverChannel(MessagePortService* message_port_service);
  ~StreamingReceiverChannel();

  StreamingReceiverChannel(const StreamingReceiverChannel&) = delete;
  StreamingReceiverChannel& operator=(const StreamingReceiverChannel&) = delete;

  // Sends an InputEvent to the sender.
  void SendInputEvent(const InputEvent& event);

  // Sends InputCapabilities to the sender.
  void SendInputCapabilities(const InputCapabilities& capabilities);

 private:
  class PortHandler : public cast_api_bindings::MessagePort::Receiver {
   public:
    PortHandler(std::string_view name,
                std::unique_ptr<cast_api_bindings::MessagePort> port);
    ~PortHandler() override;

    cast_api_bindings::MessagePort* port() { return port_.get(); }

   private:
    // cast_api_bindings::MessagePort::Receiver implementation:
    bool OnMessage(std::string_view message,
                   std::vector<std::unique_ptr<cast_api_bindings::MessagePort>>
                       ports) override;
    void OnPipeError() override;

    std::string name_;
    std::unique_ptr<cast_api_bindings::MessagePort> port_;
  };

  void SendProtoMessage(PortHandler* handler,
                        const google::protobuf::MessageLite& message);

  std::unique_ptr<PortHandler> input_event_handler_;
  std::unique_ptr<PortHandler> input_capabilities_handler_;
};

}  // namespace cast_receiver

#endif  // COMPONENTS_CAST_RECEIVER_BROWSER_STREAMING_RECEIVER_CHANNEL_H_
