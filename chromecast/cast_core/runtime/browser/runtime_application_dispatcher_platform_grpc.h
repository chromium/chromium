// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_RUNTIME_BROWSER_RUNTIME_APPLICATION_DISPATCHER_PLATFORM_GRPC_H_
#define CHROMECAST_CAST_CORE_RUNTIME_BROWSER_RUNTIME_APPLICATION_DISPATCHER_PLATFORM_GRPC_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromecast/cast_core/grpc/grpc_server.h"
#include "chromecast/cast_core/runtime/browser/cast_runtime_action_recorder.h"
#include "chromecast/cast_core/runtime/browser/cast_runtime_metrics_recorder.h"
#include "chromecast/cast_core/runtime/browser/cast_runtime_metrics_recorder_service.h"
#include "chromecast/cast_core/runtime/browser/runtime_application_dispatcher_platform.h"
#include "components/cast_receiver/common/public/status.h"
#include "components/cast_streaming/browser/public/network_context_getter.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/cast_core/public/src/proto/metrics/metrics_recorder.castcore.pb.h"
#include "third_party/cast_core/public/src/proto/runtime/runtime_service.castcore.pb.h"

namespace chromecast {

class CastWebService;

// A gRPC-based implementation of RuntimeApplicationDispatcherPlatform for use
// with Cast Core.
class RuntimeApplicationDispatcherPlatformGrpc final
    : public RuntimeApplicationDispatcherPlatform {
 public:
  // |application_client| is expected to persist for the lifetime of this
  // instance.
  RuntimeApplicationDispatcherPlatformGrpc(
      RuntimeApplicationDispatcherPlatform::Client& client,
      CastWebService* web_service,
      CastRuntimeMetricsRecorder::EventBuilderFactory* event_builder_factory,
      std::string runtime_id,
      std::string runtime_service_endpoint);
  ~RuntimeApplicationDispatcherPlatformGrpc() override;

  // RuntimeApplicationDispatcherPlatform implementation.
  bool Start() override;
  void Stop() override;

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
      cast_receiver::Status success);
  void OnApplicationLaunching(
      std::string session_id,
      cast::runtime::RuntimeServiceHandler::LaunchApplication::Reactor* reactor,
      cast_receiver::Status success);
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

  base::raw_ref<RuntimeApplicationDispatcherPlatform::Client> client_;

  const std::string runtime_id_;
  const std::string runtime_service_endpoint_;

  SEQUENCE_CHECKER(sequence_checker_);
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Allows metrics, histogram, action recording, which can be reported by
  // CastRuntimeMetricsRecorderService if Cast Core starts it.
  CastRuntimeMetricsRecorder metrics_recorder_;
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

  base::WeakPtrFactory<RuntimeApplicationDispatcherPlatformGrpc> weak_factory_{
      this};
};

}  // namespace chromecast

#endif  // CHROMECAST_CAST_CORE_RUNTIME_BROWSER_RUNTIME_APPLICATION_DISPATCHER_PLATFORM_GRPC_H_
