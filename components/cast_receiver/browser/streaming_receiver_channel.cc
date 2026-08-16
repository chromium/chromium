// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_receiver/browser/streaming_receiver_channel.h"

#include <google/protobuf/message_lite.h>

#include <optional>
#include <string>
#include <utility>

#include "base/base64.h"
#include "base/check.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "components/cast_receiver/proto/input_capabilities.pb.h"
#include "components/cast_receiver/proto/input_event.pb.h"
#include "components/cast_streaming/common/message_serialization.h"

namespace cast_receiver {
namespace {

constexpr char kExoBootstrapNamespace[] =
    "urn:x-cast:com.google.cast.exo.bootstrap";
constexpr char kExoInputNamespace[] = "urn:x-cast:com.google.cast.exo.input";
constexpr char kExoCapabilityNamespace[] =
    "urn:x-cast:com.google.cast.exo.capability";

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
    std::unique_ptr<cast_api_bindings::MessagePort> port,
    std::optional<DisplayInfo> display_info,
    BootstrapCallback bootstrap_cb)
    : port_(std::move(port)),
      display_info_(std::move(display_info)),
      bootstrap_cb_(std::move(bootstrap_cb)) {
  CHECK(port_);
  CHECK(bootstrap_cb_);
  port_->SetReceiver(this);
}

StreamingReceiverChannel::~StreamingReceiverChannel() = default;

void StreamingReceiverChannel::SendMessage(
    const std::string& sender_id,
    const std::string& message_namespace,
    const std::string& message) {
  if (port_->CanPostMessage()) {
    port_->PostMessage(
        cast_streaming::SerializeCastMessage(sender_id, message_namespace, message));
  }
}

bool StreamingReceiverChannel::PostMessage(std::string_view message) {
  return port_->PostMessage(message);
}

bool StreamingReceiverChannel::PostMessageWithTransferables(
    std::string_view message,
    std::vector<std::unique_ptr<cast_api_bindings::MessagePort>> ports) {
  return port_->PostMessageWithTransferables(message, std::move(ports));
}

void StreamingReceiverChannel::SetReceiver(
    cast_api_bindings::MessagePort::Receiver* receiver) {
  receiver_ = receiver;
  if (receiver_) {
    for (auto& pending : pending_messages_) {
      receiver_->OnMessage(pending.message, std::move(pending.ports));
    }
    pending_messages_.clear();
  }
}

void StreamingReceiverChannel::Close() {
  port_->Close();
}

bool StreamingReceiverChannel::CanPostMessage() const {
  return port_->CanPostMessage();
}

bool StreamingReceiverChannel::OnMessage(
    std::string_view message,
    std::vector<std::unique_ptr<cast_api_bindings::MessagePort>> ports) {
  std::string sender_id;
  std::string message_namespace;
  std::string data;

  if (cast_streaming::DeserializeCastMessage(message, &sender_id,
                                             &message_namespace, &data)) {
    if (!sender_id.empty()) {
      last_sender_id_ = sender_id;
    }

    if (message_namespace == kExoBootstrapNamespace) {
      std::string decoded;
      if (!base::Base64Decode(data, &decoded)) {
        LOG(ERROR) << "Failed to Base64 decode Exo bootstrap payload.";
        return false;
      }
      ExoBootstrapMessage request;
      if (!request.ParseFromString(decoded)) {
        LOG(ERROR) << "Failed to parse ExoBootstrapMessage proto.";
        return false;
      }
      OnBootstrapRequest(request);
      return true;
    }

    std::string input_namespace = input_event_handler_
                                      ? input_event_handler_->message_namespace()
                                      : kExoInputNamespace;
    std::string caps_namespace = input_capabilities_handler_
                                     ? input_capabilities_handler_->message_namespace()
                                     : kExoCapabilityNamespace;

    if (message_namespace != input_namespace &&
        message_namespace != caps_namespace &&
        message_namespace != kExoBootstrapNamespace) {
      if (receiver_) {
        return receiver_->OnMessage(message, std::move(ports));
      }
      pending_messages_.emplace_back(message, std::move(ports));
      return true;
    }
    return true;
  }

  if (receiver_) {
    return receiver_->OnMessage(message, std::move(ports));
  }
  pending_messages_.emplace_back(message, std::move(ports));
  return true;
}

void StreamingReceiverChannel::OnPipeError() {
  if (receiver_) {
    receiver_->OnPipeError();
  }
}

void StreamingReceiverChannel::SendInputEvent(const InputEvent& event) {
  if (!input_event_handler_) {
    return;
  }

  std::string serialized_event;
  if (!event.SerializeToString(&serialized_event)) {
    return;
  }

  InputEventList event_list;
  event_list.set_transaction_id(next_transaction_id_++);
  event_list.add_serialized_past_events(serialized_event);

  SendProtoMessage(input_event_handler_.get(), event_list);
}

void StreamingReceiverChannel::SendInputCapabilities(
    const InputCapabilities& capabilities) {
  if (!input_capabilities_handler_) {
    return;
  }
  SendProtoMessage(input_capabilities_handler_.get(), capabilities);
}

bool StreamingReceiverChannel::SendProtoMessage(
    SubChannelHandler* handler,
    const google::protobuf::MessageLite& message) {
  if (!handler) {
    return false;
  }
  std::string ns = handler->message_namespace();
  if (ns.empty()) {
    ns = (handler->name() == kInputEventServiceName) ? kExoInputNamespace
                                                     : kExoCapabilityNamespace;
  }

  std::string serialized;
  if (!message.SerializeToString(&serialized)) {
    LOG(ERROR) << "Failed to serialize protobuf message.";
    return false;
  }

  SendMessage(last_sender_id_, ns, base::Base64Encode(serialized));
  return true;
}

std::unique_ptr<StreamingReceiverChannel::SubChannelHandler>
StreamingReceiverChannel::ConnectSubChannel(std::string_view name,
                                            std::string_view namespace_name) {
  return std::make_unique<SubChannelHandler>(name, namespace_name);
}

void StreamingReceiverChannel::OnBootstrapRequest(
    const ExoBootstrapMessage& request) {
  std::string event_label =
      GetLabelForService(request, kInputEventServiceName);
  if (event_label.empty()) {
    event_label = kExoInputNamespace;
  }
  input_event_handler_ =
      ConnectSubChannel(kInputEventServiceName, event_label);

  std::string caps_label =
      GetLabelForService(request, kInputCapabilitiesServiceName);
  if (caps_label.empty()) {
    caps_label = kExoCapabilityNamespace;
  }
  input_capabilities_handler_ =
      ConnectSubChannel(kInputCapabilitiesServiceName, caps_label);

  if (request.has_bootstrap_action() &&
      request.bootstrap_action().action() ==
          BootstrapAction::START_BOOTSTRAP) {
    SendBootstrapResponse(request);
  }
}

void StreamingReceiverChannel::SendBootstrapResponse(
    const ExoBootstrapMessage& request) {
  ExoBootstrapMessage response = request;
  response.mutable_bootstrap_action()->set_action(
      BootstrapAction::START_BOOTSTRAP);

  if (display_info_) {
    *response.mutable_bootstrap_action()
         ->mutable_launch_info()
         ->mutable_display_info() = *display_info_;
  }

  auto* bootstrap_info = response.mutable_bootstrap_info();
  bootstrap_info->clear_resolutions();
  if (input_event_handler_) {
    auto* input_event_res = bootstrap_info->add_resolutions();
    input_event_res->mutable_service_identifier()->set_service_name(
        kInputEventServiceName);
    input_event_res->add_transport_options()
        ->mutable_data_channel_options()
        ->set_label(input_event_handler_->message_namespace());
  }

  if (input_capabilities_handler_) {
    auto* input_caps_res = bootstrap_info->add_resolutions();
    input_caps_res->mutable_service_identifier()->set_service_name(
        kInputCapabilitiesServiceName);
    input_caps_res->add_transport_options()
        ->mutable_data_channel_options()
        ->set_label(input_capabilities_handler_->message_namespace());
  }

  std::string serialized;
  if (response.SerializeToString(&serialized)) {
    SendMessage(last_sender_id_, kExoBootstrapNamespace,
                base::Base64Encode(serialized));
  }

  if (bootstrap_cb_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(bootstrap_cb_), std::move(response)));
  }
}

StreamingReceiverChannel::SubChannelHandler::SubChannelHandler(
    std::string_view name,
    std::string_view message_namespace)
    : name_(name), namespace_(message_namespace) {}

StreamingReceiverChannel::SubChannelHandler::~SubChannelHandler() = default;

StreamingReceiverChannel::PendingMessage::PendingMessage(
    std::string_view msg,
    std::vector<std::unique_ptr<cast_api_bindings::MessagePort>> ports)
    : message(msg), ports(std::move(ports)) {}

StreamingReceiverChannel::PendingMessage::~PendingMessage() = default;
StreamingReceiverChannel::PendingMessage::PendingMessage(
    PendingMessage&&) = default;
StreamingReceiverChannel::PendingMessage&
StreamingReceiverChannel::PendingMessage::operator=(PendingMessage&&) = default;

}  // namespace cast_receiver
