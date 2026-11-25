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
#include "base/metrics/field_trial_params.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/types/expected.h"
#include "components/browser_actuator/internal/proto/task_ingress.pb.h"
#include "components/browser_actuator/internal/task_ingress_transport_handler.h"
#include "components/browser_actuator/public/browser_actuator_task_client.h"
#include "components/stream_transport/public/common.h"
#include "components/stream_transport/public/transport_session_registry.h"

namespace google::protobuf {
class MessageLite;
}  // namespace google::protobuf

namespace stream_transport {
class TransportService;
class TransportSession;
}  // namespace stream_transport

namespace browser_actuator {

// Maximum additional start attempts after the initial `StartClientSession()`
// call (`2` retries = up to `3` total attempts).
BASE_DECLARE_FEATURE_PARAM(int, kTaskIngressMaxStartRetries);

// Base delay before retrying a `kTransientError` session start; doubles with
// each consecutive transient retry (`1s`, `2s`, ...). Non-positive values retry
// on the next task turn with zero delay.
BASE_DECLARE_FEATURE_PARAM(base::TimeDelta, kTaskIngressStartRetryDelay);

// Maximum inactivity interval while `kAccepted` or `kRunning` before failing
// the task. Non-positive values disable the watchdog.
BASE_DECLARE_FEATURE_PARAM(base::TimeDelta, kTaskIngressIdleTimeout);

// Grace window after `OnTaskCompletedEvent()` before client-side session close
// so the server's trailing `close_session` command can arrive cleanly.
// Non-positive values close immediately.
BASE_DECLARE_FEATURE_PARAM(base::TimeDelta,
                           kTaskIngressPostCompletionCloseDelay);

class TaskIngressTransportHandlerFactory;

// Implementation of `BrowserActuatorTaskClient` managing task lifecycles on a
// single UI sequence.
class BrowserActuatorTaskClientImpl
    : public BrowserActuatorTaskClient,
      public TaskIngressEventDelegate,
      public stream_transport::TransportSessionRegistry::Observer {
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

  // stream_transport::TransportSessionRegistry::Observer:
  void OnSessionRemoved(std::string_view session_id) override;

  State state() const;
  const std::string& session_id() const;

 private:
  void SendStartRequest();
  void OnSessionStarted(
      base::expected<stream_transport::TransportSession*,
                     stream_transport::StartClientSessionError> res);
  void ClearStartupBuffers();
  void StopAllTimers();
  void UnregisterSession();
  void ClosePendingSession();
  void CloseActiveSession();
  void NotifyTaskCompleted(Result result);
  void DispatchTaskCompleted(BrowserActuatorTaskClient::Observer* observer,
                             Result result);
  void ResetIdleTimer();
  void OnIdleTimeout();
  void OnPostCompletionCloseTimeout();

  SEQUENCE_CHECKER(sequence_checker_);

  const HandlerFactory handler_factory_;
  const DeviceIdProvider device_id_provider_;
  raw_ptr<stream_transport::TransportService> transport_service_
      GUARDED_BY_CONTEXT(sequence_checker_);

  State state_ GUARDED_BY_CONTEXT(sequence_checker_) = State::kIdle;
  bool start_task_called_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  std::string session_id_ GUARDED_BY_CONTEXT(sequence_checker_);
  UseCase use_case_ GUARDED_BY_CONTEXT(sequence_checker_) =
      UseCase::kAutoPasswordChange;
  std::string type_url_ GUARDED_BY_CONTEXT(sequence_checker_);
  std::string payload_ GUARDED_BY_CONTEXT(sequence_checker_);
  std::string client_id_ GUARDED_BY_CONTEXT(sequence_checker_);
  raw_ptr<BrowserActuatorTaskClient::Observer> observer_
      GUARDED_BY_CONTEXT(sequence_checker_) = nullptr;
  bool has_active_session_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  raw_ptr<stream_transport::TransportSession> pending_session_
      GUARDED_BY_CONTEXT(sequence_checker_) = nullptr;

  base::ScopedObservation<stream_transport::TransportSessionRegistry,
                          stream_transport::TransportSessionRegistry::Observer>
      session_registry_observation_ GUARDED_BY_CONTEXT(sequence_checker_){this};
  int start_retries_remaining_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;
  int transient_retry_count_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;
  bool current_session_had_transient_error_
      GUARDED_BY_CONTEXT(sequence_checker_) = false;
  base::OneShotTimer start_retry_timer_ GUARDED_BY_CONTEXT(sequence_checker_);
  base::OneShotTimer idle_timer_ GUARDED_BY_CONTEXT(sequence_checker_);
  base::OneShotTimer post_completion_close_timer_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<BrowserActuatorTaskClientImpl> weak_ptr_factory_{this};
};

}  // namespace browser_actuator

#endif  // COMPONENTS_BROWSER_ACTUATOR_INTERNAL_BROWSER_ACTUATOR_TASK_CLIENT_IMPL_H_
