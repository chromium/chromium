// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_GRPC_GRPC_CALL_H_
#define CHROMECAST_CAST_CORE_GRPC_GRPC_CALL_H_

#include <grpcpp/grpcpp.h>

#include <utility>

#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "chromecast/cast_core/grpc/grpc_call_options.h"

namespace cast {
namespace utils {

// The base class for all gRPC call implementations. Provides some common
// functionality used to instantiate the call (ie request, stub interface etc).
template <typename TGrpcStub, typename TRequest>
class GrpcCall {
 public:
  using SyncInterface = typename TGrpcStub::SyncInterface;
  using AsyncInterface = typename TGrpcStub::AsyncInterface;
  using Request = TRequest;

  // Client call context valid only through duration of the call.
  class Context {
   public:
    explicit Context(grpc::ClientContext* grpc_context)
        : grpc_context_(grpc_context) {}

    // Try cancelling the call.
    void Cancel() { grpc_context_->TryCancel(); }

   private:
    raw_ptr<grpc::ClientContext> grpc_context_;
  };

  explicit GrpcCall(SyncInterface* stub) : GrpcCall(stub, Request()) {}

  GrpcCall(SyncInterface* stub, Request request)
      : stub_(stub), request_(std::move(request)) {
    DCHECK(stub_);
    async_ = stub_->async();
  }

  virtual ~GrpcCall() = default;

  // Returns the reference to the request.
  Request& request() & { return request_; }

  // Returns the move reference to the request.
  Request&& request() && { return std::move(request_); }

  // Sets a deadline for gRPC call.
  void SetDeadline(int64_t deadline_ms) { options_.SetDeadline(deadline_ms); }

 protected:
  SyncInterface* sync() && {
    DCHECK(stub_);
    return std::exchange(stub_, nullptr);
  }

  AsyncInterface* async() && {
    DCHECK(async_);
    return std::exchange(async_, nullptr);
  }

  GrpcCallOptions&& options() && { return std::move(options_); }

 private:
  raw_ptr<SyncInterface> stub_;
  raw_ptr<AsyncInterface> async_;
  Request request_;
  GrpcCallOptions options_;
};

}  // namespace utils
}  // namespace cast

#endif  // CHROMECAST_CAST_CORE_GRPC_GRPC_CALL_H_
