// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_receiver/browser/streaming_receiver_channel.h"

#include <google/protobuf/message_lite.h>

#include <string>
#include <utility>

#include "base/base64.h"
#include "base/check.h"
#include "base/logging.h"
#include "components/cast/message_port/platform_message_port.h"
#include "components/cast_receiver/browser/public/message_port_service.h"
#include "components/cast_receiver/proto/input_capabilities.pb.h"
#include "components/cast_receiver/proto/input_event.pb.h"

namespace cast_receiver {

StreamingReceiverChannel::StreamingReceiverChannel(
    MessagePortService* message_port_service) {
  CHECK(message_port_service);

  // Set up input event channel.
  std::unique_ptr<cast_api_bindings::MessagePort> input_event_server_port;
  std::unique_ptr<cast_api_bindings::MessagePort> input_event_client_port;
  cast_api_bindings::CreatePlatformMessagePortPair(&input_event_client_port,
                                                   &input_event_server_port);
  message_port_service->ConnectToPortAsync(kInputEventChannelNamespace,
                                           std::move(input_event_client_port));
  input_event_handler_ = std::make_unique<PortHandler>(
      "InputEvent", std::move(input_event_server_port));

  // Set up input capabilities channel.
  std::unique_ptr<cast_api_bindings::MessagePort> input_caps_server_port;
  std::unique_ptr<cast_api_bindings::MessagePort> input_caps_client_port;
  cast_api_bindings::CreatePlatformMessagePortPair(&input_caps_client_port,
                                                   &input_caps_server_port);
  message_port_service->ConnectToPortAsync(kInputCapabilitiesChannelNamespace,
                                           std::move(input_caps_client_port));
  input_capabilities_handler_ = std::make_unique<PortHandler>(
      "InputCapabilities", std::move(input_caps_server_port));
}

StreamingReceiverChannel::~StreamingReceiverChannel() = default;

void StreamingReceiverChannel::SendInputEvent(const InputEvent& event) {
  SendProtoMessage(input_event_handler_.get(), event);
}

void StreamingReceiverChannel::SendInputCapabilities(
    const InputCapabilities& capabilities) {
  SendProtoMessage(input_capabilities_handler_.get(), capabilities);
}

void StreamingReceiverChannel::SendProtoMessage(
    PortHandler* handler,
    const google::protobuf::MessageLite& message) {
  auto* port = handler->port();
  if (!port->CanPostMessage()) {
    LOG(WARNING) << "Cannot send message, port is closed or invalid.";
    return;
  }

  std::string serialized;
  if (!message.SerializeToString(&serialized)) {
    LOG(ERROR) << "Failed to serialize protobuf message.";
    return;
  }

  port->PostMessage(base::Base64Encode(serialized));
}

StreamingReceiverChannel::PortHandler::PortHandler(
    std::string_view name,
    std::unique_ptr<cast_api_bindings::MessagePort> port)
    : name_(name), port_(std::move(port)) {
  CHECK(port_);
  port_->SetReceiver(this);
}

StreamingReceiverChannel::PortHandler::~PortHandler() {
  port_->Close();
}

bool StreamingReceiverChannel::PortHandler::OnMessage(
    std::string_view message,
    std::vector<std::unique_ptr<cast_api_bindings::MessagePort>> ports) {
  // TODO(b/501522411): Handle messages received from the sender.
  return true;
}

void StreamingReceiverChannel::PortHandler::OnPipeError() {
  LOG(WARNING) << "Streaming Receiver Channel pipe error on port: " << name_;
}

}  // namespace cast_receiver
