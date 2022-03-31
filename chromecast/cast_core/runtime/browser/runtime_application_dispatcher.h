// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_RUNTIME_BROWSER_RUNTIME_APPLICATION_DISPATCHER_H_
#define CHROMECAST_CAST_CORE_RUNTIME_BROWSER_RUNTIME_APPLICATION_DISPATCHER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
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
class RuntimeApplicationWatcher;

class RuntimeApplicationDispatcher {
 public:
  RuntimeApplicationDispatcher(
      CastWebService* web_service,
      CastRuntimeMetricsRecorder::EventBuilderFactory* event_builder_factory,
      cast_streaming::NetworkContextGetter network_context_getter,
      media::VideoPlaneController* video_plane_controller,
      RuntimeApplicationWatcher* application_watcher);
  ~RuntimeApplicationDispatcher();

  // Starts and stops the runtime service, including the gRPC completion queue.
  bool Start(const std::string& runtime_id,
             const std::string& runtime_service_endpoint);
  void Stop();

  const std::string& GetCastMediaServiceEndpoint() const;
  CastWebService* GetCastWebService() const;
  RuntimeApplication* GetRuntimeApplication();

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
      cast::runtime::RuntimeServiceHandler::LoadApplication::Reactor* reactor,
      grpc::Status status);
  void OnApplicationLaunched(
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
  void ResetApp();

  CastWebService* const web_service_;
  cast_streaming::NetworkContextGetter network_context_getter_;
  std::unique_ptr<RuntimeApplication> app_;

  // Heartbeat period as set by Cast Core.
  base::TimeDelta heartbeat_period_;
  // Heartbeat timeout timer.
  base::OneShotTimer heartbeat_timer_;
  // Server streaming reactor used to send the heartbeats to Cast Core.
  cast::runtime::RuntimeServiceHandler::Heartbeat::Reactor* heartbeat_reactor_ =
      nullptr;

  // Allows histogram and action recording, which can be reported by
  // CastRuntimeMetricsRecorderService if Cast Core starts it.
  CastRuntimeMetricsRecorder metrics_recorder_;
  CastRuntimeActionRecorder action_recorder_;

  absl::optional<cast::utils::GrpcServer> grpc_server_;
  absl::optional<cast::metrics::MetricsRecorderServiceStub>
      metrics_recorder_stub_;
  absl::optional<CastRuntimeMetricsRecorderService> metrics_recorder_service_;

  media::VideoPlaneController* video_plane_controller_;
  RuntimeApplicationWatcher* application_watcher_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<RuntimeApplicationDispatcher> weak_factory_{this};
};

}  // namespace chromecast

#endif  // CHROMECAST_CAST_CORE_RUNTIME_BROWSER_RUNTIME_APPLICATION_DISPATCHER_H_
