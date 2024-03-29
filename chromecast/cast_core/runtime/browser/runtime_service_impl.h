// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_RUNTIME_BROWSER_RUNTIME_SERVICE_IMPL_H_
#define CHROMECAST_CAST_CORE_RUNTIME_BROWSER_RUNTIME_SERVICE_IMPL_H_

#include <memory>
#include <optional>
#include <string_view>

#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromecast/cast_core/grpc/grpc_server.h"
#include "chromecast/cast_core/runtime/browser/cast_runtime_action_recorder.h"
#include "chromecast/cast_core/runtime/browser/cast_runtime_metrics_recorder.h"
#include "chromecast/cast_core/runtime/browser/cast_runtime_metrics_recorder_service.h"
#include "chromecast/cast_core/runtime/browser/runtime_application_service_impl.h"
#include "components/cast_receiver/browser/public/runtime_application_dispatcher.h"
#include "components/cast_receiver/common/public/status.h"
#include "third_party/cast_core/public/src/proto/metrics/metrics_recorder.castcore.pb.h"
#include "third_party/cast_core/public/src/proto/runtime/runtime_service.castcore.pb.h"

namespace cast_receiver {
class ContentBrowserClientMixins;
}  // namespace cast_receiver

namespace chromecast {

class CastWebService;

// An implementation of the gRPC-defined RuntimeService for use with Cast Core.
class RuntimeServiceImpl final
    : public CastRuntimeMetricsRecorder::EventBuilderFactory {
 public:
  // |application_client| and |web_service| are expected to persist for the
  // lifetime of this instance.
  RuntimeServiceImpl(cast_receiver::ContentBrowserClientMixins& browser_mixins,
                     CastWebService& web_service);
  ~RuntimeServiceImpl() override;

  // Starts and stops the runtime service, including the gRPC completion queue.
  cast_receiver::Status Start();
  cast_receiver::Status Start(std::string_view runtime_id,
                              std::string_view runtime_service_endpoint);
  cast_receiver::Status Stop();

  // CastRuntimeMetricsRecorder::EventBuilderFactory overrides:
  std::unique_ptr<CastEventBuilder> CreateEventBuilder() override;

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
      cast_receiver::Status status);
  void OnApplicationLaunching(
      std::string session_id,
      cast::runtime::RuntimeServiceHandler::LaunchApplication::Reactor* reactor,
      cast_receiver::Status status);
  void OnApplicationStopping(
      std::string session_id,
      cast::runtime::RuntimeServiceHandler::StopApplication::Reactor* reactor,
      cast_receiver::Status status);
  void SendHeartbeat();
  void OnHeartbeatSent(
      grpc::Status status,
      cast::runtime::RuntimeServiceHandler::Heartbeat::Reactor* reactor);
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

  void ResetGrpcServices();

  SEQUENCE_CHECKER(sequence_checker_);

  std::unique_ptr<cast_receiver::RuntimeApplicationDispatcher<
      RuntimeApplicationServiceImpl>>
      application_dispatcher_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  raw_ref<CastWebService> const web_service_;

  // Allows metrics, histogram, action recording, which can be reported by
  // CastRuntimeMetricsRecorderService if Cast Core starts it.
  CastRuntimeActionRecorder action_recorder_;
  CastRuntimeMetricsRecorder metrics_recorder_;

  std::optional<cast::utils::GrpcServer> grpc_server_;
  std::optional<cast::metrics::MetricsRecorderServiceStub>
      metrics_recorder_stub_;
  std::optional<CastRuntimeMetricsRecorderService> metrics_recorder_service_;

  // Heartbeat period as set by Cast Core.
  base::TimeDelta heartbeat_period_;

  // Heartbeat timeout timer.
  base::OneShotTimer heartbeat_timer_;

  // Server streaming reactor used to send the heartbeats to Cast Core.
  cast::runtime::RuntimeServiceHandler::Heartbeat::Reactor* heartbeat_reactor_ =
      nullptr;

  base::WeakPtrFactory<RuntimeServiceImpl> weak_factory_{this};
};

}  // namespace chromecast

#endif  // CHROMECAST_CAST_CORE_RUNTIME_BROWSER_RUNTIME_SERVICE_IMPL_H_
