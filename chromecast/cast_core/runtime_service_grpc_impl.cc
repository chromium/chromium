// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/runtime_service_grpc_impl.h"

#include "base/logging.h"
#include "chromecast/cast_core/simple_async_grpc.h"

namespace chromecast {
namespace {

class LoadApplication final
    : public SimpleAsyncGrpc<LoadApplication,
                             cast::runtime::LoadApplicationRequest,
                             cast::runtime::LoadApplicationResponse> {
 public:
  LoadApplication(cast::runtime::RuntimeService::AsyncService* service,
                  RuntimeServiceDelegate* delegate,
                  grpc::ServerCompletionQueue* cq)
      : SimpleAsyncGrpc(cq), delegate_(delegate), service_(service) {
    service_->RequestLoadApplication(&ctx_, &request_, &responder_, cq_, cq_,
                                     static_cast<GRPC*>(this));
  }

  cast::runtime::RuntimeService::AsyncService* service() { return service_; }
  RuntimeServiceDelegate* delegate() { return delegate_; }

  void DoMethod() { delegate_->LoadApplication(request_, &response_, this); }

 private:
  RuntimeServiceDelegate* delegate_;
  cast::runtime::RuntimeService::AsyncService* service_;
};

class LaunchApplication final
    : public SimpleAsyncGrpc<LaunchApplication,
                             cast::runtime::LaunchApplicationRequest,
                             cast::runtime::LaunchApplicationResponse> {
 public:
  LaunchApplication(cast::runtime::RuntimeService::AsyncService* service,
                    RuntimeServiceDelegate* delegate,
                    grpc::ServerCompletionQueue* cq)
      : SimpleAsyncGrpc(cq), delegate_(delegate), service_(service) {
    service_->RequestLaunchApplication(&ctx_, &request_, &responder_, cq_, cq_,
                                       static_cast<GRPC*>(this));
  }

  cast::runtime::RuntimeService::AsyncService* service() { return service_; }
  RuntimeServiceDelegate* delegate() { return delegate_; }

  void DoMethod() { delegate_->LaunchApplication(request_, &response_, this); }

 private:
  RuntimeServiceDelegate* delegate_;
  cast::runtime::RuntimeService::AsyncService* service_;
};

class StopApplication final
    : public SimpleAsyncGrpc<StopApplication,
                             cast::runtime::StopApplicationRequest,
                             cast::runtime::StopApplicationResponse> {
 public:
  StopApplication(cast::runtime::RuntimeService::AsyncService* service,
                  RuntimeServiceDelegate* delegate,
                  grpc::ServerCompletionQueue* cq)
      : SimpleAsyncGrpc(cq), delegate_(delegate), service_(service) {
    service_->RequestStopApplication(&ctx_, &request_, &responder_, cq_, cq_,
                                     static_cast<GRPC*>(this));
  }

  cast::runtime::RuntimeService::AsyncService* service() { return service_; }
  RuntimeServiceDelegate* delegate() { return delegate_; }

  void DoMethod() { delegate_->StopApplication(request_, &response_, this); }

 private:
  RuntimeServiceDelegate* delegate_;
  cast::runtime::RuntimeService::AsyncService* service_;
};

class StartMetricsRecorder final
    : public SimpleAsyncGrpc<StartMetricsRecorder,
                             cast::runtime::StartMetricsRecorderRequest,
                             cast::runtime::StartMetricsRecorderResponse> {
 public:
  StartMetricsRecorder(cast::runtime::RuntimeService::AsyncService* service,
                       RuntimeServiceDelegate* delegate,
                       grpc::ServerCompletionQueue* cq)
      : SimpleAsyncGrpc(cq), delegate_(delegate), service_(service) {
    service_->RequestStartMetricsRecorder(&ctx_, &request_, &responder_, cq_,
                                          cq_, static_cast<GRPC*>(this));
  }

  cast::runtime::RuntimeService::AsyncService* service() { return service_; }
  RuntimeServiceDelegate* delegate() { return delegate_; }

  void DoMethod() {
    delegate_->StartMetricsRecorder(request_, &response_, this);
  }

 private:
  RuntimeServiceDelegate* delegate_;
  cast::runtime::RuntimeService::AsyncService* service_;
};

class StopMetricsRecorder final
    : public SimpleAsyncGrpc<StopMetricsRecorder,
                             cast::runtime::StopMetricsRecorderRequest,
                             cast::runtime::StopMetricsRecorderResponse> {
 public:
  StopMetricsRecorder(cast::runtime::RuntimeService::AsyncService* service,
                      RuntimeServiceDelegate* delegate,
                      grpc::ServerCompletionQueue* cq)
      : SimpleAsyncGrpc(cq), delegate_(delegate), service_(service) {
    service_->RequestStopMetricsRecorder(&ctx_, &request_, &responder_, cq_,
                                         cq_, static_cast<GRPC*>(this));
  }

  cast::runtime::RuntimeService::AsyncService* service() { return service_; }
  RuntimeServiceDelegate* delegate() { return delegate_; }

  void DoMethod() {
    delegate_->StopMetricsRecorder(request_, &response_, this);
  }

 private:
  RuntimeServiceDelegate* delegate_;
  cast::runtime::RuntimeService::AsyncService* service_;
};

}  // namespace

RuntimeServiceDelegate::~RuntimeServiceDelegate() = default;

HeartbeatMethod::HeartbeatMethod(
    cast::runtime::RuntimeService::AsyncService* service,
    RuntimeServiceDelegate* delegate,
    grpc::ServerCompletionQueue* cq)
    : GrpcMethod(cq),
      service_(service),
      delegate_(delegate),
      responder_(&ctx_) {
  service_->RequestHeartbeat(&ctx_, &request_, &responder_, cq_, cq_,
                             static_cast<GRPC*>(this));
}

HeartbeatMethod::~HeartbeatMethod() = default;

void HeartbeatMethod::Tick() {
  if (state_ == kWriteReady) {
    cast::runtime::HeartbeatResponse response;
    responder_.Write(response, this);
    state_ = kWritePending;
  }
}

void HeartbeatMethod::Finish(grpc::Status status) {
  responder_.Finish(status, static_cast<GRPC*>(this));
  state_ = kFinish;
  Done();
}

GrpcMethod* HeartbeatMethod::Clone() {
  return new HeartbeatMethod(service_, delegate_, cq_);
}

void HeartbeatMethod::StepInternal(grpc::Status status) {
  switch (state_) {
    case kStart:
      DCHECK(status.ok());
      state_ = kWriteReady;
      delegate_->Heartbeat(request_, this);
      break;
    case kWritePending:
      state_ = kWriteReady;
      break;
    default:
      NOTREACHED();
      break;
  }
}

void StartRuntimeServiceMethods(
    cast::runtime::RuntimeService::AsyncService* service,
    RuntimeServiceDelegate* delegate,
    ::grpc::ServerCompletionQueue* cq) {
  new LoadApplication(service, delegate, cq);
  new LaunchApplication(service, delegate, cq);
  new StopApplication(service, delegate, cq);
  new HeartbeatMethod(service, delegate, cq);
  new StartMetricsRecorder(service, delegate, cq);
  new StopMetricsRecorder(service, delegate, cq);
}

}  // namespace chromecast
