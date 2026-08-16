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
#include "base/memory/raw_ptr.h"
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

// Represents a channel for transmitting Exo bootstrap and input messages over
// the primary Cast transport port.
class StreamingReceiverChannel
    : public cast_api_bindings::MessagePort,
      public cast_api_bindings::MessagePort::Receiver {
 public:
  using BootstrapCallback = base::OnceCallback<void(ExoBootstrapMessage)>;

  StreamingReceiverChannel(
      std::unique_ptr<cast_api_bindings::MessagePort> port,
      std::optional<DisplayInfo> display_info,
      BootstrapCallback bootstrap_cb);
  ~StreamingReceiverChannel() override;

  StreamingReceiverChannel(const StreamingReceiverChannel&) = delete;
  StreamingReceiverChannel& operator=(const StreamingReceiverChannel&) = delete;

  // Sends an InputEvent to the sender.
  void SendInputEvent(const InputEvent& event);

  // Sends InputCapabilities to the sender.
  void SendInputCapabilities(const InputCapabilities& capabilities);

  base::WeakPtr<StreamingReceiverChannel> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  // Sends an Exo message wrapped in JSON over the underlying port.
  void SendMessage(const std::string& sender_id,
                   const std::string& message_namespace,
                   const std::string& message);

  // cast_api_bindings::MessagePort implementation:
  bool PostMessage(std::string_view message) override;
  bool PostMessageWithTransferables(
      std::string_view message,
      std::vector<std::unique_ptr<cast_api_bindings::MessagePort>> ports)
      override;
  void SetReceiver(
      cast_api_bindings::MessagePort::Receiver* receiver) override;
  void Close() override;
  bool CanPostMessage() const override;

  // cast_api_bindings::MessagePort::Receiver implementation:
  bool OnMessage(std::string_view message,
                 std::vector<std::unique_ptr<cast_api_bindings::MessagePort>>
                     ports) override;
  void OnPipeError() override;

 private:
  class SubChannelHandler {
   public:
    SubChannelHandler(std::string_view name, std::string_view message_namespace);
    ~SubChannelHandler();

    const std::string& name() const { return name_; }
    const std::string& message_namespace() const { return namespace_; }

   private:
    std::string name_;
    std::string namespace_;
  };

  std::unique_ptr<SubChannelHandler> ConnectSubChannel(
      std::string_view name,
      std::string_view namespace_name);

  bool SendProtoMessage(SubChannelHandler* handler,
                        const google::protobuf::MessageLite& message);

  void OnBootstrapRequest(const ExoBootstrapMessage& request);
  void SendBootstrapResponse(const ExoBootstrapMessage& request);

  std::unique_ptr<cast_api_bindings::MessagePort> port_;
  std::optional<DisplayInfo> display_info_;
  BootstrapCallback bootstrap_cb_;

  struct PendingMessage {
    PendingMessage(
        std::string_view msg,
        std::vector<std::unique_ptr<cast_api_bindings::MessagePort>> ports);
    ~PendingMessage();
    PendingMessage(PendingMessage&&);
    PendingMessage& operator=(PendingMessage&&);

    std::string message;
    std::vector<std::unique_ptr<cast_api_bindings::MessagePort>> ports;
  };

  raw_ptr<cast_api_bindings::MessagePort::Receiver> receiver_ = nullptr;
  std::vector<PendingMessage> pending_messages_;
  std::string last_sender_id_ = "SystemSender";
  int64_t next_transaction_id_ = 1;

  std::unique_ptr<SubChannelHandler> input_event_handler_;
  std::unique_ptr<SubChannelHandler> input_capabilities_handler_;

  base::WeakPtrFactory<StreamingReceiverChannel> weak_factory_{this};
};

}  // namespace cast_receiver

#endif  // COMPONENTS_CAST_RECEIVER_BROWSER_STREAMING_RECEIVER_CHANNEL_H_
