// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_GRPC_TEST_SERVICE_HANDLERS_H_
#define CHROMECAST_CAST_CORE_GRPC_TEST_SERVICE_HANDLERS_H_

#include "chromecast/cast_core/grpc/grpc_server_streaming_handler.h"
#include "chromecast/cast_core/grpc/grpc_unary_handler.h"
#include "chromecast/cast_core/grpc/test_service.grpc.pb.h"
#include "chromecast/cast_core/grpc/test_service.pb.h"

namespace cast {
namespace utils {

class SimpleServiceHandler {
 private:
  static const char kSimpleCall[];

 public:
  using SimpleCall = utils::
      GrpcUnaryHandler<SimpleService, TestRequest, TestResponse, kSimpleCall>;
};

class ServerStreamingServiceHandler {
 private:
  static const char kStreamingCall[];

 public:
  using StreamingCall =
      utils::GrpcServerStreamingHandler<ServerStreamingService,
                                        TestRequest,
                                        TestResponse,
                                        kStreamingCall>;
};

}  // namespace utils
}  // namespace cast

#endif  // CHROMECAST_CAST_CORE_GRPC_TEST_SERVICE_HANDLERS_H_
