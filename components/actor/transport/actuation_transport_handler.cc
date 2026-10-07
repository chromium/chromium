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
#include "components/actor/core/task_id.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "components/actor/transport/proto/actuation_transport.pb.h"
#include "components/browser_actuator/public/common.h"
#include "components/browser_actuator/public/transport_session.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"

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

  proto::ActuationDownstreamMessage message;
  if (!message.ParseFromString(serialized_payload)) {
    return;
  }

  switch (message.payload_case()) {
    case proto::ActuationDownstreamMessage::kStartTask:
      HandleStartTask(message.request_id(), message.start_task());
      break;
    case proto::ActuationDownstreamMessage::kStopTask:
      HandleStopTask(message.request_id(), message.stop_task());
      break;
    case proto::ActuationDownstreamMessage::kActions:
      HandleAct(message.request_id(), message.actions());
      break;
    case proto::ActuationDownstreamMessage::PAYLOAD_NOT_SET:
      break;
  }
}

void ActuationTransportHandler::HandleStartTask(
    const std::string& request_id,
    const optimization_guide::proto::BrowserStartTask& request) {
  delegate_->StartTask(
      std::string(session()->GetSessionId()), request,
      base::BindOnce(&ActuationTransportHandler::OnStartTaskComplete,
                     weak_ptr_factory_.GetWeakPtr(), request_id));
}

void ActuationTransportHandler::OnStartTaskComplete(
    const std::string& request_id,
    optimization_guide::proto::BrowserStartTaskResult result) {
  if (result.status() ==
      optimization_guide::proto::BrowserStartTaskResult::SUCCESS) {
    active_tasks_.insert(TaskId(result.task_id()));
  }
  proto::ActuationUpstreamMessage response;
  response.set_request_id(request_id);
  *response.mutable_start_task_result() = std::move(result);
  std::ignore =
      SendUpstreamMessage(browser_actuator::PayloadType::kActuation, response);
}

void ActuationTransportHandler::HandleStopTask(const std::string& request_id,
                                               const proto::StopTask& request) {
  TaskId task_id(request.task_id());
  if (active_tasks_.erase(task_id) == 0) {
    OnStopTaskComplete(request_id, task_id, false);
    return;
  }
  delegate_->StopTask(
      task_id,
      base::BindOnce(&ActuationTransportHandler::OnStopTaskComplete,
                     weak_ptr_factory_.GetWeakPtr(), request_id, task_id));
}

void ActuationTransportHandler::OnStopTaskComplete(
    const std::string& request_id,
    TaskId task_id,
    bool success) {
  proto::ActuationUpstreamMessage response;
  response.set_request_id(request_id);
  proto::StopTaskResult* stop_task_result = response.mutable_stop_task_result();
  stop_task_result->set_task_id(task_id.value());
  stop_task_result->set_status(
      success ? proto::StopTaskResult::STOP_TASK_STATUS_SUCCESS
              : proto::StopTaskResult::STOP_TASK_STATUS_FAILURE);
  std::ignore =
      SendUpstreamMessage(browser_actuator::PayloadType::kActuation, response);
}

void ActuationTransportHandler::HandleAct(
    const std::string& request_id,
    const optimization_guide::proto::Actions& actions) {
  if (!active_tasks_.contains(TaskId(actions.task_id()))) {
    optimization_guide::proto::ActionsResult result;
    result.set_action_result(
        static_cast<int32_t>(mojom::ActionResultCode::kTaskWentAway));
    OnActComplete(request_id, std::move(result));
    return;
  }
  delegate_->Act(actions,
                 base::BindOnce(&ActuationTransportHandler::OnActComplete,
                                weak_ptr_factory_.GetWeakPtr(), request_id));
}

void ActuationTransportHandler::OnActComplete(
    const std::string& request_id,
    optimization_guide::proto::ActionsResult result) {
  proto::ActuationUpstreamMessage response;
  response.set_request_id(request_id);
  *response.mutable_actions_result() = std::move(result);
  std::ignore =
      SendUpstreamMessage(browser_actuator::PayloadType::kActuation, response);
}

}  // namespace actor
