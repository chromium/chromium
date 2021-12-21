// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/runtime/browser/grpc/grpc_method.h"

namespace chromecast {

GrpcCall::GrpcCall() = default;

GrpcCall::~GrpcCall() = default;

void GrpcMethod::StepGRPC(grpc::Status status) {
  switch (method_state_) {
    case kRequestPending:
      if (!status.ok()) {
        delete this;
        return;
      }
      method_state_ = kActionPending;
      Clone();
      StepInternal(status);
      break;
    case kActionPending:
      StepInternal(status);
      break;
    case kFinish:
      delete this;
      break;
  }
}

GrpcMethod::GrpcMethod(::grpc::ServerCompletionQueue* cq) : cq_(cq) {}

GrpcMethod::~GrpcMethod() = default;

void GrpcMethod::Done() {
  method_state_ = kFinish;
}

}  // namespace chromecast
