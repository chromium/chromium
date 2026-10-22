// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/actor/transport/actuation_transport_handler.h"

#include <optional>
#include <tuple>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/notimplemented.h"
#include "base/strings/string_number_conversions.h"
#include "components/actor/core/task_id.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "components/actor/transport/proto/actuation_transport.pb.h"
#include "components/browser_actuator/public/transport/common.h"
#include "components/browser_actuator/public/transport/transport_session.h"

namespace actor {
namespace {

std::optional<TaskId> ParseTaskId(std::string_view task_id_str) {
  int task_id_value = 0;
  if (!base::StringToInt(task_id_str, &task_id_value) || task_id_value <= 0) {
    return std::nullopt;
  }
  return TaskId(task_id_value);
}

}  // namespace

ActuationTransportHandler::ActuationTransportHandler(
    browser_actuator::transport::TransportSession* session,
    std::unique_ptr<ActuationDelegate> delegate)
    : browser_actuator::transport::TransportHandler(session),
      delegate_(std::move(delegate)) {
  CHECK(session);
  CHECK(delegate_);
}

ActuationTransportHandler::~ActuationTransportHandler() {
  proto::StopTaskRequest stop_request;
  stop_request.set_reason(proto::StopTaskRequest::REASON_UNSPECIFIED);
  for (TaskId task_id : active_tasks_) {
    delegate_->StopTask(task_id, stop_request, base::DoNothing());
  }
}

void ActuationTransportHandler::OnMessage(
    browser_actuator::transport::PayloadType payload_type,
    std::string_view serialized_payload) {
  if (payload_type != browser_actuator::transport::PayloadType::kActuation) {
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
      HandlePerformActions(message.request_id(), message.task_id(),
                           message.perform_actions());
      break;
    case proto::ServerToBrowserPayload::kStopTask:
      HandleStopTask(message.request_id(), message.task_id(),
                     message.stop_task());
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
  std::ignore = SendUpstreamMessage(
      browser_actuator::transport::PayloadType::kActuation, upstream);
}

void ActuationTransportHandler::HandleStopTask(
    const std::string& request_id,
    std::string_view task_id_str,
    const proto::StopTaskRequest& request) {
  std::optional<TaskId> task_id = ParseTaskId(task_id_str);
  if (!task_id || active_tasks_.erase(*task_id) == 0) {
    OnStopTaskComplete(request_id, std::string(task_id_str), false);
    return;
  }
  delegate_->StopTask(
      *task_id, request,
      base::BindOnce(&ActuationTransportHandler::OnStopTaskComplete,
                     weak_ptr_factory_.GetWeakPtr(), request_id,
                     std::string(task_id_str)));
}

void ActuationTransportHandler::OnStopTaskComplete(
    const std::string& request_id,
    const std::string& task_id_str,
    bool success) {
  proto::BrowserToServerPayload upstream;
  upstream.set_task_id(task_id_str);
  upstream.set_request_id(request_id);
  upstream.mutable_stop_task()->set_status(
      success ? proto::StopTaskResponse::STATUS_OK
              : proto::StopTaskResponse::STATUS_FAILED);
  std::ignore = SendUpstreamMessage(
      browser_actuator::transport::PayloadType::kActuation, upstream);
}

void ActuationTransportHandler::HandlePerformActions(
    const std::string& request_id,
    std::string_view task_id_str,
    const proto::PerformActionsRequest& request) {
  std::optional<TaskId> task_id = ParseTaskId(task_id_str);
  if (!task_id || !active_tasks_.contains(*task_id)) {
    proto::PerformActionsResponse response;
    response.set_status(proto::PerformActionsResponse::STATUS_TAB_DISCONNECTED);
    response.set_action_status_code(
        static_cast<int32_t>(mojom::ActionResultCode::kTaskWentAway));
    OnActComplete(request_id, std::string(task_id_str), std::move(response));
    return;
  }
  delegate_->Act(*task_id, request,
                 base::BindOnce(&ActuationTransportHandler::OnActComplete,
                                weak_ptr_factory_.GetWeakPtr(), request_id,
                                std::string(task_id_str)));
}

void ActuationTransportHandler::OnActComplete(
    const std::string& request_id,
    const std::string& task_id_str,
    proto::PerformActionsResponse response) {
  proto::BrowserToServerPayload upstream;
  upstream.set_task_id(task_id_str);
  upstream.set_request_id(request_id);
  *upstream.mutable_perform_actions() = std::move(response);
  std::ignore = SendUpstreamMessage(
      browser_actuator::transport::PayloadType::kActuation, upstream);
}

}  // namespace actor
