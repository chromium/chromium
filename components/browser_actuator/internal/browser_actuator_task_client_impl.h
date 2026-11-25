// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_ACTUATOR_INTERNAL_BROWSER_ACTUATOR_TASK_CLIENT_IMPL_H_
#define COMPONENTS_BROWSER_ACTUATOR_INTERNAL_BROWSER_ACTUATOR_TASK_CLIENT_IMPL_H_

#include <string>
#include <string_view>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/types/expected.h"
#include "components/browser_actuator/internal/proto/task_ingress.pb.h"
#include "components/browser_actuator/internal/task_ingress_transport_handler.h"
#include "components/browser_actuator/public/browser_actuator_task_client.h"
#include "components/stream_transport/public/common.h"

namespace google::protobuf {
class MessageLite;
}  // namespace google::protobuf

namespace stream_transport {
class TransportService;
class TransportSession;
}  // namespace stream_transport

namespace browser_actuator {

class TaskIngressTransportHandlerFactory;

// Implementation of `BrowserActuatorTaskClient` managing task lifecycles on a
// single UI sequence.
class BrowserActuatorTaskClientImpl : public BrowserActuatorTaskClient,
                                      public TaskIngressEventDelegate {
 public:
  using DeviceIdProvider = base::RepeatingCallback<std::string()>;

  // Factory used to instantiate and register transport handlers for new
  // client sessions. `base::WeakPtr` is required because the factory's lifetime
  // is tied to the profile's task client factory, which may be destroyed
  // independently of this client.
  using HandlerFactory = base::WeakPtr<TaskIngressTransportHandlerFactory>;

  enum class State { kIdle, kStarting, kAccepted, kRunning, kDone };

  // `transport_service` and `device_id_provider` must be non-null.
  // `transport_service` must outlive active use until destruction or
  // `DetachForShutdown()`.
  BrowserActuatorTaskClientImpl(
      stream_transport::TransportService* transport_service,
      HandlerFactory handler_factory,
      DeviceIdProvider device_id_provider);
  ~BrowserActuatorTaskClientImpl() override;

  BrowserActuatorTaskClientImpl(const BrowserActuatorTaskClientImpl&) = delete;
  BrowserActuatorTaskClientImpl& operator=(
      const BrowserActuatorTaskClientImpl&) = delete;

  // BrowserActuatorTaskClient:
  void StartTask(UseCase use_case,
                 const google::protobuf::MessageLite& payload,
                 BrowserActuatorTaskClient::Observer* observer) override;
  void Cancel() override;

  // TaskIngressEventDelegate:
  void OnTaskStartedEvent() override;
  void OnWorklogUpdateEvent(std::string_view step_description) override;
  void OnTaskCompletedEvent(TaskCompletionStatus status,
                            std::string_view error_message) override;
  void DetachForShutdown() override;

  State state() const;
  const std::string& session_id() const;

 private:
  void SendStartRequest(UseCase use_case,
                        std::string type_url,
                        std::string serialized_payload,
                        std::string client_id);
  void OnSessionStarted(
      base::expected<stream_transport::TransportSession*,
                     stream_transport::StartClientSessionError> res);
  void UnregisterSession();
  void ClosePendingSession();
  void CloseActiveSession();
  void NotifyTaskCompleted(Result result);
  void DispatchTaskCompleted(BrowserActuatorTaskClient::Observer* observer,
                             Result result);

  SEQUENCE_CHECKER(sequence_checker_);

  const HandlerFactory handler_factory_;
  const DeviceIdProvider device_id_provider_;
  raw_ptr<stream_transport::TransportService> transport_service_
      GUARDED_BY_CONTEXT(sequence_checker_);

  State state_ GUARDED_BY_CONTEXT(sequence_checker_) = State::kIdle;
  bool start_task_called_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  std::string session_id_ GUARDED_BY_CONTEXT(sequence_checker_);
  raw_ptr<BrowserActuatorTaskClient::Observer> observer_
      GUARDED_BY_CONTEXT(sequence_checker_) = nullptr;
  bool has_active_session_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  raw_ptr<stream_transport::TransportSession> pending_session_
      GUARDED_BY_CONTEXT(sequence_checker_) = nullptr;

  base::WeakPtrFactory<BrowserActuatorTaskClientImpl> weak_ptr_factory_{this};
};

}  // namespace browser_actuator

#endif  // COMPONENTS_BROWSER_ACTUATOR_INTERNAL_BROWSER_ACTUATOR_TASK_CLIENT_IMPL_H_
