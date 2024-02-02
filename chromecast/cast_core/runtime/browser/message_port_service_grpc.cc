// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/runtime/browser/message_port_service_grpc.h"

#include <sstream>
#include <string_view>

#include "base/logging.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "chromecast/cast_core/runtime/browser/message_port_handler.h"

namespace chromecast {

MessagePortServiceGrpc::MessagePortServiceGrpc(
    cast::v2::CoreMessagePortApplicationServiceStub* core_app_stub)
    : core_app_stub_(core_app_stub),
      task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {
  DCHECK(core_app_stub_);
}

MessagePortServiceGrpc::~MessagePortServiceGrpc() = default;

cast_receiver::Status MessagePortServiceGrpc::HandleMessage(
    cast::web::Message message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  cast::web::MessagePortStatus response;
  const uint32_t channel_id = message.channel().channel_id();
  auto entry = ports_.find(channel_id);
  if (entry == ports_.end()) {
    std::stringstream error_ss;
    error_ss << "Got message for unknown channel: " << channel_id;
    return cast_receiver::Status(cast_receiver::StatusCode::kUnknown,
                                 error_ss.str());
  }

  return entry->second->HandleMessage(message);
}

void MessagePortServiceGrpc::ConnectToPortAsync(
    std::string_view port_name,
    std::unique_ptr<cast_api_bindings::MessagePort> port) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DLOG(INFO) << "Connecting to port '" << port_name << "' as channel "
             << next_outgoing_channel_id_;

  const uint32_t channel_id = next_outgoing_channel_id_++;
  auto result = ports_.emplace(
      channel_id, MakeMessagePortHandler(channel_id, std::move(port)));
  DCHECK(result.second);

  auto call = core_app_stub_->CreateCall<
      cast::v2::CoreMessagePortApplicationServiceStub::Connect>();
  call.request().set_port_name(std::string(port_name));
  cast::web::MessagePortDescriptor* port_descriptor =
      call.request().mutable_port();
  port_descriptor->mutable_channel()->set_channel_id(channel_id);
  port_descriptor->mutable_peer_status()->set_status(
      cast::web::MessagePortStatus_Status_STARTED);
  port_descriptor->set_sequence_number(0);

  std::move(call).InvokeAsync(base::BindPostTask(
      task_runner_,
      base::BindOnce(&MessagePortServiceGrpc::OnPortConnectionEstablished,
                     weak_factory_.GetWeakPtr(), channel_id)));
}

uint32_t MessagePortServiceGrpc::RegisterOutgoingPort(
    std::unique_ptr<cast_api_bindings::MessagePort> port) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  uint32_t channel_id = next_outgoing_channel_id_++;
  ports_.emplace(channel_id,
                 MakeMessagePortHandler(channel_id, std::move(port)));
  return channel_id;
}

void MessagePortServiceGrpc::RegisterIncomingPort(
    uint32_t channel_id,
    std::unique_ptr<cast_api_bindings::MessagePort> port) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto result = ports_.emplace(
      channel_id, MakeMessagePortHandler(channel_id, std::move(port)));
  DCHECK(result.second);
}

void MessagePortServiceGrpc::Remove(uint32_t channel_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ports_.erase(channel_id);
}

std::unique_ptr<MessagePortHandler>
MessagePortServiceGrpc::MakeMessagePortHandler(
    uint32_t channel_id,
    std::unique_ptr<cast_api_bindings::MessagePort> port) {
  auto port_handler = std::make_unique<MessagePortHandler>(
      std::move(port), channel_id, this, core_app_stub_, task_runner_);
  return port_handler;
}

void MessagePortServiceGrpc::OnPortConnectionEstablished(
    uint32_t channel_id,
    cast::utils::GrpcStatusOr<cast::bindings::ConnectResponse> response_or) {
  if (!response_or.ok()) {
    LOG(ERROR) << "Message port connect failed: channel_id=" << channel_id;
    Remove(channel_id);
  }
}

}  // namespace chromecast
