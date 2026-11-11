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
#include "base/logging.h"
#include "base/notimplemented.h"
#include "components/actor/core/task_id.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "components/actor/transport/proto/actuation_transport.pb.h"
#include "components/stream_transport/public/common.h"
#include "components/stream_transport/public/transport_session.h"

namespace actor {
namespace {

// Returns true when `action_status_code` indicates that the underlying
// `ActorTask` has already been stopped and removed from `ActorKeyedService`.
bool IsTaskStoppedResultCode(int32_t action_status_code) {
  switch (static_cast<mojom::ActionResultCode>(action_status_code)) {
    case mojom::ActionResultCode::kTaskWentAway:
    case mojom::ActionResultCode::kUserNavigatedAway:
      return true;
    default:
      return false;
  }
}

}  // namespace

ActuationTransportHandler::ActuationTransportHandler(
    stream_transport::TransportSession* session,
    std::unique_ptr<ActuationDelegate> delegate)
    : stream_transport::TransportHandler(session),
      delegate_(std::move(delegate)) {
  CHECK(session);
  CHECK(delegate_);
}

ActuationTransportHandler::~ActuationTransportHandler() {
  // Tasks whose StartTask is still pending have no TaskId yet, so they cannot
  // be stopped here. The delegate is destroyed along with this handler and, per
  // the ActuationDelegate contract, must not leave such a task running.
  proto::StopTaskRequest stop_request;
  stop_request.set_reason(proto::StopTaskRequest::REASON_UNSPECIFIED);
  for (const auto& [external_task_id, task_id] : active_tasks_) {
    delegate_->StopTask(task_id, stop_request, base::DoNothing());
  }
}

void ActuationTransportHandler::OnMessage(
    stream_transport::PayloadType payload_type,
    std::string_view serialized_payload) {
  if (payload_type != stream_transport::PayloadType::kActuation) {
    return;
  }

  proto::ServerToBrowserPayload message;
  if (!message.ParseFromString(serialized_payload)) {
    return;
  }

  switch (message.payload_case()) {
    case proto::ServerToBrowserPayload::kStartTask:
      HandleStartTask(message.request_id(), message.task_id(),
                      message.start_task());
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
    const std::string& external_task_id,
    const proto::StartTaskRequest& request) {
  // The server owns task_id and must set it on StartTask. Reject a missing ID,
  // or one already used by an active or still-starting task in this session,
  // without creating a Chrome task.
  std::optional<proto::StartTaskResponse::Status> rejection;
  if (external_task_id.empty()) {
    rejection = proto::StartTaskResponse::STATUS_MISSING_TASK_ID;
  } else if (active_tasks_.contains(external_task_id) ||
             pending_start_tasks_.contains(external_task_id)) {
    rejection = proto::StartTaskResponse::STATUS_DUPLICATE_TASK_ID;
  }
  if (rejection) {
    DVLOG(1) << "Rejecting StartTask with "
             << (external_task_id.empty() ? "missing" : "duplicate")
             << " task_id: '" << external_task_id << "'";
    proto::StartTaskResponse response;
    response.set_status(*rejection);
    SendStartTaskResponse(request_id, external_task_id, std::move(response));
    return;
  }

  // Track the ID until the delegate replies so a duplicate StartTask or a
  // StopTask that arrives before then is handled correctly, even if the
  // delegate completes asynchronously.
  pending_start_tasks_.emplace(external_task_id, std::nullopt);
  delegate_->StartTask(
      request, base::BindOnce(&ActuationTransportHandler::OnStartTaskComplete,
                              weak_ptr_factory_.GetWeakPtr(), request_id,
                              external_task_id));
}

void ActuationTransportHandler::OnStartTaskComplete(
    const std::string& request_id,
    const std::string& external_task_id,
    TaskId task_id,
    proto::StartTaskResponse response) {
  auto pending_it = pending_start_tasks_.find(external_task_id);
  CHECK(pending_it != pending_start_tasks_.end());
  std::optional<PendingStop> pending_stop = std::move(pending_it->second);
  pending_start_tasks_.erase(pending_it);

  const bool started =
      response.status() == proto::StartTaskResponse::STATUS_OK && task_id;
  if (started && !pending_stop) {
    // Duplicates were rejected in HandleStartTask() while this ID was pending,
    // so the insertion cannot collide.
    auto [it, inserted] = active_tasks_.emplace(external_task_id, task_id);
    CHECK(inserted);
  }
  SendStartTaskResponse(request_id, external_task_id, std::move(response));

  if (!pending_stop) {
    return;
  }
  if (!started) {
    // The task never started, so there is nothing to stop.
    OnStopTaskComplete(pending_stop->request_id, external_task_id, false);
    return;
  }
  // A StopTask arrived while the start was pending. Stop the new task now
  // instead of leaving it running.
  delegate_->StopTask(
      task_id, pending_stop->request,
      base::BindOnce(&ActuationTransportHandler::OnStopTaskComplete,
                     weak_ptr_factory_.GetWeakPtr(), pending_stop->request_id,
                     external_task_id));
}

void ActuationTransportHandler::SendStartTaskResponse(
    const std::string& request_id,
    const std::string& external_task_id,
    proto::StartTaskResponse response) {
  proto::BrowserToServerPayload upstream;
  upstream.set_task_id(external_task_id);
  upstream.set_request_id(request_id);
  *upstream.mutable_start_task() = std::move(response);
  std::ignore =
      SendUpstreamMessage(stream_transport::PayloadType::kActuation, upstream);
}

void ActuationTransportHandler::HandleStopTask(
    const std::string& request_id,
    const std::string& task_id_str,
    const proto::StopTaskRequest& request) {
  auto pending_it = pending_start_tasks_.find(task_id_str);
  if (pending_it != pending_start_tasks_.end()) {
    if (pending_it->second) {
      // A StopTask is already queued for this task.
      OnStopTaskComplete(request_id, task_id_str, false);
      return;
    }
    // The StartTask is still pending. Queue the stop and reply once
    // OnStartTaskComplete() stops the task.
    pending_it->second = PendingStop{request_id, request};
    return;
  }

  auto it = active_tasks_.find(task_id_str);
  if (it == active_tasks_.end()) {
    OnStopTaskComplete(request_id, task_id_str, false);
    return;
  }
  TaskId task_id = it->second;
  active_tasks_.erase(it);
  delegate_->StopTask(
      task_id, request,
      base::BindOnce(&ActuationTransportHandler::OnStopTaskComplete,
                     weak_ptr_factory_.GetWeakPtr(), request_id, task_id_str));
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
  std::ignore =
      SendUpstreamMessage(stream_transport::PayloadType::kActuation, upstream);
}

void ActuationTransportHandler::HandlePerformActions(
    const std::string& request_id,
    const std::string& task_id_str,
    const proto::PerformActionsRequest& request) {
  if (pending_start_tasks_.contains(task_id_str)) {
    // The task exists but its StartTask has not completed yet.
    proto::PerformActionsResponse response;
    response.set_status(proto::PerformActionsResponse::STATUS_ACTION_FAILED);
    response.set_action_status_code(
        static_cast<int32_t>(mojom::ActionResultCode::kInvalidTaskStateForAct));
    OnActComplete(request_id, task_id_str, std::move(response));
    return;
  }

  auto it = active_tasks_.find(task_id_str);
  if (it == active_tasks_.end()) {
    proto::PerformActionsResponse response;
    response.set_status(proto::PerformActionsResponse::STATUS_TAB_DISCONNECTED);
    response.set_action_status_code(
        static_cast<int32_t>(mojom::ActionResultCode::kTaskWentAway));
    OnActComplete(request_id, task_id_str, std::move(response));
    return;
  }
  delegate_->Act(
      it->second, request,
      base::BindOnce(&ActuationTransportHandler::OnActComplete,
                     weak_ptr_factory_.GetWeakPtr(), request_id, task_id_str));
}

void ActuationTransportHandler::OnActComplete(
    const std::string& request_id,
    const std::string& task_id_str,
    proto::PerformActionsResponse response) {
  if (response.status() != proto::PerformActionsResponse::STATUS_OK &&
      IsTaskStoppedResultCode(response.action_status_code())) {
    active_tasks_.erase(task_id_str);
  }
  proto::BrowserToServerPayload upstream;
  upstream.set_task_id(task_id_str);
  upstream.set_request_id(request_id);
  *upstream.mutable_perform_actions() = std::move(response);
  std::ignore =
      SendUpstreamMessage(stream_transport::PayloadType::kActuation, upstream);
}

}  // namespace actor
