// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_LIBASSISTANT_GRPC_EXTERNAL_SERVICES_GRPC_SERVICES_INITIALIZER_H_
#define CHROMEOS_SERVICES_LIBASSISTANT_GRPC_EXTERNAL_SERVICES_GRPC_SERVICES_INITIALIZER_H_

#include <memory>
#include <string>

#include "base/containers/flat_set.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"

namespace chromeos {
namespace libassistant {

class GrpcLibassistantClient;

// Component responsible for:
// 1. Set up a gRPC client by establishing a new channel with Libassistant
// server.
// 2. Start and manage Libassistant V2 event observer gRPC services.
class GrpcServicesInitializer {
 public:
  explicit GrpcServicesInitializer(
      const std::string& libassistant_service_address);
  GrpcServicesInitializer(const GrpcServicesInitializer&) = delete;
  GrpcServicesInitializer& operator=(const GrpcServicesInitializer&) = delete;
  ~GrpcServicesInitializer();

  // Expose a reference to |GrpcLibassistantClient|.
  GrpcLibassistantClient& GrpcLibassistantClient();

 private:
  // 1. Creates a channel object to obtain a handle to libassistant gRPC server
  // services.
  // 2. Creates a gRPC client using that channel and through which we can invoke
  // service methods implemented in the server.
  void InitGrpcClient();

  // Address of Libassistant gRPC server.
  const std::string libassistant_service_address_;

  // The entrypoint through which we can query Libassistant V2 APIs.
  std::unique_ptr<chromeos::libassistant::GrpcLibassistantClient>
      libassistant_client_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<GrpcServicesInitializer> weak_factory_{this};
};

}  // namespace libassistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_LIBASSISTANT_GRPC_EXTERNAL_SERVICES_GRPC_SERVICES_INITIALIZER_H_
