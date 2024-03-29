// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_GRPC_GRPC_UNARY_HANDLER_H_
#define CHROMECAST_CAST_CORE_GRPC_GRPC_UNARY_HANDLER_H_

#include <grpcpp/grpcpp.h>

#include "base/functional/callback.h"
#include "base/logging.h"
#include "chromecast/cast_core/grpc/cancellable_reactor.h"
#include "chromecast/cast_core/grpc/grpc_handler.h"
#include "chromecast/cast_core/grpc/grpc_server_reactor.h"
#include "chromecast/cast_core/grpc/grpc_status_or.h"
#include "chromecast/cast_core/grpc/server_reactor_tracker.h"
#include "chromecast/cast_core/grpc/trackable_reactor.h"

namespace cast {
namespace utils {

// A generic handler for unary, ie request/response, gRPC APIs. Can only be used
// with rpc that have the following signature:
//       rpc Foo(Request) returns (Response)
//
// - TService is the gRPC service type.
// - TRequest is the service request type.
// - TResponse is the service response type.
// - MethodName is the rpc method as a string.
//
// This class is not thread-safe. Appropriate means have to be added by the
// users to guarantee thread-safety (ie task runners, mutexes etc).
template <typename TService,
          typename TRequest,
          typename TResponse,
          const char* MethodName>
class GrpcUnaryHandler final : public GrpcHandler {
 public:
  using ReactorBase = GrpcServerReactor<TRequest, TResponse>;

  class Reactor : public ReactorBase {
   public:
    using ReactorBase::name;
    using ReactorBase::Write;

    using OnRequestCallback = base::RepeatingCallback<void(TRequest, Reactor*)>;

    template <typename... TArgs>
    explicit Reactor(OnRequestCallback on_request_callback, TArgs&&... args)
        : ReactorBase(std::forward<TArgs>(args)...),
          on_request_callback_(std::move(on_request_callback)) {
      ReadRequest();
    }

   protected:
    using ReactorBase::Finish;
    using ReactorBase::ReadRequest;
    using ReactorBase::StartRead;
    using ReactorBase::StartWriteAndFinish;

    // Implements GrpcServerReactor APIs.
    void WriteResponse(const grpc::ByteBuffer* buffer) override {
      DCHECK(buffer);
      FinishWriting(buffer, grpc::Status::OK);
    }

    void FinishWriting(const grpc::ByteBuffer* buffer,
                       const grpc::Status& status) override {
      DCHECK((status.ok() && buffer) || !status.ok())
          << "Either buffer must be set or status must flag an error";
      DVLOG(1) << "Reactor is finished: " << name()
               << ", status=" << GrpcStatusToString(status);
      if (status.ok()) {
        StartWriteAndFinish(buffer, grpc::WriteOptions(), grpc::Status::OK);
      } else {
        Finish(status);
      }
    }

    void OnResponseDone(const grpc::Status& status) override {
      // This method may be called from the cancelled_reactor as a generic way
      // to signal reactor is done via OnResponseDone API. For unary reactor it
      // is a no-op.
      CHECK(status.error_code() == grpc::StatusCode::ABORTED)
          << "Unexpected status: " << GrpcStatusToString(status);
    }

    void OnRequestDone(GrpcStatusOr<TRequest> request) override {
      if (!request.ok()) {
        FinishWriting(nullptr, request.status());
        return;
      }
      on_request_callback_.Run(std::move(request).value(), this);
    }

    OnRequestCallback on_request_callback_;
  };

  using Response = TResponse;
  using OnRequestCallback = typename Reactor::OnRequestCallback;

  GrpcUnaryHandler(OnRequestCallback on_request_callback,
                   ServerReactorTracker* server_reactor_tracker)
      : GrpcHandler(server_reactor_tracker),
        on_request_callback_(std::move(on_request_callback)) {}

  static std::string rpc_name() {
    return std::string("/") + TService::service_full_name() + "/" + MethodName;
  }

 private:
  // Implements GrpcHandler APIs.
  grpc::ServerGenericBidiReactor* CreateReactor(
      grpc::CallbackServerContext* context) override {
    return new CancellableReactor<TrackableReactor<Reactor>>(
        server_reactor_tracker(), on_request_callback_, rpc_name(), context);
  }

  OnRequestCallback on_request_callback_;
};

}  // namespace utils
}  // namespace cast

#endif  // CHROMECAST_CAST_CORE_GRPC_GRPC_UNARY_HANDLER_H_
