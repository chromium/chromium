// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/libassistant/grpc/external_services/customer_registration_client.h"

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task/bind_post_task.h"
#include "base/time/time.h"
#include "chromeos/ash/services/libassistant/grpc/grpc_libassistant_client.h"
#include "chromeos/ash/services/libassistant/grpc/grpc_state.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/customer_registration_service.grpc.pb.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/delegate/event_handler_service.grpc.pb.h"

namespace ash::libassistant {

namespace {

using ::assistant::api::HeartbeatEventHandlerInterface;
using ::assistant::api::RegisterCustomerResponse;
using ::assistant::api::ServiceRegistrationRequest;
using ::assistant::api::ServiceRegistrationResponse;

// Period at which CustomerRegistrationClient sends requests to the assistant's
// customer registration service until it receives the first heartbeat.
constexpr base::TimeDelta kRegistrationPollingPeriod = base::Seconds(3);

StateConfig BuildCustomerRegistrationStateConfig() {
  StateConfig state_config;
  state_config.max_retries = 20;
  state_config.timeout_in_ms = kRegistrationPollingPeriod.InMilliseconds();
  state_config.wait_for_ready = true;
  return state_config;
}

}  // namespace

CustomerRegistrationClient::CustomerRegistrationClient(
    const std::string& customer_server_address,
    base::TimeDelta heartbeat_period,
    GrpcLibassistantClient* libassistant_client)
    : customer_server_address_(customer_server_address),
      libassistant_client_(libassistant_client) {
  DCHECK(!customer_server_address_.empty());
  DCHECK(libassistant_client_);

  customer_registration_request_.set_server_address(customer_server_address_);
  ServiceRegistrationRequest heartbeat_service_request;
  heartbeat_service_request.mutable_heartbeat_handler_metadata()
      ->set_heartbeat_response_period_ms(heartbeat_period.InMilliseconds());

  AddSupportedService(
      HeartbeatEventHandlerInterface::service_full_name(),
      heartbeat_service_request,
      base::BindOnce(
          &CustomerRegistrationClient::OnHeartbeatServiceRegistrationResponse,
          weak_factory_.GetWeakPtr()));
}

CustomerRegistrationClient::~CustomerRegistrationClient() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void CustomerRegistrationClient::AddSupportedService(
    const std::string& service_name,
    const ServiceRegistrationRequest& service_registration_request,
    ServiceRegistrationResponseCb on_service_registration_response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // All services must be registered before Start().
  DCHECK(!is_started_);
  DCHECK(on_service_registration_response);
  // Same service should not be registered more than once.
  DCHECK(!(customer_registration_request_.services().contains(service_name) ||
           service_registration_response_cbs_.contains(service_name)))
      << service_name << " has already been registered.";
  (*customer_registration_request_.mutable_services())[service_name] =
      service_registration_request;
  service_registration_response_cbs_[service_name] =
      std::move(on_service_registration_response);
}

void CustomerRegistrationClient::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_started_ = true;

  RegisterWithCustomerService();
}

void CustomerRegistrationClient::RegisterWithCustomerService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  libassistant_client_->CallServiceMethod(
      customer_registration_request_,
      base::BindOnce(
          &CustomerRegistrationClient::OnCustomerRegistrationResponse,
          weak_factory_.GetWeakPtr()),
      BuildCustomerRegistrationStateConfig());
}

void CustomerRegistrationClient::OnCustomerRegistrationResponse(
    const grpc::Status& status,
    const RegisterCustomerResponse& response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!status.ok()) {
    LOG(ERROR) << "Customer " << customer_server_address_
               << " failed to connect to CustomerRegistrationService. "
                  "Trying again...";
  } else {
    for (const auto& service_response_pair : response.services()) {
      const std::string& service_name = service_response_pair.first;
      if (!service_registration_response_cbs_.contains(service_name)) {
        // If it contains some other services that are not present in the
        // RegisterCustomerRequest, it's almost certainly a bug somewhere.
        // Instead of simply crashing the whole service, which would be too
        // severe, we put an error log before continuing as an alert of this
        // situation.
        LOG(ERROR) << "Found unregistered service " << service_name
                   << " in the RegisterCustomerResponse.";
        continue;
      }
      std::move(service_registration_response_cbs_.at(service_name))
          .Run(service_response_pair.second);
    }
    DVLOG(3) << "Customer " << customer_server_address_
             << " successfully registered with libassistant.";
  }

  // Note that regardless of the success of the RegisterCustomer Rpc, the client
  // should continue make RegisterCustomer calls until a heartbeat is received.
  // TODO(meilinw): add retry logic.
}

void CustomerRegistrationClient::OnHeartbeatServiceRegistrationResponse(
    const ServiceRegistrationResponse& response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // This really should never fail. If it does, there's almost certainly a bug
  // somewhere, but crashing the device is too extreme of a reaction, and
  // there's nothing that can be done to recover from this error.
  if (response.result() != ServiceRegistrationResponse::SUCCESS) {
    LOG(ERROR) << "Customer " << customer_server_address_
               << " failed to register its HearbeatService with Libassistant";
  }
}

}  // namespace ash::libassistant
