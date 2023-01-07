// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_GRPC_GRPC_STUB_H_
#define CHROMECAST_CAST_CORE_GRPC_GRPC_STUB_H_

#include <grpcpp/grpcpp.h>

#include "base/logging.h"
#include "chromecast/cast_core/grpc/grpc_server_streaming_call.h"
#include "chromecast/cast_core/grpc/grpc_unary_call.h"

namespace cast {
namespace utils {

// A gRPC stub definition with a copy/move enabled constructors and assignment
// operators.
template <typename TService>
class GrpcStub {
 public:
  using SyncInterface = typename TService::StubInterface;
  using AsyncInterface = typename TService::StubInterface::async_interface;

  // Constructs a service stub on an |endpoint|. The is a fast call as gRPC
  // creates actual resources for the channel in the background thread.
  explicit GrpcStub(const std::string& endpoint)
      : GrpcStub(grpc::CreateChannel(endpoint,
                                     grpc::InsecureChannelCredentials())) {}

  // Constructs a service stub with an existing |channel|. The is a fast call
  // that shares an existing channel with a new stub.
  explicit GrpcStub(std::shared_ptr<grpc::Channel> channel)
      : channel_(std::move(channel)), stub_(TService::NewStub(channel_)) {
    DCHECK(channel_);
  }

  // Copy constructor that reuses the |channel_|.
  GrpcStub(const GrpcStub& rhs) : GrpcStub(rhs.channel_) {}

  // Assignment operator that reuses the |channel_|.
  GrpcStub& operator=(const GrpcStub& rhs) {
    DCHECK(rhs.channel_);
    channel_ = rhs.channel_;
    stub_ = TService::NewStub(rhs.channel_);
    return *this;
  }

  // Default specification of move ctor's - placeholder for future expansion.
  GrpcStub(GrpcStub&& rhs) = default;
  GrpcStub& operator=(GrpcStub&& rhs) = default;

  virtual ~GrpcStub() = default;

  // Creates an instance of TGrpcCall that provides the request object and
  // invoke APIs:
  //   auto call = stub.CreateCall();
  //   call.request()->set_xxx();
  //   std::move(call).InvokeAsync();
  template <typename TGrpcCall>
  TGrpcCall CreateCall() {
    return TGrpcCall(stub_.get());
  }

  // Creates an instance of TGrpcCall and sets the initial |request| object.
  template <typename TGrpcCall>
  TGrpcCall CreateCall(typename TGrpcCall::Request request) {
    return TGrpcCall(stub_.get(), std::move(request));
  }

 private:
  std::shared_ptr<grpc::Channel> channel_;
  std::unique_ptr<typename TService::StubInterface> stub_;
};

}  // namespace utils
}  // namespace cast

#endif  // CHROMECAST_CAST_CORE_GRPC_GRPC_STUB_H_
