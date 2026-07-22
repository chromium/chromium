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
#include "base/task/sequenced_task_runner.h"
#include "components/cast/message_port/platform_message_port.h"
#include "components/cast_receiver/browser/public/message_port_service.h"
#include "components/cast_receiver/proto/input_capabilities.pb.h"
#include "components/cast_receiver/proto/input_event.pb.h"

namespace cast_receiver {
namespace {

constexpr char kInputEventServiceName[] = "InputEvent";
constexpr char kInputCapabilitiesServiceName[] = "InputCapabilities";

std::string GetLabelForService(const ExoBootstrapMessage& message,
                               std::string_view service_name) {
  if (!message.has_bootstrap_info()) {
    return "";
  }
  for (const auto& res : message.bootstrap_info().resolutions()) {
    if (res.has_service_identifier() &&
        res.service_identifier().service_name() == service_name) {
      for (const auto& opt : res.transport_options()) {
        if (opt.has_data_channel_options() &&
            !opt.data_channel_options().label().empty()) {
          return opt.data_channel_options().label();
        }
      }
    }
  }
  return "";
}

}  // namespace

StreamingReceiverChannel::StreamingReceiverChannel(
    MessagePortService* message_port_service,
    std::optional<DisplayInfo> display_info,
    BootstrapCallback bootstrap_cb)
    : message_port_service_(message_port_service),
      display_info_(std::move(display_info)),
      bootstrap_cb_(std::move(bootstrap_cb)) {
  CHECK(message_port_service_);
  CHECK(bootstrap_cb_);

  std::unique_ptr<cast_api_bindings::MessagePort> control_server_port;
  std::unique_ptr<cast_api_bindings::MessagePort> control_client_port;
  cast_api_bindings::CreatePlatformMessagePortPair(&control_client_port,
                                                   &control_server_port);
  message_port_service_->ConnectToPortAsync(
      "urn:x-cast:com.google.cast.exo.bootstrap",
      std::move(control_client_port));

  bootstrap_handler_ =
      std::make_unique<BootstrapHandler>(this, std::move(control_server_port));
}

StreamingReceiverChannel::~StreamingReceiverChannel() = default;

void StreamingReceiverChannel::SendInputEvent(const InputEvent& event) {
  SendProtoMessage(input_event_handler_.get(), event);
}

void StreamingReceiverChannel::SendInputCapabilities(
    const InputCapabilities& capabilities) {
  SendProtoMessage(input_capabilities_handler_.get(), capabilities);
}

bool StreamingReceiverChannel::SendProtoMessage(
    SubChannelHandler* handler,
    const google::protobuf::MessageLite& message) {
  if (!handler) {
    LOG(WARNING) << "Sub-channel is not connected.";
    return false;
  }
  auto* port = handler->port();
  if (!port->CanPostMessage()) {
    LOG(WARNING) << "Cannot send message, port is closed or invalid.";
    return false;
  }

  std::string serialized;
  CHECK(message.SerializeToString(&serialized));

  return port->PostMessage(base::Base64Encode(serialized));
}

std::unique_ptr<StreamingReceiverChannel::SubChannelHandler>
StreamingReceiverChannel::ConnectSubChannel(std::string_view namespace_name) {
  std::unique_ptr<cast_api_bindings::MessagePort> server_port;
  std::unique_ptr<cast_api_bindings::MessagePort> client_port;
  cast_api_bindings::CreatePlatformMessagePortPair(&client_port, &server_port);
  message_port_service_->ConnectToPortAsync(namespace_name,
                                            std::move(client_port));
  return std::make_unique<SubChannelHandler>(namespace_name,
                                             std::move(server_port));
}

void StreamingReceiverChannel::OnBootstrapRequest(
    const ExoBootstrapMessage& request) {
  if (!request.has_bootstrap_action() ||
      request.bootstrap_action().action() != BootstrapAction::START_BOOTSTRAP) {
    return;
  }

  // Connect sub-channels.
  std::string event_label = GetLabelForService(request, kInputEventServiceName);
  if (!event_label.empty()) {
    input_event_handler_ = ConnectSubChannel(event_label);
  }

  std::string caps_label =
      GetLabelForService(request, kInputCapabilitiesServiceName);
  if (!caps_label.empty()) {
    input_capabilities_handler_ = ConnectSubChannel(caps_label);
  }

  SendBootstrapResponse(request);
}

void StreamingReceiverChannel::SendBootstrapResponse(
    const ExoBootstrapMessage& request) {
  ExoBootstrapMessage response = request;
  auto* launch_info =
      response.mutable_bootstrap_action()->mutable_launch_info();
  if (display_info_) {
    *launch_info->mutable_display_info() = *display_info_;
  }

  // Populate negotiated resolutions (labels) for supported services.
  auto* bootstrap_info = response.mutable_bootstrap_info();
  bootstrap_info->clear_resolutions();

  if (input_event_handler_) {
    auto* input_event_res = bootstrap_info->add_resolutions();
    input_event_res->mutable_service_identifier()->set_service_name(
        kInputEventServiceName);
    input_event_res->add_transport_options()
        ->mutable_data_channel_options()
        ->set_label(input_event_handler_->name());
  }

  if (input_capabilities_handler_) {
    auto* input_caps_res = bootstrap_info->add_resolutions();
    input_caps_res->mutable_service_identifier()->set_service_name(
        kInputCapabilitiesServiceName);
    input_caps_res->add_transport_options()
        ->mutable_data_channel_options()
        ->set_label(input_capabilities_handler_->name());
  }

  std::string serialized;
  CHECK(response.SerializeToString(&serialized));

  std::string encoded = base::Base64Encode(serialized);

  if (!bootstrap_handler_->port()->PostMessage(encoded)) {
    LOG(ERROR) << "Failed to send bootstrap response";
    return;
  }

  // Post task to notify complete to avoid deleting `this` inside OnMessage.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&StreamingReceiverChannel::NotifyComplete,
                     weak_factory_.GetWeakPtr(), std::move(response)));
}

