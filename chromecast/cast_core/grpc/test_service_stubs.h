// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_GRPC_TEST_SERVICE_STUBS_H_
#define CHROMECAST_CAST_CORE_GRPC_TEST_SERVICE_STUBS_H_

#include "chromecast/cast_core/grpc/grpc_server_streaming_call.h"
#include "chromecast/cast_core/grpc/grpc_stub.h"
#include "chromecast/cast_core/grpc/grpc_unary_call.h"
#include "chromecast/cast_core/grpc/test_service.grpc.pb.h"
#include "chromecast/cast_core/grpc/test_service.pb.h"

namespace cast {
namespace utils {

class SimpleServiceStub final : public utils::GrpcStub<SimpleService> {
 public:
  using GrpcStub::GrpcStub;
  using GrpcStub::operator=;
  using GrpcStub::AsyncInterface;
  using GrpcStub::CreateCall;
  using GrpcStub::SyncInterface;

  using SimpleCall = utils::GrpcUnaryCall<SimpleServiceStub,
                                          TestRequest,
                                          TestResponse,
                                          &AsyncInterface::SimpleCall,
                                          &SyncInterface::SimpleCall>;
};

class ServerStreamingServiceStub
    : public utils::GrpcStub<ServerStreamingService> {
 public:
  using GrpcStub::GrpcStub;
  using GrpcStub::operator=;
  using GrpcStub::AsyncInterface;
  using GrpcStub::CreateCall;
  using GrpcStub::SyncInterface;

  using StreamingCall =
      utils::GrpcServerStreamingCall<ServerStreamingServiceStub,
                                     TestRequest,
                                     TestResponse,
                                     &AsyncInterface::StreamingCall>;
};

}  // namespace utils
}  // namespace cast

#endif  // CHROMECAST_CAST_CORE_GRPC_TEST_SERVICE_STUBS_H_
