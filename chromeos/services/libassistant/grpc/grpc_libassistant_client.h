// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_LIBASSISTANT_GRPC_GRPC_LIBASSISTANT_CLIENT_H_
#define CHROMEOS_SERVICES_LIBASSISTANT_GRPC_GRPC_LIBASSISTANT_CLIENT_H_

#include <memory>

#include "third_party/grpc/src/include/grpcpp/channel.h"

namespace chromeos {
namespace libassistant {

// Defines grpc client for ipc. All client methods should be implemented here to
// send the requests to server.
class GrpcLibassistantClient {
 public:
  explicit GrpcLibassistantClient(std::shared_ptr<grpc::Channel> channel);
  GrpcLibassistantClient(const GrpcLibassistantClient&) = delete;
  GrpcLibassistantClient& operator=(const GrpcLibassistantClient&) = delete;
  ~GrpcLibassistantClient();

  // CustomerRegistrationService:
  // Handles CustomerRegistrationRequest sent from libassistant customers to
  // register themselves before allowing to use libassistant services.
  void RegisterCustomer();

 private:
  // This channel will be shared between all stubs used to communicate with
  // multiple services. All channels are reference counted and will be freed
  // automatically.
  std::shared_ptr<grpc::Channel> channel_;
};

}  // namespace libassistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_LIBASSISTANT_GRPC_GRPC_LIBASSISTANT_CLIENT_H_
