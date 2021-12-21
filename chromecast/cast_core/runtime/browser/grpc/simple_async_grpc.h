// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_RUNTIME_BROWSER_GRPC_SIMPLE_ASYNC_GRPC_H_
#define CHROMECAST_CAST_CORE_RUNTIME_BROWSER_GRPC_SIMPLE_ASYNC_GRPC_H_

#include "base/notreached.h"
#include "chromecast/cast_core/runtime/browser/grpc/grpc_method.h"

namespace chromecast {

// This class is some common state machine glue between GrpcServer, request
// objects, and their delegates.  There are two threads/sequences involved: the
// gRPC thread and the main task runner.  The important element that this class
// helps handle is that after GrpcServer::Stop(), we can't reference gRPC
// objects that may lead back to the underlying server.  This includes not
// making any new requests (i.e. not call `new T` in Clone()).
//
// There are two signals this class uses to cancel gRPC operations: |delegate|
// and |is_shutdown|.  |delegate| should return nullptr when the actual delegate
// is no longer alive (so it's likely a base::WeakPtr).  |is_shutdown| should
// return a bool* that, when dereferenced, indicates whether the GrpcServer has
// been or is being shutdown.  |is_shtudown| will not be dereferenced when
// |delegate| is nullptr, so it's safe to tie |is_shutdown| to the lifetime of
// |delegate|.
template <typename T, typename RequestType, typename ResponseType>
class SimpleAsyncGrpc : public GrpcMethod {
 public:
  enum State {
    kStart,
    kRespond,
    kFinish,
  };

  explicit SimpleAsyncGrpc(grpc::ServerCompletionQueue* cq)
      : GrpcMethod(cq), responder_(&ctx_) {}

  GrpcMethod* Clone() override {
    if (!self()->delegate() || *self()->is_shutdown()) {
      return nullptr;
    }
    return new T(self()->service(), self()->delegate(), cq_,
                 self()->is_shutdown());
  }

  void StepInternal(grpc::Status status) override {
    switch (state_) {
      case kStart:
        DCHECK(status.ok());
        state_ = kRespond;
        if (self()->delegate()) {
          self()->DoMethod();
        } else {
          delete this;
        }
        break;
      case kRespond:
        state_ = kFinish;
        if (!status.ok() || !self()->delegate() || *self()->is_shutdown()) {
          delete this;
        } else {
          responder_.Finish(response_, status, static_cast<GRPC*>(this));
          Done();
        }
        break;
      default:
        NOTREACHED();
        break;
    }
  }

 protected:
  State state_{kStart};
  RequestType request_;
  ResponseType response_;
  ::grpc::ServerAsyncResponseWriter<ResponseType> responder_;

 private:
  T* self() { return static_cast<T*>(this); }
};

}  // namespace chromecast

#endif  // CHROMECAST_CAST_CORE_RUNTIME_BROWSER_GRPC_SIMPLE_ASYNC_GRPC_H_
