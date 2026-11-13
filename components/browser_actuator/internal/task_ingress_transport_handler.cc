// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/internal/task_ingress_transport_handler.h"

#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/logging.h"
#include "base/sequence_checker.h"
#include "components/browser_actuator/internal/proto/task_ingress.pb.h"
#include "components/stream_transport/public/common.h"

namespace browser_actuator {

TaskIngressEventDelegate::~TaskIngressEventDelegate() = default;

TaskIngressTransportHandler::TaskIngressTransportHandler(
    stream_transport::TransportSession* session,
    base::WeakPtr<TaskIngressEventDelegate> delegate)
    : stream_transport::TransportHandler(session),
      delegate_(std::move(delegate)) {
  CHECK(session);
}

TaskIngressTransportHandler::~TaskIngressTransportHandler() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void TaskIngressTransportHandler::OnMessage(
    stream_transport::PayloadType payload_type,
    std::string_view serialized_payload) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (payload_type != stream_transport::PayloadType::kTaskIngress) {
    DLOG(WARNING) << "Unexpected payload type: "
                  << static_cast<int>(payload_type);
    return;
  }

  TaskIngressDownstreamPayload payload;
  if (!payload.ParseFromString(serialized_payload)) {
    DLOG(WARNING) << "Failed to parse TaskIngressDownstreamPayload";
    return;
  }

  if (!delegate_) {
    return;
  }

  // Delegate callbacks may synchronously close the session and destroy this
  // handler, so member variables must remain untouched after dispatching.
  switch (payload.event_case()) {
    case TaskIngressDownstreamPayload::kTaskStarted:
      delegate_->OnTaskStartedEvent();
      break;
    case TaskIngressDownstreamPayload::kWorklogUpdate:
      delegate_->OnWorklogUpdateEvent(
          payload.worklog_update().step_description());
      break;
    case TaskIngressDownstreamPayload::kTaskCompleted:
      delegate_->OnTaskCompletedEvent(payload.task_completed().status(),
                                      payload.task_completed().error_message());
      break;
    case TaskIngressDownstreamPayload::EVENT_NOT_SET:
      DLOG(WARNING) << "TaskIngressDownstreamPayload event not set";
      break;
  }
}

}  // namespace browser_actuator
