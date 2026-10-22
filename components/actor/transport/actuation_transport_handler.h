// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACTOR_TRANSPORT_ACTUATION_TRANSPORT_HANDLER_H_
#define COMPONENTS_ACTOR_TRANSPORT_ACTUATION_TRANSPORT_HANDLER_H_

#include <memory>
#include <string>
#include <string_view>

#include "base/containers/flat_set.h"
#include "base/memory/weak_ptr.h"
#include "components/actor/core/task_id.h"
#include "components/actor/transport/actuation_delegate.h"
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
  void HandleStartTask(const std::string& request_id,
                       const proto::StartTaskRequest& request);
  void OnStartTaskComplete(const std::string& request_id,
                           TaskId task_id,
                           proto::StartTaskResponse response);

  void HandleStopTask(const std::string& request_id,
                      std::string_view task_id_str,
                      const proto::StopTaskRequest& request);
  void OnStopTaskComplete(const std::string& request_id,
                          const std::string& task_id_str,
                          bool success);

  void HandlePerformActions(const std::string& request_id,
                            std::string_view task_id_str,
                            const proto::PerformActionsRequest& request);
  void OnActComplete(const std::string& request_id,
                     const std::string& task_id_str,
                     proto::PerformActionsResponse response);

  std::unique_ptr<ActuationDelegate> delegate_;
  base::flat_set<TaskId> active_tasks_;
  base::WeakPtrFactory<ActuationTransportHandler> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // COMPONENTS_ACTOR_TRANSPORT_ACTUATION_TRANSPORT_HANDLER_H_
