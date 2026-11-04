// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACTOR_TRANSPORT_ACTUATION_TRANSPORT_HANDLER_H_
#define COMPONENTS_ACTOR_TRANSPORT_ACTUATION_TRANSPORT_HANDLER_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "components/actor/core/task_id.h"
#include "components/actor/transport/actuation_delegate.h"
#include "components/actor/transport/proto/actuation_transport.pb.h"
#include "components/browser_actuator/public/transport/transport_handler.h"

namespace browser_actuator::transport {
class TransportSession;
}  // namespace browser_actuator::transport

namespace actor {

// Routes actuation commands from a transport session to an ActuationDelegate
// and sends the results upstream.
class ActuationTransportHandler
    : public browser_actuator::transport::TransportHandler {
 public:
  ActuationTransportHandler(
      browser_actuator::transport::TransportSession* session,
      std::unique_ptr<ActuationDelegate> delegate);
  ActuationTransportHandler(const ActuationTransportHandler&) = delete;
  ActuationTransportHandler& operator=(const ActuationTransportHandler&) =
      delete;
  ~ActuationTransportHandler() override;

  // browser_actuator::transport::TransportHandler:
  void OnMessage(browser_actuator::transport::PayloadType payload_type,
                 std::string_view serialized_payload) override;

 private:
  // A StopTask that arrived while its StartTask was still pending.
  struct PendingStop {
    std::string request_id;
    proto::StopTaskRequest request;
  };

  void HandleStartTask(const std::string& request_id,
                       const std::string& external_task_id,
                       const proto::StartTaskRequest& request);
  void OnStartTaskComplete(const std::string& request_id,
                           const std::string& external_task_id,
                           TaskId task_id,
                           proto::StartTaskResponse response);
  void SendStartTaskResponse(const std::string& request_id,
                             const std::string& external_task_id,
                             proto::StartTaskResponse response);

  void HandleStopTask(const std::string& request_id,
                      const std::string& task_id_str,
                      const proto::StopTaskRequest& request);
  void OnStopTaskComplete(const std::string& request_id,
                          const std::string& task_id_str,
                          bool success);

  void HandlePerformActions(const std::string& request_id,
                            const std::string& task_id_str,
                            const proto::PerformActionsRequest& request);
  void OnActComplete(const std::string& request_id,
                     const std::string& task_id_str,
                     proto::PerformActionsResponse response);

  std::unique_ptr<ActuationDelegate> delegate_;
  // Maps server-owned task_id strings to Chrome-internal TaskIds for tasks
  // started on this session.
  base::flat_map<std::string, TaskId> active_tasks_;
  // Server-owned task_ids whose StartTask is waiting on the delegate, mapped to
  // the StopTask received while waiting, if any.
  base::flat_map<std::string, std::optional<PendingStop>> pending_start_tasks_;
  base::WeakPtrFactory<ActuationTransportHandler> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // COMPONENTS_ACTOR_TRANSPORT_ACTUATION_TRANSPORT_HANDLER_H_