void StreamingReceiverChannel::NotifyComplete(ExoBootstrapMessage request) {
  if (bootstrap_cb_) {
    std::move(bootstrap_cb_).Run(std::move(request));
  }
}

StreamingReceiverChannel::SubChannelHandler::SubChannelHandler(
    std::string_view name,
    std::unique_ptr<cast_api_bindings::MessagePort> port)
    : name_(name), port_(std::move(port)) {
  CHECK(port_);
  port_->SetReceiver(this);
}

StreamingReceiverChannel::SubChannelHandler::~SubChannelHandler() {
  port_->Close();
}

bool StreamingReceiverChannel::SubChannelHandler::OnMessage(
    std::string_view message,
    std::vector<std::unique_ptr<cast_api_bindings::MessagePort>> ports) {
  // TODO(b/501522411): Handle messages received from the sender.
  return true;
}

void StreamingReceiverChannel::SubChannelHandler::OnPipeError() {
  LOG(WARNING) << "Streaming Receiver Channel pipe error on port: " << name_;
}

StreamingReceiverChannel::BootstrapHandler::BootstrapHandler(
    StreamingReceiverChannel* owner,
    std::unique_ptr<cast_api_bindings::MessagePort> port)
    : owner_(owner), port_(std::move(port)) {
  CHECK(owner_);
  CHECK(port_);
  port_->SetReceiver(this);
}

StreamingReceiverChannel::BootstrapHandler::~BootstrapHandler() {
  if (port_) {
    port_->Close();
  }
}

bool StreamingReceiverChannel::BootstrapHandler::OnMessage(
    std::string_view message,
    std::vector<std::unique_ptr<cast_api_bindings::MessagePort>> ports) {
  std::string decoded_message;
  if (!base::Base64Decode(message, &decoded_message)) {
    LOG(ERROR) << "Failed to Base64 decode bootstrap message";
    return false;
  }

  ExoBootstrapMessage request;
  if (!request.ParseFromString(decoded_message)) {
    LOG(ERROR) << "Failed to parse ExoBootstrapMessage";
    return false;
  }

  owner_->OnBootstrapRequest(request);
  return true;
}

void StreamingReceiverChannel::BootstrapHandler::OnPipeError() {
  LOG(WARNING) << "Bootstrap channel pipe error.";
}

}  // namespace cast_receiver
