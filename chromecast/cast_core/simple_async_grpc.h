// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_SIMPLE_ASYNC_GRPC_H_
#define CHROMECAST_CAST_CORE_SIMPLE_ASYNC_GRPC_H_

#include "base/notreached.h"
#include "chromecast/cast_core/grpc_method.h"

namespace chromecast {

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
    return new T(self()->service(), self()->delegate(), cq_);
  }

  void StepInternal(grpc::Status status) override {
    switch (state_) {
      case kStart:
        DCHECK(status.ok());
        state_ = kRespond;
        self()->DoMethod();
        break;
      case kRespond:
        state_ = kFinish;
        responder_.Finish(response_, status, static_cast<GRPC*>(this));
        Done();
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

#endif  // CHROMECAST_CAST_CORE_SIMPLE_ASYNC_GRPC_H_
