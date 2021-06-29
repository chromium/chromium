// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_LIBASSISTANT_GRPC_GRPC_LIBASSISTANT_CLIENT_H_
#define CHROMEOS_SERVICES_LIBASSISTANT_GRPC_GRPC_LIBASSISTANT_CLIENT_H_

#include <memory>

#include "chromeos/assistant/internal/proto/shared/proto/v2/customer_registration_interface.pb.h"
#include "chromeos/services/libassistant/grpc/grpc_client_thread.h"
#include "chromeos/services/libassistant/grpc/grpc_state.h"
#include "chromeos/services/libassistant/grpc/grpc_util.h"
#include "third_party/grpc/src/include/grpcpp/channel.h"

namespace chromeos {
namespace libassistant {

namespace {

// Defines one async client method.
#define LIBAS_GRPC_CLIENT_INTERFACE(method)                               \
  void method(const ::assistant::api::method##Request& request,           \
              chromeos::libassistant::ResponseCallback<                   \
                  grpc::Status, ::assistant::api::method##Response> done, \
              chromeos::libassistant::StateConfig state_config =          \
                  chromeos::libassistant::StateConfig());

}  // namespace

// Interface for all methods we as a client can invoke from Libassistant gRPC
// services. All client methods should be implemented here to send the requests
// to server. We only introduce methods that are currently in use.
class GrpcLibassistantClient {
 public:
  explicit GrpcLibassistantClient(std::shared_ptr<grpc::Channel> channel);
  GrpcLibassistantClient(const GrpcLibassistantClient&) = delete;
  GrpcLibassistantClient& operator=(const GrpcLibassistantClient&) = delete;
  ~GrpcLibassistantClient();

  // CustomerRegistrationService:
  // Handles CustomerRegistrationRequest sent from libassistant customers to
  // register themselves before allowing to use libassistant services.
  LIBAS_GRPC_CLIENT_INTERFACE(RegisterCustomer)

 private:
  // This channel will be shared between all stubs used to communicate with
  // multiple services. All channels are reference counted and will be freed
  // automatically.
  std::shared_ptr<grpc::Channel> channel_;

  // Thread running the completion queue.
  chromeos::libassistant::GrpcClientThread client_thread_;
};

}  // namespace libassistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_LIBASSISTANT_GRPC_GRPC_LIBASSISTANT_CLIENT_H_
