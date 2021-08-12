// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/libassistant/grpc/external_services/heartbeat_event_handler_driver.h"

#include <utility>

#include "base/bind.h"
#include "base/sequence_checker.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/delegate/event_handler_interface.pb.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/delegate/libas_server_status.pb.h"
#include "chromeos/services/libassistant/grpc/assistant_connection_status.h"

namespace chromeos {
namespace libassistant {

namespace {

using ::assistant::api::HeartbeatEventHandlerInterface;
using ::assistant::api::OnHeartbeatEventRequest;
using ::assistant::api::OnHeartbeatEventResponse;

bool ConvertServerStatus(::assistant::api::LibasServerStatus input,
                         AssistantConnectionStatus* output) {
  switch (input) {
    // We consider both states as booting up as a customer, although they are
    // two different states in Libassistant bootup protocol.
    case ::assistant::api::CUSTOMER_REGISTRATION_SERVICE_AVAILABLE:
    case ::assistant::api::ESSENTIAL_SERVICES_AVAILABLE:
      *output = AssistantConnectionStatus::ONLINE_BOOTING_UP;
      return true;

    case ::assistant::api::ALL_SERVICES_AVAILABLE:
      *output = AssistantConnectionStatus::ONLINE_ALL_SERVICES_AVAILABLE;
      return true;

    case ::assistant::api::UNKNOWN_LIBAS_SERVER_STATUS:
      return false;
  }
}

std::string GetServerStatusLogString(AssistantConnectionStatus status) {
  switch (status) {
    case AssistantConnectionStatus::OFFLINE:
      return "Libassistant service is OFFLINE.";
    case AssistantConnectionStatus::ONLINE_BOOTING_UP:
      return "Libassistant service is BOOTING UP.";
    case AssistantConnectionStatus::ONLINE_ALL_SERVICES_AVAILABLE:
      return "Libassistant service is ALL READY.";
  }
}

}  // namespace

HeartbeatEventHandlerDriver::HeartbeatEventHandlerDriver(
    ::grpc::ServerBuilder* server_builder)
    : AsyncServiceDriver(server_builder) {
  server_builder_->RegisterService(&service_);
}

HeartbeatEventHandlerDriver::~HeartbeatEventHandlerDriver() = default;

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

  AssistantConnectionStatus new_status;
  if (!ConvertServerStatus(request->current_server_status(), &new_status)) {
    LOG(ERROR) << "Received unknown Libassistant server status";
    return;
  }

  DVLOG(3) << GetServerStatusLogString(new_status);
}

}  // namespace libassistant
}  // namespace chromeos
