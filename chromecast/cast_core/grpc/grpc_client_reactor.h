// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_GRPC_GRPC_CLIENT_REACTOR_H_
#define CHROMECAST_CAST_CORE_GRPC_GRPC_CLIENT_REACTOR_H_

#include <grpcpp/grpcpp.h>

#include <memory>

#include "base/task/sequenced_task_runner.h"
#include "chromecast/cast_core/grpc/grpc_call_options.h"

namespace cast {
namespace utils {

// A base class for all gRPC client reactor implementations.
//
// The gRPC Callback stack is built on the Reactor concept which serves as the
// observer/callback mechanism for notifications on received responses. The
// instance of the Reactor always belongs to the gRPC framework. It is created
// in GrpcCall implementation and deleted in Reactor::OnDone callback from gRPC.
//
// |TRequest| is the gRPC API request type.
// |TUnderlyingReactor| is the type of the gRPC framework reactor that is added
// as a base class for GrpcClientReactor and used in implementation.
template <typename TRequest, typename TUnderlyingReactor>
class GrpcClientReactor : public TUnderlyingReactor {
 public:
  ~GrpcClientReactor() override = default;

  // Copy and move are deleted.
  GrpcClientReactor(const GrpcClientReactor&) = delete;
  GrpcClientReactor(GrpcClientReactor&&) = delete;
  GrpcClientReactor& operator=(const GrpcClientReactor&) = delete;
  GrpcClientReactor& operator=(GrpcClientReactor&&) = delete;

  // Returns the gRPC client context.
  grpc::ClientContext* context() { return &context_; }

  // Returns the original request.
  const TRequest* request() const { return &request_; }

  // Initiates the gRPC client call.
  virtual void Start() { options_.ApplyOptionsToContext(&context_); }

 protected:
  explicit GrpcClientReactor(TRequest request, GrpcCallOptions options)
      : request_(std::move(request)),
        options_(std::move(options)),
        task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {}

  void DeleteThis() {
    // Client reactors must be deleted asynchronously to avoid a crash in debug
    // builds caused absl::Mutex assert on "unlocking a mutex after dtor"
    // triggered by the ClientContext mutex in TryCancel call.
    task_runner_->DeleteSoon(FROM_HERE, this);
  }

 private:
  grpc::ClientContext context_;
  TRequest request_;
  GrpcCallOptions options_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

}  // namespace utils
}  // namespace cast

#endif  // CHROMECAST_CAST_CORE_GRPC_GRPC_CLIENT_REACTOR_H_
