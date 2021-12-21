// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_RUNTIME_BROWSER_RUNTIME_APPLICATION_DISPATCHER_H_
#define CHROMECAST_CAST_CORE_RUNTIME_BROWSER_RUNTIME_APPLICATION_DISPATCHER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "chromecast/cast_core/runtime/browser/cast_runtime_action_recorder.h"
#include "chromecast/cast_core/runtime/browser/cast_runtime_metrics_recorder.h"
#include "chromecast/cast_core/runtime/browser/cast_runtime_metrics_recorder_service.h"
#include "chromecast/cast_core/runtime/browser/grpc/grpc_method.h"
#include "chromecast/cast_core/runtime/browser/grpc/grpc_server.h"
#include "chromecast/cast_core/runtime/browser/metrics_recorder_grpc.h"
#include "chromecast/cast_core/runtime/browser/runtime_service_grpc_impl.h"
#include "components/cast_streaming/browser/public/network_context_getter.h"
#include "third_party/cast_core/public/src/proto/metrics/metrics_recorder.grpc.pb.h"
#include "third_party/grpc/src/include/grpcpp/server.h"

namespace chromecast {

class CastWebService;
class RuntimeApplication;

class RuntimeApplicationDispatcher final : public GrpcServer,
                                           public RuntimeServiceDelegate,
                                           public MetricsRecorderGrpc {
 public:
  RuntimeApplicationDispatcher(
      CastWebService* web_service,
      CastRuntimeMetricsRecorder::EventBuilderFactory* event_builder_factory,
      cast_streaming::NetworkContextGetter network_context_getter);
  ~RuntimeApplicationDispatcher() override;

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
  CastWebService* GetCastWebService() const;
  RuntimeApplication* GetRuntimeApplication();

 private:
  // This class handles asynchronously calling MetricsRecorderService->Record
  // and notifying RuntimeApplicationDispatcher when it completes.  There should
  // only be one of these at a time.
  class AsyncMetricsRecord final : public GrpcCall {
   public:
    AsyncMetricsRecord(
        const cast::metrics::RecordRequest& request,
        cast::metrics::MetricsRecorderService::Stub* metrics_recorder_stub,
        grpc::CompletionQueue* cq,
        base::WeakPtr<RuntimeApplicationDispatcher> runtime_service);
    ~AsyncMetricsRecord() override;

    // GrpcCall implementation:
    void StepGRPC(grpc::Status status) override;

   private:
    base::WeakPtr<RuntimeApplicationDispatcher> runtime_service_;

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

  CastWebService* const web_service_;

  cast_streaming::NetworkContextGetter network_context_getter_;

  std::unique_ptr<RuntimeApplication> app_;

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

  base::WeakPtrFactory<RuntimeApplicationDispatcher> weak_factory_{this};
};

}  // namespace chromecast

#endif  // CHROMECAST_CAST_CORE_RUNTIME_BROWSER_RUNTIME_APPLICATION_DISPATCHER_H_
