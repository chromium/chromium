// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_RUNTIME_SERVICE_H_
#define CHROMECAST_CAST_CORE_RUNTIME_SERVICE_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "chromecast/cast_core/cast_runtime_action_recorder.h"
#include "chromecast/cast_core/cast_runtime_metrics_recorder.h"
#include "chromecast/cast_core/cast_runtime_metrics_recorder_service.h"
#include "chromecast/cast_core/grpc_method.h"
#include "chromecast/cast_core/grpc_server.h"
#include "chromecast/cast_core/metrics_recorder_grpc.h"
#include "chromecast/cast_core/runtime_service_grpc_impl.h"
#include "third_party/grpc/src/include/grpcpp/server.h"
#include "third_party/openscreen/src/cast/cast_core/api/metrics/metrics_recorder.grpc.pb.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace chromecast {

class CastWebService;
class CastWebViewFactory;
class CastWindowManager;
class RuntimeApplicationService;

class RuntimeService final : public GrpcServer,
                             public RuntimeServiceDelegate,
                             public MetricsRecorderGrpc {
 public:
  RuntimeService(
      content::BrowserContext* browser_context,
      CastWindowManager* window_manager,
      CastRuntimeMetricsRecorder::EventBuilderFactory* event_builder_factory);
  ~RuntimeService() override;

  // Starts and stops the runtime service, including the gRPC completion queue.
  bool Start(const std::string& runtime_id,
             const std::string& runtime_service_path);
  void Stop();

  // RuntimeServiceDelegate implementation:
  void LoadApplication(const cast::runtime::LoadApplicationRequest& request,
                       cast::runtime::LoadApplicationResponse* response,
                       GrpcMethod* callback) override;
  void LaunchApplication(const cast::runtime::LaunchApplicationRequest& request,
                         cast::runtime::LaunchApplicationResponse* response,
                         GrpcMethod* callback) override;
  void StopApplication(const cast::runtime::StopApplicationRequest& request,
                       cast::runtime::StopApplicationResponse* response,
                       GrpcMethod* callback) override;
  void Heartbeat(const cast::runtime::HeartbeatRequest& request,
                 HeartbeatMethod* heartbeat) override;
  void StartMetricsRecorder(
      const cast::runtime::StartMetricsRecorderRequest& request,
      cast::runtime::StartMetricsRecorderResponse* response,
      GrpcMethod* callback) override;
  void StopMetricsRecorder(
      const cast::runtime::StopMetricsRecorderRequest& request,
      cast::runtime::StopMetricsRecorderResponse* response,
      GrpcMethod* callback) override;

  const std::string& GetCastMediaServiceGrpcEndpoint() const;

 private:
  // This class handles asynchronously calling MetricsRecorderService->Record
  // and notifying RuntimeService when it completes.  There should only be one
  // of these at a time.
  class AsyncMetricsRecord final : public GrpcCall {
   public:
    AsyncMetricsRecord(
        const cast::metrics::RecordRequest& request,
        cast::metrics::MetricsRecorderService::Stub* metrics_recorder_stub,
        grpc::CompletionQueue* cq,
        base::WeakPtr<RuntimeService> runtime_service);
    ~AsyncMetricsRecord() override;

    // GrpcCall implementation:
    void StepGRPC(grpc::Status status) override;

   private:
    base::WeakPtr<RuntimeService> runtime_service_;

    cast::metrics::RecordResponse response_;
    std::unique_ptr<
        grpc::ClientAsyncResponseReader<cast::metrics::RecordResponse>>
        response_reader_;
  };

  void SendHeartbeat();
  void OnRecordComplete();
  void OnMetricsRecorderServiceStopped(GrpcMethod* callback);

  // MetricsRecorderGrpc implementation:
  void Record(const cast::metrics::RecordRequest& request) override;

  const std::unique_ptr<CastWebViewFactory> web_view_factory_;
  const std::unique_ptr<CastWebService> web_service_;

  std::unique_ptr<RuntimeApplicationService> app_service_;

  // These variables support the runtime heartbeat stream.  |heartbeat_| will
  // live until Finish() is called, but it is a GrpcMethod so it is unowned.
  HeartbeatMethod* heartbeat_{nullptr};
  int64_t heartbeat_period_{0};

  // Allows histogram and action recording, which can be reported by
  // CastRuntimeMetricsRecorderService if Cast Core starts it.
  CastRuntimeMetricsRecorder metrics_recorder_;
  CastRuntimeActionRecorder action_recorder_;

  std::string runtime_id_;
  std::string runtime_service_path_;
  std::unique_ptr<cast::metrics::MetricsRecorderService::Stub>
      metrics_recorder_stub_;
  cast::runtime::RuntimeService::AsyncService grpc_runtime_service_;
  std::unique_ptr<CastRuntimeMetricsRecorderService> metrics_recorder_service_;

  base::WeakPtrFactory<RuntimeService> weak_factory_{this};
};

}  // namespace chromecast

#endif  // CHROMECAST_CAST_CORE_RUNTIME_SERVICE_H_
