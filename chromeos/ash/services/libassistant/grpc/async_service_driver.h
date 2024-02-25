// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_LIBASSISTANT_GRPC_ASYNC_SERVICE_DRIVER_H_
#define CHROMEOS_ASH_SERVICES_LIBASSISTANT_GRPC_ASYNC_SERVICE_DRIVER_H_

#include "base/check.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "third_party/grpc/src/include/grpcpp/server_builder.h"

namespace ash::libassistant {

// Base class for asynchronous RPC drivers. Implementations of async drivers
// for gRPC services exposed by libassistant should derive from this class.
class AsyncServiceDriver {
 public:
  // |server_builder| should not be nullptr.
  explicit AsyncServiceDriver(grpc::ServerBuilder* server_builder)
      : server_builder_(server_builder) {
    DCHECK(server_builder);
  }

  virtual ~AsyncServiceDriver() = default;

  virtual void StartCQ(grpc::ServerCompletionQueue* cq) = 0;

 protected:
  // Owned by |GrpcServicesInitializer|.
  raw_ptr<grpc::ServerBuilder> server_builder_;
};

}  // namespace ash::libassistant

#endif  // CHROMEOS_ASH_SERVICES_LIBASSISTANT_GRPC_ASYNC_SERVICE_DRIVER_H_
