// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/runtime/browser/message_port_service.h"

#include "base/logging.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chromecast/cast_core/runtime/browser/message_port_handler.h"

namespace chromecast {

MessagePortService::MessagePortService(
    grpc::CompletionQueue* grpc_cq,
    cast::v2::CoreApplicationService::Stub* core_app_stub)
    : grpc_cq_(grpc_cq), core_app_stub_(core_app_stub) {
  DCHECK(grpc_cq_);
  DCHECK(core_app_stub_);
}

MessagePortService::~MessagePortService() = default;

void MessagePortService::HandleMessage(const cast::web::Message& message,
                                       cast::web::MessagePortStatus* response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  uint32_t channel_id = message.channel().channel_id();
  auto entry = ports_.find(channel_id);
  if (entry == ports_.end()) {
    DLOG(INFO) << "Got message for unknown channel: " << channel_id;
    response->set_status(cast::web::MessagePortStatus_Status_ERROR);
    return;
  }

  if (entry->second->HandleMessage(message)) {
    response->set_status(cast::web::MessagePortStatus_Status_OK);
  } else {
    response->set_status(cast::web::MessagePortStatus_Status_ERROR);
  }
}

bool MessagePortService::ConnectToPort(
    base::StringPiece port_name,
    std::unique_ptr<cast_api_bindings::MessagePort> port) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DLOG(INFO) << "MessagePortService connecting to port '" << port_name
             << "' as channel " << next_outgoing_channel_id_;
  cast::web::MessagePortDescriptor port_descriptor;
  uint32_t channel_id = next_outgoing_channel_id_++;
  port_descriptor.mutable_channel()->set_channel_id(channel_id);
  port_descriptor.mutable_peer_status()->set_status(
      cast::web::MessagePortStatus_Status_STARTED);
  port_descriptor.set_sequence_number(0);

  cast::bindings::ConnectRequest connect_request;
  connect_request.set_port_name(std::string(port_name));
  *connect_request.mutable_port() = port_descriptor;
  auto result = ports_.emplace(
      channel_id, MakeMessagePortHandler(channel_id, std::move(port)));
  DCHECK(result.second);

  new AsyncConnect(connect_request, core_app_stub_, grpc_cq_,
                   weak_factory_.GetWeakPtr());
  return true;
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
      std::move(port), channel_id, this, grpc_cq_, core_app_stub_,
      base::SequencedTaskRunnerHandle::Get());
  return port_handler;
}

void MessagePortService::OnConnectComplete(bool ok, uint32_t channel_id) {
  if (!ok) {
    DLOG(INFO) << "CoreApplicationService::Connect failed";
    Remove(channel_id);
  }
}

MessagePortService::AsyncConnect::AsyncConnect(
    const cast::bindings::ConnectRequest& request,
    cast::v2::CoreApplicationService::Stub* core_app_stub,
    grpc::CompletionQueue* cq,
    base::WeakPtr<MessagePortService> service)
    : service_(service), channel_id_(request.port().channel().channel_id()) {
  response_reader_ = core_app_stub->PrepareAsyncConnect(&context_, request, cq);
  response_reader_->StartCall();
  response_reader_->Finish(&response_, &status_, static_cast<GRPC*>(this));
}

MessagePortService::AsyncConnect::~AsyncConnect() = default;

void MessagePortService::AsyncConnect::StepGRPC(grpc::Status status) {
  if (service_) {
    service_->OnConnectComplete(status_.ok(), channel_id_);
  }
  delete this;
}

}  // namespace chromecast
