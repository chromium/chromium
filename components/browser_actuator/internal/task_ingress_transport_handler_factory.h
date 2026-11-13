// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_ACTUATOR_INTERNAL_TASK_INGRESS_TRANSPORT_HANDLER_FACTORY_H_
#define COMPONENTS_BROWSER_ACTUATOR_INTERNAL_TASK_INGRESS_TRANSPORT_HANDLER_FACTORY_H_

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "components/stream_transport/public/common.h"
#include "components/stream_transport/public/transport_handler_factory.h"

namespace browser_actuator {

class TaskIngressEventDelegate;

// Factory creating `TaskIngressTransportHandler` instances for registered
// sessions. Owned directly by `BrowserActuatorServiceImpl`.
//
// All methods must be called on the sequence that created this instance.
class TaskIngressTransportHandlerFactory
    : public stream_transport::TransportHandlerFactory {
 public:
  TaskIngressTransportHandlerFactory();
  ~TaskIngressTransportHandlerFactory() override;

  TaskIngressTransportHandlerFactory(
      const TaskIngressTransportHandlerFactory&) = delete;
  TaskIngressTransportHandlerFactory& operator=(
      const TaskIngressTransportHandlerFactory&) = delete;

  // stream_transport::TransportHandlerFactory:
  stream_transport::FactoryId GetFactoryId() const override;
  std::vector<stream_transport::PayloadType> GetSupportedPayloadTypes()
      const override;
  std::unique_ptr<stream_transport::TransportHandler> OnNewSession(
      stream_transport::TransportSession* session) override;

  // Maps a session ID to a client event delegate for routing incoming sessions
  // in `OnNewSession()`. Session registration does not track delegates for
  // shutdown notifications; use `TrackClient()` to register delegates that
  // require `DetachForShutdown()` cleanup on factory destruction.
  void RegisterSessionClient(std::string_view session_id,
                             base::WeakPtr<TaskIngressEventDelegate> client);
  void UnregisterSessionClient(std::string_view session_id);

  // Tracks a live client delegate to receive `DetachForShutdown()` when
  // `DetachAllClients()` is called. Redundant registrations of the same
  // delegate or expired pointers are safely ignored.
  void TrackClient(base::WeakPtr<TaskIngressEventDelegate> client);
  void DetachAllClients();

  base::WeakPtr<TaskIngressTransportHandlerFactory> GetWeakPtr();

 private:
  SEQUENCE_CHECKER(sequence_checker_);
  base::flat_map<std::string, base::WeakPtr<TaskIngressEventDelegate>>
      session_clients_ GUARDED_BY_CONTEXT(sequence_checker_);
  std::vector<base::WeakPtr<TaskIngressEventDelegate>> tracked_clients_
      GUARDED_BY_CONTEXT(sequence_checker_);
  base::WeakPtrFactory<TaskIngressTransportHandlerFactory> weak_ptr_factory_{
      this};
};

}  // namespace browser_actuator

#endif  // COMPONENTS_BROWSER_ACTUATOR_INTERNAL_TASK_INGRESS_TRANSPORT_HANDLER_FACTORY_H_
