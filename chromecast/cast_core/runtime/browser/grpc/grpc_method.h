// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_RUNTIME_BROWSER_GRPC_GRPC_METHOD_H_
#define CHROMECAST_CAST_CORE_RUNTIME_BROWSER_GRPC_GRPC_METHOD_H_

#include "third_party/grpc/src/include/grpcpp/client_context.h"
#include "third_party/grpc/src/include/grpcpp/completion_queue.h"
#include "third_party/grpc/src/include/grpcpp/server_context.h"

namespace chromecast {

// This class provides a basic interface for managing some boilerplate
// associated with handling asynchronous gRPC calls.  The common practice with
// async gRPC calls is to have an object without an explicit owner whose address
// is used as a tag in the gRPC completion queue.  The completion queue returns
// this "tag" when a remote gRPC call is made, which can then be converted back
// to the object's address.  Whenever the object is done, it deletes itself.
// This class provides a standard way to buy into that general lifecycle.
class GRPC {
 public:
  GRPC() = default;
  virtual ~GRPC() = default;

  GRPC(const GRPC&) = delete;
  GRPC(GRPC&&) = delete;
  GRPC& operator=(const GRPC&) = delete;
  GRPC& operator=(GRPC&&) = delete;

  virtual void StepGRPC(grpc::Status status) = 0;
};

// This provides some additional client boilerplate for async gRPC calls.
class GrpcCall : public GRPC {
 public:
  GrpcCall();
  ~GrpcCall() override;

  GrpcCall(const GrpcCall&) = delete;
  GrpcCall(GrpcCall&&) = delete;
  GrpcCall& operator=(const GrpcCall&) = delete;
  GrpcCall& operator=(GrpcCall&&) = delete;

 protected:
  grpc::ClientContext context_;
  grpc::Status status_;
};

// This provides some boilerplate for servicing gRPC calls, including lifetime
// management with a simple state machine.
class GrpcMethod : public GRPC {
 public:
  enum State {
    kRequestPending,
    kActionPending,
    kFinish,
  };

  GrpcMethod(const GrpcMethod&) = delete;
  GrpcMethod(GrpcMethod&&) = delete;
  GrpcMethod& operator=(const GrpcMethod&) = delete;
  GrpcMethod& operator=(GrpcMethod&&) = delete;

  // Callers should not store a pointer to |this| after calling Step(), because
  // it will be destroyed if it is now finished.
  void StepGRPC(grpc::Status status) override;

  // This is used to place a new object for receiving the same method back on
  // the completion queue when an incoming call is received and will be handled
  // by |this|.
  virtual GrpcMethod* Clone() = 0;

  // Allows the underlying method implementation to run its own state machine
  // for handling the gRPC call.
  virtual void StepInternal(grpc::Status status) = 0;

 protected:
  explicit GrpcMethod(::grpc::ServerCompletionQueue* cq);
  ~GrpcMethod() override;

  // This should be called when the gRPC call has been successfully handled but
  // before the response has finished writing (i.e. before the _callback_ from
  // ServerAsyncResponseWriter::Finish) to signal that the next Step() call is
  // from the response finishing and |this| can be deleted.
  void Done();

  State method_state_{kRequestPending};
  ::grpc::ServerCompletionQueue* cq_;
  ::grpc::ServerContext ctx_;
};

}  // namespace chromecast

#endif  // CHROMECAST_CAST_CORE_RUNTIME_BROWSER_GRPC_GRPC_METHOD_H_
