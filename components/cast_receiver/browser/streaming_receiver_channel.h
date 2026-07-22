// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_RECEIVER_BROWSER_STREAMING_RECEIVER_CHANNEL_H_
#define COMPONENTS_CAST_RECEIVER_BROWSER_STREAMING_RECEIVER_CHANNEL_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/cast/message_port/message_port.h"
#include "components/cast_receiver/proto/display_info.pb.h"
#include "components/cast_receiver/proto/exo_bootstrap.pb.h"

namespace google::protobuf {
class MessageLite;
}

namespace cast_receiver {

class InputCapabilities;
class InputEvent;
class MessagePortService;

// Represents a channel for transmitting various messages over the connected
// channels.
//
// This class manages the control port (for bootstrap) and the sub-channels
// (for input events and capabilities) using MessagePortService.
class StreamingReceiverChannel {
 public:
  using BootstrapCallback = base::OnceCallback<void(ExoBootstrapMessage)>;

  StreamingReceiverChannel(MessagePortService* message_port_service,
                           std::optional<DisplayInfo> display_info,
                           BootstrapCallback bootstrap_cb);
  ~StreamingReceiverChannel();

  StreamingReceiverChannel(const StreamingReceiverChannel&) = delete;
  StreamingReceiverChannel& operator=(const StreamingReceiverChannel&) = delete;

  // Sends an InputEvent to the sender.
  void SendInputEvent(const InputEvent& event);

  // Sends InputCapabilities to the sender.
  void SendInputCapabilities(const InputCapabilities& capabilities);

 private:
  class SubChannelHandler : public cast_api_bindings::MessagePort::Receiver {
   public:
    SubChannelHandler(std::string_view name,
                      std::unique_ptr<cast_api_bindings::MessagePort> port);
    ~SubChannelHandler() override;

    cast_api_bindings::MessagePort* port() { return port_.get(); }
    const std::string& name() const { return name_; }

   private:
    // cast_api_bindings::MessagePort::Receiver implementation:
    bool OnMessage(std::string_view message,
                   std::vector<std::unique_ptr<cast_api_bindings::MessagePort>>
                       ports) override;
    void OnPipeError() override;

    std::string name_;
    std::unique_ptr<cast_api_bindings::MessagePort> port_;
  };

  std::unique_ptr<SubChannelHandler> ConnectSubChannel(
      std::string_view namespace_name);

  // Handler for the bootstrap control port.
  class BootstrapHandler : public cast_api_bindings::MessagePort::Receiver {
   public:
    BootstrapHandler(StreamingReceiverChannel* owner,
                     std::unique_ptr<cast_api_bindings::MessagePort> port);
    ~BootstrapHandler() override;

    cast_api_bindings::MessagePort* port() { return port_.get(); }

   private:
    // cast_api_bindings::MessagePort::Receiver implementation:
    bool OnMessage(std::string_view message,
                   std::vector<std::unique_ptr<cast_api_bindings::MessagePort>>
                       ports) override;
    void OnPipeError() override;

    StreamingReceiverChannel* const owner_;
    std::unique_ptr<cast_api_bindings::MessagePort> port_;
  };
  friend class BootstrapHandler;

  void OnBootstrapRequest(const ExoBootstrapMessage& request);
  void SendBootstrapResponse(const ExoBootstrapMessage& request);
  void NotifyComplete(ExoBootstrapMessage request);

  bool SendProtoMessage(SubChannelHandler* handler,
                        const google::protobuf::MessageLite& message);

  MessagePortService* const message_port_service_;
  std::optional<DisplayInfo> display_info_;
  BootstrapCallback bootstrap_cb_;

  std::unique_ptr<SubChannelHandler> input_event_handler_;
  std::unique_ptr<SubChannelHandler> input_capabilities_handler_;
  std::unique_ptr<BootstrapHandler> bootstrap_handler_;

  base::WeakPtrFactory<StreamingReceiverChannel> weak_factory_{this};
};

}  // namespace cast_receiver

#endif  // COMPONENTS_CAST_RECEIVER_BROWSER_STREAMING_RECEIVER_CHANNEL_H_
