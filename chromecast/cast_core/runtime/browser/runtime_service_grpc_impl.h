// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_RUNTIME_BROWSER_RUNTIME_SERVICE_GRPC_IMPL_H_
#define CHROMECAST_CAST_CORE_RUNTIME_BROWSER_RUNTIME_SERVICE_GRPC_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "chromecast/cast_core/runtime/browser/grpc/grpc_method.h"
#include "third_party/cast_core/public/src/proto/runtime/runtime_service.grpc.pb.h"
#include "third_party/grpc/src/include/grpcpp/completion_queue.h"
#include "third_party/grpc/src/include/grpcpp/server_context.h"

namespace chromecast {

class GrpcMethod;
class HeartbeatMethod;

class RuntimeServiceDelegate {
 public:
  virtual ~RuntimeServiceDelegate() = 0;

  virtual void LoadApplication(
      const cast::runtime::LoadApplicationRequest& request,
      cast::runtime::LoadApplicationResponse* response,
      GrpcMethod* callback) = 0;
  virtual void LaunchApplication(
      const cast::runtime::LaunchApplicationRequest& request,
      cast::runtime::LaunchApplicationResponse* response,
      GrpcMethod* callback) = 0;
  virtual void StopApplication(
      const cast::runtime::StopApplicationRequest& request,
      cast::runtime::StopApplicationResponse* response,
      GrpcMethod* callback) = 0;
  virtual void Heartbeat(const cast::runtime::HeartbeatRequest& request,
                         HeartbeatMethod* heartbeat) = 0;
  virtual void StartMetricsRecorder(
      const cast::runtime::StartMetricsRecorderRequest& request,
      cast::runtime::StartMetricsRecorderResponse* response,
      GrpcMethod* callback) = 0;
  virtual void StopMetricsRecorder(
      const cast::runtime::StopMetricsRecorderRequest& request,
      cast::runtime::StopMetricsRecorderResponse* response,
      GrpcMethod* callback) = 0;
};

class HeartbeatMethod final : public GrpcMethod {
 public:
  enum State {
    kStart,
    kWriteReady,
    kWritePending,
    kFinish,
  };

  HeartbeatMethod(cast::runtime::RuntimeService::AsyncService* service,
                  base::WeakPtr<RuntimeServiceDelegate> delegate,
                  grpc::ServerCompletionQueue* cq,
                  bool* is_shutdown);
  ~HeartbeatMethod() override;

  void Tick();
  void Finish(grpc::Status status);

  // GrpcMethod implementation:
  GrpcMethod* Clone() override;
  void StepInternal(grpc::Status status) override;

 private:
  State state_{kStart};
  bool* is_shutdown_;
  cast::runtime::RuntimeService::AsyncService* service_;
  base::WeakPtr<RuntimeServiceDelegate> delegate_;
  cast::runtime::HeartbeatRequest request_;
  grpc::ServerAsyncWriter<cast::runtime::HeartbeatResponse> responder_;
};

void StartRuntimeServiceMethods(
    cast::runtime::RuntimeService::AsyncService* service,
    base::WeakPtr<RuntimeServiceDelegate> delegate,
    ::grpc::ServerCompletionQueue* cq,
    bool* is_shutdown);

}  // namespace chromecast

#endif  // CHROMECAST_CAST_CORE_RUNTIME_BROWSER_RUNTIME_SERVICE_GRPC_IMPL_H_
