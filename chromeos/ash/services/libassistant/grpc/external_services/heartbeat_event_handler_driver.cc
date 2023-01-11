// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/libassistant/grpc/external_services/heartbeat_event_handler_driver.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/sequence_checker.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/delegate/event_handler_interface.pb.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/delegate/libas_server_status.pb.h"

namespace ash::libassistant {

namespace {

using ::assistant::api::HeartbeatEventHandlerInterface;
using ::assistant::api::OnHeartbeatEventRequest;
using ::assistant::api::OnHeartbeatEventResponse;

}  // namespace

HeartbeatEventHandlerDriver::HeartbeatEventHandlerDriver(
    ::grpc::ServerBuilder* server_builder)
    : AsyncServiceDriver(server_builder) {
  server_builder_->RegisterService(&service_);
}

HeartbeatEventHandlerDriver::~HeartbeatEventHandlerDriver() = default;

void HeartbeatEventHandlerDriver::AddObserver(
    GrpcServicesObserver<::assistant::api::OnHeartbeatEventRequest>* observer) {
  observers_.AddObserver(observer);
}

void HeartbeatEventHandlerDriver::RemoveObserver(
    GrpcServicesObserver<::assistant::api::OnHeartbeatEventRequest>* observer) {
  observers_.RemoveObserver(observer);
}

void HeartbeatEventHandlerDriver::StartCQ(::grpc::ServerCompletionQueue* cq) {
  on_event_rpc_method_driver_ = std::make_unique<
      RpcMethodDriver<OnHeartbeatEventRequest, OnHeartbeatEventResponse>>(
      cq,
      base::BindRepeating(&HeartbeatEventHandlerInterface::AsyncService::
                              RequestOnEventFromLibas,
                          async_service_weak_factory_.GetWeakPtr()),
      base::BindRepeating(&HeartbeatEventHandlerDriver::HandleEvent,
                          weak_factory_.GetWeakPtr()));
}

void HeartbeatEventHandlerDriver::HandleEvent(
    grpc::ServerContext* context,
    const OnHeartbeatEventRequest* request,
    base::OnceCallback<void(const grpc::Status&,
                            const OnHeartbeatEventResponse&)> done) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  int heartbeat_count = request->heartbeat_num();
  DVLOG(3) << "Heartbeat from libassistant : " << heartbeat_count;
  OnHeartbeatEventResponse response;
  std::move(done).Run(grpc::Status::OK, response);

  // Parse heartbeat request.
  if (!request->has_current_server_status()) {
    LOG(ERROR) << "Received Libassistant heartbeat without server status";
    return;
  }

  for (auto& observer : observers_)
    observer.OnGrpcMessage(*request);
}

}  // namespace ash::libassistant
