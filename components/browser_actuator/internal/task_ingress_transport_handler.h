// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_ACTUATOR_INTERNAL_TASK_INGRESS_TRANSPORT_HANDLER_H_
#define COMPONENTS_BROWSER_ACTUATOR_INTERNAL_TASK_INGRESS_TRANSPORT_HANDLER_H_

#include <string_view>

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "components/browser_actuator/internal/proto/task_ingress.pb.h"
#include "components/stream_transport/public/common.h"
#include "components/stream_transport/public/transport_handler.h"

namespace stream_transport {
class TransportSession;
}  // namespace stream_transport

namespace browser_actuator {

// Delegate interface receiving parsed task ingress events from the handler.
class TaskIngressEventDelegate {
 public:
  virtual ~TaskIngressEventDelegate();
  virtual void OnTaskStartedEvent() = 0;
  virtual void OnWorklogUpdateEvent(std::string_view step_description) = 0;
  virtual void OnTaskCompletedEvent(TaskCompletionStatus status,
                                    std::string_view error_message) = 0;
  virtual void DetachForShutdown() = 0;
};

// `TransportHandler` for task ingress payloads. Dispatches downstream events
// to an associated `TaskIngressEventDelegate`.
class TaskIngressTransportHandler : public stream_transport::TransportHandler {
 public:
  // `session` must be non-null and is retained by the base class for the
  // lifetime of this handler.
  TaskIngressTransportHandler(stream_transport::TransportSession* session,
                              base::WeakPtr<TaskIngressEventDelegate> delegate);
  ~TaskIngressTransportHandler() override;

  TaskIngressTransportHandler(const TaskIngressTransportHandler&) = delete;
  TaskIngressTransportHandler& operator=(const TaskIngressTransportHandler&) =
      delete;

  // stream_transport::TransportHandler:
  void OnMessage(stream_transport::PayloadType payload_type,
                 std::string_view serialized_payload) override;

 private:
  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtr<TaskIngressEventDelegate> delegate_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace browser_actuator

#endif  // COMPONENTS_BROWSER_ACTUATOR_INTERNAL_TASK_INGRESS_TRANSPORT_HANDLER_H_
