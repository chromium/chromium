// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACTOR_TRANSPORT_ACTUATION_TRANSPORT_HANDLER_H_
#define COMPONENTS_ACTOR_TRANSPORT_ACTUATION_TRANSPORT_HANDLER_H_

#include <memory>
#include <string>
#include <string_view>

#include "components/actor/core/task_id.h"
#include "components/actor/transport/actuation_delegate.h"
#include "components/browser_actuator/public/transport_handler.h"

namespace browser_actuator {
class TransportSession;
}  // namespace browser_actuator

namespace actor {

namespace proto {
class StopTask;
}  // namespace proto

// Routes actuation commands from a transport session to an ActuationDelegate
// and sends the results upstream.
class ActuationTransportHandler : public browser_actuator::TransportHandler {
 public:
  ActuationTransportHandler(browser_actuator::TransportSession* session,
                            std::unique_ptr<ActuationDelegate> delegate);
  ActuationTransportHandler(const ActuationTransportHandler&) = delete;
  ActuationTransportHandler& operator=(const ActuationTransportHandler&) =
      delete;
  ~ActuationTransportHandler() override;

  // browser_actuator::TransportHandler:
  void OnMessage(browser_actuator::PayloadType payload_type,
                 std::string_view serialized_payload) override;

 private:
  void HandleStartTask(
      const std::string& request_id,
      const optimization_guide::proto::BrowserStartTask& request);
  void OnStartTaskComplete(
      const std::string& request_id,
      optimization_guide::proto::BrowserStartTaskResult result);

  void HandleStopTask(const std::string& request_id,
                      const proto::StopTask& request);
  void OnStopTaskComplete(const std::string& request_id,
                          TaskId task_id,
                          bool success);

  void HandleAct(const std::string& request_id,
                 const optimization_guide::proto::Actions& actions);
  void OnActComplete(const std::string& request_id,
                     optimization_guide::proto::ActionsResult result);
};

}  // namespace actor

#endif  // COMPONENTS_ACTOR_TRANSPORT_ACTUATION_TRANSPORT_HANDLER_H_
