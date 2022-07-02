// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_RUNTIME_BROWSER_RUNTIME_APPLICATION_DISPATCHER_H_
#define CHROMECAST_CAST_CORE_RUNTIME_BROWSER_RUNTIME_APPLICATION_DISPATCHER_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromecast/cast_core/grpc/grpc_server.h"
#include "chromecast/cast_core/runtime/browser/cast_runtime_action_recorder.h"
#include "chromecast/cast_core/runtime/browser/cast_runtime_metrics_recorder.h"
#include "chromecast/cast_core/runtime/browser/cast_runtime_metrics_recorder_service.h"
#include "components/cast_streaming/browser/public/network_context_getter.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/cast_core/public/src/proto/metrics/metrics_recorder.castcore.pb.h"
#include "third_party/cast_core/public/src/proto/runtime/runtime_service.castcore.pb.h"

namespace chromecast {

namespace media {
class VideoPlaneController;
}  // namespace media

class CastWebService;
class RuntimeApplication;

class RuntimeApplicationDispatcher final {
 public:
  // Observer interface for dispatcher notifications.
  class Observer : public base::CheckedObserver {
   public:
    // Called with a valid pointer when application is brought to the
    // foreground. Otherwise a nullptr is passed.
    virtual void OnForegroundApplicationChanged(RuntimeApplication* app) = 0;
  };

  RuntimeApplicationDispatcher(
      CastWebService* web_service,
      CastRuntimeMetricsRecorder::EventBuilderFactory* event_builder_factory,
      cast_streaming::NetworkContextGetter network_context_getter,
      media::VideoPlaneController* video_plane_controller);
  ~RuntimeApplicationDispatcher();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Starts and stops the runtime service, including the gRPC completion queue.
  bool Start(const std::string& runtime_id,
             const std::string& runtime_service_endpoint);
  void Stop();

  const std::string& GetCastMediaServiceEndpoint() const;

 private:
  // RuntimeService gRPC handlers:
  void HandleLoadApplication(
      cast::runtime::LoadApplicationRequest request,
      cast::runtime::RuntimeServiceHandler::LoadApplication::Reactor* reactor);
  void HandleLaunchApplication(
      cast::runtime::LaunchApplicationRequest request,
      cast::runtime::RuntimeServiceHandler::LaunchApplication::Reactor*
          reactor);
  void HandleStopApplication(
      cast::runtime::StopApplicationRequest request,
      cast::runtime::RuntimeServiceHandler::StopApplication::Reactor* reactor);
  void HandleHeartbeat(
      cast::runtime::HeartbeatRequest request,
      cast::runtime::RuntimeServiceHandler::Heartbeat::Reactor* reactor);
  void HandleStartMetricsRecorder(
      cast::runtime::StartMetricsRecorderRequest request,
      cast::runtime::RuntimeServiceHandler::StartMetricsRecorder::Reactor*
          reactor);
  void HandleStopMetricsRecorder(
      cast::runtime::StopMetricsRecorderRequest request,
      cast::runtime::RuntimeServiceHandler::StopMetricsRecorder::Reactor*
          reactor);

  // Helper methods.
  void OnApplicationLoaded(
      std::string session_id,
      cast::runtime::RuntimeServiceHandler::LoadApplication::Reactor* reactor,
      grpc::Status status);
  void OnApplicationLaunched(
      std::string session_id,
      cast::runtime::RuntimeServiceHandler::LaunchApplication::Reactor* reactor,
      grpc::Status status);
  void SendHeartbeat();
  void OnHeartbeatSent(
      cast::utils::GrpcStatusOr<
          cast::runtime::RuntimeServiceHandler::Heartbeat::Reactor*>
          reactor_or);
  void RecordMetrics(cast::metrics::RecordRequest request,
                     CastRuntimeMetricsRecorderService::RecordCompleteCallback
                         record_complete_callback);
  void OnMetricsRecorded(
      CastRuntimeMetricsRecorderService::RecordCompleteCallback
          record_complete_callback,
      cast::utils::GrpcStatusOr<cast::metrics::RecordResponse> response_or);
  void OnMetricsRecorderServiceStopped(
      cast::runtime::RuntimeServiceHandler::StopMetricsRecorder::Reactor*
          reactor);
  // Returns an app for the |session_id| or nullptr if not found.
  RuntimeApplication* GetApp(const std::string& session_id) const;
  // Destroys the app for the |session_id|.
  void ResetApp(const std::string& session_id);

  SEQUENCE_CHECKER(sequence_checker_);
  CastWebService* const web_service_;
  cast_streaming::NetworkContextGetter network_context_getter_;
  CastRuntimeMetricsRecorder metrics_recorder_;
  media::VideoPlaneController* const video_plane_controller_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::ObserverList<Observer> observers_;

  base::flat_map<std::string, std::unique_ptr<RuntimeApplication>> loaded_apps_;

  // Allows histogram and action recording, which can be reported by
  // CastRuntimeMetricsRecorderService if Cast Core starts it.
  CastRuntimeActionRecorder action_recorder_;

  absl::optional<cast::utils::GrpcServer> grpc_server_;
  absl::optional<cast::metrics::MetricsRecorderServiceStub>
      metrics_recorder_stub_;
  absl::optional<CastRuntimeMetricsRecorderService> metrics_recorder_service_;
  // Heartbeat period as set by Cast Core.
  base::TimeDelta heartbeat_period_;
  // Heartbeat timeout timer.
  base::OneShotTimer heartbeat_timer_;
  // Server streaming reactor used to send the heartbeats to Cast Core.
  cast::runtime::RuntimeServiceHandler::Heartbeat::Reactor* heartbeat_reactor_ =
      nullptr;

  base::WeakPtrFactory<RuntimeApplicationDispatcher> weak_factory_{this};
};

}  // namespace chromecast

#endif  // CHROMECAST_CAST_CORE_RUNTIME_BROWSER_RUNTIME_APPLICATION_DISPATCHER_H_
