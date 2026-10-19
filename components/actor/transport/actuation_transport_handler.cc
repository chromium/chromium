// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/actor/transport/actuation_transport_handler.h"

#include <tuple>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/notimplemented.h"
#include "base/strings/string_number_conversions.h"
#include "components/actor/core/task_id.h"
#include "components/actor/transport/proto/actuation_transport.pb.h"
#include "components/browser_actuator/public/common.h"
#include "components/browser_actuator/public/transport_session.h"

namespace actor {

ActuationTransportHandler::ActuationTransportHandler(
    browser_actuator::TransportSession* session,
    std::unique_ptr<ActuationDelegate> delegate)
    : browser_actuator::TransportHandler(session),
      delegate_(std::move(delegate)) {
  CHECK(session);
  CHECK(delegate_);
}

ActuationTransportHandler::~ActuationTransportHandler() {
  for (TaskId task_id : active_tasks_) {
    delegate_->StopTask(task_id, base::DoNothing());
  }
}

void ActuationTransportHandler::OnMessage(
    browser_actuator::PayloadType payload_type,
    std::string_view serialized_payload) {
  if (payload_type != browser_actuator::PayloadType::kActuation) {
    return;
  }

  proto::ServerToBrowserPayload message;
  if (!message.ParseFromString(serialized_payload)) {
    return;
  }

  switch (message.payload_case()) {
    case proto::ServerToBrowserPayload::kStartTask:
      HandleStartTask(message.request_id(), message.start_task());
      break;
    case proto::ServerToBrowserPayload::kPerformActions:
    case proto::ServerToBrowserPayload::kStopTask:
      // TODO(crbug.com/574548021): Route perform actions and stop task
      // payloads.
      NOTIMPLEMENTED();
      break;
    case proto::ServerToBrowserPayload::kPauseTask:
    case proto::ServerToBrowserPayload::kResumeTask:
      // TODO(crbug.com/574548405): Route pause and resume task payloads.
      NOTIMPLEMENTED();
      break;
    case proto::ServerToBrowserPayload::PAYLOAD_NOT_SET:
      break;
  }
}

void ActuationTransportHandler::HandleStartTask(
    const std::string& request_id,
    const proto::StartTaskRequest& request) {
  delegate_->StartTask(
      request, base::BindOnce(&ActuationTransportHandler::OnStartTaskComplete,
                              weak_ptr_factory_.GetWeakPtr(), request_id));
}

void ActuationTransportHandler::OnStartTaskComplete(
    const std::string& request_id,
    TaskId task_id,
    proto::StartTaskResponse response) {
  proto::BrowserToServerPayload upstream;
  upstream.set_request_id(request_id);
  if (response.status() == proto::StartTaskResponse::STATUS_OK && task_id) {
    active_tasks_.insert(task_id);
    upstream.set_task_id(base::NumberToString(task_id.value()));
  }
  *upstream.mutable_start_task() = std::move(response);
  std::ignore =
      SendUpstreamMessage(browser_actuator::PayloadType::kActuation, upstream);
}

}  // namespace actor
