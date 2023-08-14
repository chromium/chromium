// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_GRPC_GRPC_HANDLER_H_
#define CHROMECAST_CAST_CORE_GRPC_GRPC_HANDLER_H_

#include <grpcpp/generic/async_generic_service.h>
#include <grpcpp/grpcpp.h>

#include "base/memory/raw_ptr.h"
#include "chromecast/cast_core/grpc/server_reactor_tracker.h"

namespace cast {
namespace utils {

// A base class for all gRPC server reactor implementations.
//
// This class provides a mechanism to track currently active reactors. The data
// is used during server shutdown to notify about pending non-finished reactors.
class GrpcHandler {
 public:
  explicit GrpcHandler(ServerReactorTracker* server_reactor_tracker);
  virtual ~GrpcHandler();

  // Returns a tracker of all the reactors of the implemented handler.
  ServerReactorTracker* server_reactor_tracker() {
    return server_reactor_tracker_;
  }

  // Creates a reactor used to process specific gRPC API.
  virtual grpc::ServerGenericBidiReactor* CreateReactor(
      grpc::CallbackServerContext* context) = 0;

 private:
  raw_ptr<ServerReactorTracker> server_reactor_tracker_;
};

}  // namespace utils
}  // namespace cast

#endif  // CHROMECAST_CAST_CORE_GRPC_GRPC_HANDLER_H_
