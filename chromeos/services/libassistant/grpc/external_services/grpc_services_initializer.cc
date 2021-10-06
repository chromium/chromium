// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/libassistant/grpc/external_services/grpc_services_initializer.h"

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "chromeos/services/libassistant/grpc/external_services/customer_registration_client.h"
#include "chromeos/services/libassistant/grpc/external_services/heartbeat_event_handler_driver.h"
#include "chromeos/services/libassistant/grpc/grpc_libassistant_client.h"
#include "chromeos/services/libassistant/grpc/grpc_util.h"
#include "third_party/grpc/src/include/grpc/grpc_security_constants.h"
#include "third_party/grpc/src/include/grpc/impl/codegen/grpc_types.h"
#include "third_party/grpc/src/include/grpcpp/create_channel.h"
#include "third_party/grpc/src/include/grpcpp/security/credentials.h"
#include "third_party/grpc/src/include/grpcpp/security/server_credentials.h"
#include "third_party/grpc/src/include/grpcpp/support/channel_arguments.h"

namespace chromeos {
namespace libassistant {

namespace {

// Desired time between consecutive heartbeats.
constexpr base::TimeDelta kHeartbeatInterval = base::Seconds(2);

}  // namespace

GrpcServicesInitializer::GrpcServicesInitializer(
    const std::string& libassistant_service_address,
    const std::string& assistant_service_address)
    : ServicesInitializerBase(
          /*cq_thread_name=*/assistant_service_address + ".GrpcCQ",
          /*main_task_runner=*/base::SequencedTaskRunnerHandle::Get()),
      assistant_service_address_(assistant_service_address),
      libassistant_service_address_(libassistant_service_address) {
  DCHECK(!libassistant_service_address.empty());
  DCHECK(!assistant_service_address.empty());

  InitLibassistGrpcClient();
  InitAssistantGrpcServer();

  customer_registration_client_ = std::make_unique<CustomerRegistrationClient>(
      assistant_service_address_, kHeartbeatInterval,
      libassistant_client_.get());
}

GrpcServicesInitializer::~GrpcServicesInitializer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (assistant_grpc_server_)
    assistant_grpc_server_->Shutdown();

  StopCQ();
}

bool GrpcServicesInitializer::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Starts the server after all drivers have been initiated.
  assistant_grpc_server_ = server_builder_.BuildAndStart();

  if (!assistant_grpc_server_) {
    LOG(ERROR) << "Failed to start a server for ChromeOS Assistant gRPC.";
    return false;
  }

  DVLOG(1) << "Started ChromeOS Assistant gRPC service";

  StartCQ();
  customer_registration_client_->Start();
  return true;
}

GrpcLibassistantClient& GrpcServicesInitializer::GrpcLibassistantClient() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return *libassistant_client_;
}

void GrpcServicesInitializer::InitDrivers(grpc::ServerBuilder* server_builder) {
  // Inits heartbeat driver.
  auto heartbeat_driver =
      std::make_unique<HeartbeatEventHandlerDriver>(&server_builder_);
  heartbeat_event_observation_.Observe(heartbeat_driver.get());

  service_drivers_.emplace_back(std::move(heartbeat_driver));
}

void GrpcServicesInitializer::InitLibassistGrpcClient() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  grpc::ChannelArguments channel_args;
  channel_args.SetInt(GRPC_ARG_INITIAL_RECONNECT_BACKOFF_MS, 200);
  channel_args.SetInt(GRPC_ARG_MIN_RECONNECT_BACKOFF_MS, 200);
  channel_args.SetInt(GRPC_ARG_MAX_RECONNECT_BACKOFF_MS, 2000);
  grpc_local_connect_type connect_type =
      GetGrpcLocalConnectType(libassistant_service_address_);

  auto channel = grpc::CreateCustomChannel(
      libassistant_service_address_,
      ::grpc::experimental::LocalCredentials(connect_type), channel_args);

  libassistant_client_ =
      std::make_unique<chromeos::libassistant::GrpcLibassistantClient>(channel);
}

void GrpcServicesInitializer::InitAssistantGrpcServer() {
  auto connect_type = GetGrpcLocalConnectType(assistant_service_address_);
  // Listen on the given address with the specified credentials.
  server_builder_.AddListeningPort(
      assistant_service_address_,
      ::grpc::experimental::LocalServerCredentials(connect_type));
  RegisterServicesAndInitCQ(&server_builder_);
}

}  // namespace libassistant
}  // namespace chromeos
