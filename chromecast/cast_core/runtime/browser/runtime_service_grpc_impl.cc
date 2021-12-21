// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/runtime/browser/runtime_service_grpc_impl.h"

#include "base/logging.h"
#include "chromecast/cast_core/runtime/browser/grpc/simple_async_grpc.h"

namespace chromecast {
namespace {

class LoadApplication final
    : public SimpleAsyncGrpc<LoadApplication,
                             cast::runtime::LoadApplicationRequest,
                             cast::runtime::LoadApplicationResponse> {
 public:
  LoadApplication(cast::runtime::RuntimeService::AsyncService* service,
                  base::WeakPtr<RuntimeServiceDelegate> delegate,
                  grpc::ServerCompletionQueue* cq,
                  bool* is_shutdown)
      : SimpleAsyncGrpc(cq),
        is_shutdown_(is_shutdown),
        delegate_(delegate),
        service_(service) {
    DCHECK(!*is_shutdown_);
    service_->RequestLoadApplication(&ctx_, &request_, &responder_, cq_, cq_,
                                     static_cast<GRPC*>(this));
  }

  cast::runtime::RuntimeService::AsyncService* service() { return service_; }
  base::WeakPtr<RuntimeServiceDelegate> delegate() { return delegate_; }
  bool* is_shutdown() { return is_shutdown_; }

  void DoMethod() { delegate_->LoadApplication(request_, &response_, this); }

 private:
  bool* is_shutdown_;
  base::WeakPtr<RuntimeServiceDelegate> delegate_;
  cast::runtime::RuntimeService::AsyncService* service_;
};

class LaunchApplication final
    : public SimpleAsyncGrpc<LaunchApplication,
                             cast::runtime::LaunchApplicationRequest,
                             cast::runtime::LaunchApplicationResponse> {
 public:
  LaunchApplication(cast::runtime::RuntimeService::AsyncService* service,
                    base::WeakPtr<RuntimeServiceDelegate> delegate,
                    grpc::ServerCompletionQueue* cq,
                    bool* is_shutdown)
      : SimpleAsyncGrpc(cq),
        is_shutdown_(is_shutdown),
        delegate_(delegate),
        service_(service) {
    DCHECK(!*is_shutdown_);
    service_->RequestLaunchApplication(&ctx_, &request_, &responder_, cq_, cq_,
                                       static_cast<GRPC*>(this));
  }

  cast::runtime::RuntimeService::AsyncService* service() { return service_; }
  base::WeakPtr<RuntimeServiceDelegate> delegate() { return delegate_; }
  bool* is_shutdown() { return is_shutdown_; }

  void DoMethod() { delegate_->LaunchApplication(request_, &response_, this); }

 private:
  bool* is_shutdown_;
  base::WeakPtr<RuntimeServiceDelegate> delegate_;
  cast::runtime::RuntimeService::AsyncService* service_;
};

class StopApplication final
    : public SimpleAsyncGrpc<StopApplication,
                             cast::runtime::StopApplicationRequest,
                             cast::runtime::StopApplicationResponse> {
 public:
  StopApplication(cast::runtime::RuntimeService::AsyncService* service,
                  base::WeakPtr<RuntimeServiceDelegate> delegate,
                  grpc::ServerCompletionQueue* cq,
                  bool* is_shutdown)
      : SimpleAsyncGrpc(cq),
        is_shutdown_(is_shutdown),
        delegate_(delegate),
        service_(service) {
    DCHECK(!*is_shutdown_);
    service_->RequestStopApplication(&ctx_, &request_, &responder_, cq_, cq_,
                                     static_cast<GRPC*>(this));
  }

  cast::runtime::RuntimeService::AsyncService* service() { return service_; }
  base::WeakPtr<RuntimeServiceDelegate> delegate() { return delegate_; }
  bool* is_shutdown() { return is_shutdown_; }

  void DoMethod() { delegate_->StopApplication(request_, &response_, this); }

 private:
  bool* is_shutdown_;
  base::WeakPtr<RuntimeServiceDelegate> delegate_;
  cast::runtime::RuntimeService::AsyncService* service_;
};

class StartMetricsRecorder final
    : public SimpleAsyncGrpc<StartMetricsRecorder,
                             cast::runtime::StartMetricsRecorderRequest,
                             cast::runtime::StartMetricsRecorderResponse> {
 public:
  StartMetricsRecorder(cast::runtime::RuntimeService::AsyncService* service,
                       base::WeakPtr<RuntimeServiceDelegate> delegate,
                       grpc::ServerCompletionQueue* cq,
                       bool* is_shutdown)
      : SimpleAsyncGrpc(cq),
        is_shutdown_(is_shutdown),
        delegate_(delegate),
        service_(service) {
    DCHECK(!*is_shutdown_);
    service_->RequestStartMetricsRecorder(&ctx_, &request_, &responder_, cq_,
                                          cq_, static_cast<GRPC*>(this));
  }

  cast::runtime::RuntimeService::AsyncService* service() { return service_; }
  base::WeakPtr<RuntimeServiceDelegate> delegate() { return delegate_; }
  bool* is_shutdown() { return is_shutdown_; }

  void DoMethod() {
    delegate_->StartMetricsRecorder(request_, &response_, this);
  }

 private:
  bool* is_shutdown_;
  base::WeakPtr<RuntimeServiceDelegate> delegate_;
  cast::runtime::RuntimeService::AsyncService* service_;
};

class StopMetricsRecorder final
    : public SimpleAsyncGrpc<StopMetricsRecorder,
                             cast::runtime::StopMetricsRecorderRequest,
                             cast::runtime::StopMetricsRecorderResponse> {
 public:
  StopMetricsRecorder(cast::runtime::RuntimeService::AsyncService* service,
                      base::WeakPtr<RuntimeServiceDelegate> delegate,
                      grpc::ServerCompletionQueue* cq,
                      bool* is_shutdown)
      : SimpleAsyncGrpc(cq),
        is_shutdown_(is_shutdown),
        delegate_(delegate),
        service_(service) {
    DCHECK(!*is_shutdown_);
    service_->RequestStopMetricsRecorder(&ctx_, &request_, &responder_, cq_,
                                         cq_, static_cast<GRPC*>(this));
  }

  cast::runtime::RuntimeService::AsyncService* service() { return service_; }
  base::WeakPtr<RuntimeServiceDelegate> delegate() { return delegate_; }
  bool* is_shutdown() { return is_shutdown_; }

  void DoMethod() {
    delegate_->StopMetricsRecorder(request_, &response_, this);
  }

 private:
  bool* is_shutdown_;
  base::WeakPtr<RuntimeServiceDelegate> delegate_;
  cast::runtime::RuntimeService::AsyncService* service_;
};

}  // namespace

RuntimeServiceDelegate::~RuntimeServiceDelegate() = default;

HeartbeatMethod::HeartbeatMethod(
    cast::runtime::RuntimeService::AsyncService* service,
    base::WeakPtr<RuntimeServiceDelegate> delegate,
    grpc::ServerCompletionQueue* cq,
    bool* is_shutdown)
    : GrpcMethod(cq),
      is_shutdown_(is_shutdown),
      service_(service),
      delegate_(delegate),
      responder_(&ctx_) {
  DCHECK(!*is_shutdown_);
  service_->RequestHeartbeat(&ctx_, &request_, &responder_, cq_, cq_,
                             static_cast<GRPC*>(this));
}

HeartbeatMethod::~HeartbeatMethod() = default;

void HeartbeatMethod::Tick() {
  DCHECK(delegate_);
  DCHECK(!*is_shutdown_);
  if (state_ == kWriteReady) {
    cast::runtime::HeartbeatResponse response;
    responder_.Write(response, this);
    state_ = kWritePending;
  }
}

void HeartbeatMethod::Finish(grpc::Status status) {
  DCHECK(delegate_);
  DCHECK(!*is_shutdown_);
  responder_.Finish(status, static_cast<GRPC*>(this));
  state_ = kFinish;
  Done();
}

GrpcMethod* HeartbeatMethod::Clone() {
  if (!delegate_ || *is_shutdown_) {
    return nullptr;
  }
  return new HeartbeatMethod(service_, delegate_, cq_, is_shutdown_);
}

void HeartbeatMethod::StepInternal(grpc::Status status) {
  switch (state_) {
    case kStart:
      DCHECK(status.ok());
      state_ = kWriteReady;
      if (delegate_) {
        delegate_->Heartbeat(request_, this);
      } else {
        delete this;
      }
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
    base::WeakPtr<RuntimeServiceDelegate> delegate,
    ::grpc::ServerCompletionQueue* cq,
    bool* is_shutdown) {
  new LoadApplication(service, delegate, cq, is_shutdown);
  new LaunchApplication(service, delegate, cq, is_shutdown);
  new StopApplication(service, delegate, cq, is_shutdown);
  new HeartbeatMethod(service, delegate, cq, is_shutdown);
  new StartMetricsRecorder(service, delegate, cq, is_shutdown);
  new StopMetricsRecorder(service, delegate, cq, is_shutdown);
}

}  // namespace chromecast
