// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_LIBASSISTANT_GRPC_EXTERNAL_SERVICES_CUSTOMER_REGISTRATION_CLIENT_H_
#define CHROMEOS_ASH_SERVICES_LIBASSISTANT_GRPC_EXTERNAL_SERVICES_CUSTOMER_REGISTRATION_CLIENT_H_

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/customer_registration_interface.pb.h"
#include "third_party/grpc/src/include/grpcpp/grpcpp.h"

namespace ash::libassistant {

class GrpcLibassistantClient;

// Client of libassistant's CustomerRegistrationService. One
// CustomerRegistrationClient should exist for each libassistant customer.
// Has the following responsibilities:
// - Registers the services that the customer supports. The heartbeat service
//   is supported automatically.
// - TODO(b/190645689): Re-registers with the CustomerRegistrationService if
//   libassistant fails to send heartbeats for a period of time (implying that
//   libassistant has crashed and is going through its bootup sequence again).
class CustomerRegistrationClient {
 public:
  using ServiceRegistrationResponseCb = base::OnceCallback<void(
      const ::assistant::api::ServiceRegistrationResponse&)>;

  // |customer_server_address|: The address of the server hosting the customer's
  // grpc services.
  // |heartbeat_period|: The period at which the customer would like to receive
  // heartbeats from libassistant.
  // |libassistant_client|: The gRPC client to invoke Libassistant service
  // methods.
  CustomerRegistrationClient(const std::string& customer_server_address,
                             base::TimeDelta heartbeat_period,
                             GrpcLibassistantClient* libassistant_client);
  ~CustomerRegistrationClient();

  // Adds a gRPC service that the customer supports (one that may be called by
  // libassistant). Must be called before Start(). CustomerRegistrationClient
  // will tell libassistant that this customer supports the specified service
  // when Start() is called.
  //
  // |service_name|: Must match the auto-generated gRPC service_full_name()
  // of the desired service.
  //
  // |service_registration_request|: The caller may set the appropriate metadata
  // for the corresponding service in the request. May be empty/default if
  // the service doesn't have any metadata.
  //
  // |on_service_registration_response|: Run whenever the customer service gets
  // registered with libassistant and a response is received from the
  // CustomerRegistrationService.
  //
  // All supported services must be registered before calling Start().
  void AddSupportedService(
      const std::string& service_name,
      const ::assistant::api::ServiceRegistrationRequest&
          service_registration_request,
      ServiceRegistrationResponseCb on_service_registration_response);

  // Starts trying to connect to libassistant's CustomerRegistrationService.
  // All services added through AddSupportedService() shall be present in the
  // registration request.
  void Start();

 private:
  void RegisterWithCustomerService();
  void OnCustomerRegistrationResponse(
      const grpc::Status& status,
      const ::assistant::api::RegisterCustomerResponse& response);

  void OnHeartbeatServiceRegistrationResponse(
      const ::assistant::api::ServiceRegistrationResponse& response);

  const std::string customer_server_address_;
  const raw_ptr<GrpcLibassistantClient> libassistant_client_ = nullptr;

  ::assistant::api::RegisterCustomerRequest customer_registration_request_;
  bool is_started_ = false;
  base::flat_map</*service_name=*/std::string, ServiceRegistrationResponseCb>
      service_registration_response_cbs_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<CustomerRegistrationClient> weak_factory_{this};
};

}  // namespace ash::libassistant

#endif  // CHROMEOS_ASH_SERVICES_LIBASSISTANT_GRPC_EXTERNAL_SERVICES_CUSTOMER_REGISTRATION_CLIENT_H_
