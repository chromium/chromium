// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/runtime/browser/message_port_service.h"

#include "base/logging.h"
#include "base/task/bind_post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chromecast/cast_core/runtime/browser/message_port_handler.h"

namespace chromecast {

MessagePortService::MessagePortService(
    cast::v2::CoreMessagePortApplicationServiceStub* core_app_stub)
    : core_app_stub_(core_app_stub),
      task_runner_(base::SequencedTaskRunnerHandle::Get()) {
  DCHECK(core_app_stub_);
}

MessagePortService::~MessagePortService() = default;

cast::utils::GrpcStatusOr<cast::web::MessagePortStatus>
MessagePortService::HandleMessage(cast::web::Message message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  cast::web::MessagePortStatus response;
  const uint32_t channel_id = message.channel().channel_id();
  auto entry = ports_.find(channel_id);
  if (entry == ports_.end()) {
    DLOG(INFO) << "Got message for unknown channel: " << channel_id;
    response.set_status(cast::web::MessagePortStatus_Status_ERROR);
  } else if (entry->second->HandleMessage(message)) {
    response.set_status(cast::web::MessagePortStatus_Status_OK);
  } else {
    response.set_status(cast::web::MessagePortStatus_Status_ERROR);
  }
  return response;
}

void MessagePortService::ConnectToPort(
    base::StringPiece port_name,
    std::unique_ptr<cast_api_bindings::MessagePort> port) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DLOG(INFO) << "MessagePortService connecting to port '" << port_name
             << "' as channel " << next_outgoing_channel_id_;

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
      base::BindOnce(&MessagePortService::OnPortConnectionEstablished,
                     weak_factory_.GetWeakPtr(), channel_id)));
}

uint32_t MessagePortService::RegisterOutgoingPort(
    std::unique_ptr<cast_api_bindings::MessagePort> port) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  uint32_t channel_id = next_outgoing_channel_id_++;
  ports_.emplace(channel_id,
                 MakeMessagePortHandler(channel_id, std::move(port)));
  return channel_id;
}

void MessagePortService::RegisterIncomingPort(
    uint32_t channel_id,
    std::unique_ptr<cast_api_bindings::MessagePort> port) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto result = ports_.emplace(
      channel_id, MakeMessagePortHandler(channel_id, std::move(port)));
  DCHECK(result.second);
}

void MessagePortService::Remove(uint32_t channel_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ports_.erase(channel_id);
}

std::unique_ptr<MessagePortHandler> MessagePortService::MakeMessagePortHandler(
    uint32_t channel_id,
    std::unique_ptr<cast_api_bindings::MessagePort> port) {
  auto port_handler = std::make_unique<MessagePortHandler>(
      std::move(port), channel_id, this, core_app_stub_, task_runner_);
  return port_handler;
}

void MessagePortService::OnPortConnectionEstablished(
    uint32_t channel_id,
    cast::utils::GrpcStatusOr<cast::bindings::ConnectResponse> response_or) {
  if (!response_or.ok()) {
    LOG(ERROR) << "Message port connect failed: channel_id=" << channel_id;
    Remove(channel_id);
  }
}

}  // namespace chromecast
