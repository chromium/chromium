// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_GRPC_GRPC_SERVER_BUILDER_H_
#define CHROMECAST_CAST_CORE_GRPC_GRPC_SERVER_BUILDER_H_

#include <grpcpp/grpcpp.h>

#include <memory>
#include <string>

namespace cast {
namespace utils {

// An grpc::Server builder interface which enables injection of additional logic
// to the vanilla grpc::ServerBuilder. For example, enabling gRPC on Android via
// IPC binder.
class GrpcServerBuilder {
 public:
  virtual ~GrpcServerBuilder();

  // Create an instance of GrpcServerBuilder. May be overridden per platform.
  static std::unique_ptr<GrpcServerBuilder> Create();

  // Adds a gRPC server listening port.
  virtual GrpcServerBuilder& AddListeningPort(
      const std::string& endpoint,
      std::shared_ptr<grpc::ServerCredentials> creds,
      int* selected_port = nullptr) = 0;

  // Registers a generic service that uses the callback API.
  virtual GrpcServerBuilder& RegisterCallbackGenericService(
      grpc::CallbackGenericService* service) = 0;

  // Builds and starts the gRPC server.
  virtual std::unique_ptr<grpc::Server> BuildAndStart() = 0;
};

}  // namespace utils
}  // namespace cast

#endif  // CHROMECAST_CAST_CORE_GRPC_GRPC_SERVER_BUILDER_H_
