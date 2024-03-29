// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_GRPC_GRPC_SERVER_STREAMING_HANDLER_H_
#define CHROMECAST_CAST_CORE_GRPC_GRPC_SERVER_STREAMING_HANDLER_H_

#include <grpcpp/grpcpp.h>

#include "base/functional/callback.h"
#include "base/logging.h"
#include "chromecast/cast_core/grpc/cancellable_reactor.h"
#include "chromecast/cast_core/grpc/grpc_server.h"
#include "chromecast/cast_core/grpc/grpc_server_reactor.h"
#include "chromecast/cast_core/grpc/grpc_status_or.h"
#include "chromecast/cast_core/grpc/server_reactor_tracker.h"
#include "chromecast/cast_core/grpc/trackable_reactor.h"

namespace cast {
namespace utils {

// A generic handler for server streaming unary, ie request followed by multiple
// responses from server, gRPC APIs. Can only be used with rpc that have the
// following signature:
//       rpc Foo(Request) returns (stream Response)
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
class GrpcServerStreamingHandler : public GrpcHandler {
 public:
  using ReactorBase = GrpcServerReactor<TRequest, TResponse>;

  // Reactor implementation of server streaming handler.
  class Reactor : public ReactorBase {
   public:
    using ReactorBase::name;
    using ReactorBase::Write;

    using OnRequestCallback = base::RepeatingCallback<void(TRequest, Reactor*)>;
    using WritesAvailableCallback =
        base::RepeatingCallback<void(grpc::Status, Reactor*)>;

    template <typename... Args>
    explicit Reactor(OnRequestCallback on_request_callback, Args&&... args)
        : ReactorBase(std::forward<Args>(args)...),
          on_request_callback_(std::move(on_request_callback)) {
      ReadRequest();
    }

    // Sets the callback that is called when writes are available.
    void SetWritesAvailableCallback(
        WritesAvailableCallback writes_available_callback) {
      writes_available_callback_ = std::move(writes_available_callback);
    }

    // Writes a packet and sets the writes availability callback.
    void Write(TResponse response,
               WritesAvailableCallback writes_available_callback) {
      writes_available_callback_ = std::move(writes_available_callback);
      ReactorBase::Write(std::move(response));
    }

   protected:
    using ReactorBase::Finish;
    using ReactorBase::ReadRequest;
    using ReactorBase::StartRead;
    using ReactorBase::StartWrite;

    // Implements GrpcServerReactor APIs.
    void WriteResponse(const grpc::ByteBuffer* buffer) override {
      DCHECK(buffer);
      StartWrite(buffer, grpc::WriteOptions());
    }

    void FinishWriting(const grpc::ByteBuffer* buffer,
                       const grpc::Status& status) override {
      DVLOG(1) << "Reactor finished: " << name()
               << ", status=" << GrpcStatusToString(status);
      DCHECK(!buffer)
          << "Server streaming call can only be finished with a status";
      if (!status.ok() && writes_available_callback_) {
        // A signal that the caller has aborted the streaming session.
        writes_available_callback_.Run(status, nullptr);
      }
      Finish(status);
    }

    void OnResponseDone(const grpc::Status& status) override {
      DCHECK(writes_available_callback_)
          << "Writes available callback must be set";
      writes_available_callback_.Run(status, status.ok() ? this : nullptr);
    }

    void OnRequestDone(GrpcStatusOr<TRequest> request) override {
      if (!request.ok()) {
        FinishWriting(nullptr, request.status());
        return;
      }

      on_request_callback_.Run(std::move(*request), this);
    }

    OnRequestCallback on_request_callback_;
    WritesAvailableCallback writes_available_callback_;
  };

  using OnRequestCallback = typename Reactor::OnRequestCallback;
  using WritesAvailableCallback = typename Reactor::WritesAvailableCallback;

  GrpcServerStreamingHandler(OnRequestCallback on_request_callback,
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

#endif  // CHROMECAST_CAST_CORE_GRPC_GRPC_SERVER_STREAMING_HANDLER_H_
