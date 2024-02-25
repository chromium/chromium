// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/libassistant/grpc/external_services/grpc_services_initializer.h"

#include <memory>

#include "base/system/sys_info.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chromeos/ash/services/libassistant/grpc/external_services/action_service.h"
#include "chromeos/ash/services/libassistant/grpc/external_services/customer_registration_client.h"
#include "chromeos/ash/services/libassistant/grpc/external_services/heartbeat_event_handler_driver.h"
#include "chromeos/ash/services/libassistant/grpc/grpc_http_connection_client.h"
#include "chromeos/ash/services/libassistant/grpc/grpc_libassistant_client.h"
#include "chromeos/ash/services/libassistant/grpc/grpc_util.h"
#include "chromeos/assistant/internal/internal_constants.h"
#include "chromeos/assistant/internal/libassistant/shared_headers.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/delegate/event_handler_interface.pb.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/delegate/event_handler_service.grpc.pb.h"
#include "third_party/grpc/src/include/grpc/grpc_security_constants.h"
#include "third_party/grpc/src/include/grpc/impl/codegen/grpc_types.h"
#include "third_party/grpc/src/include/grpcpp/create_channel.h"
#include "third_party/grpc/src/include/grpcpp/security/credentials.h"
#include "third_party/grpc/src/include/grpcpp/security/server_credentials.h"
#include "third_party/grpc/src/include/grpcpp/support/channel_arguments.h"

namespace ash::libassistant {

namespace {

// Desired time between consecutive heartbeats.
constexpr base::TimeDelta kHeartbeatInterval = base::Seconds(2);

}  // namespace

GrpcServicesInitializer::GrpcServicesInitializer(
    const std::string& libassistant_service_address,
    const std::string& assistant_service_address)
    : ServicesInitializerBase(
          /*cq_thread_name=*/assistant_service_address + ".GrpcCQ",
          /*main_task_runner=*/base::SequencedTaskRunner::GetCurrentDefault()),
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
  if (assistant_grpc_server_)
    assistant_grpc_server_->Shutdown();

  StopCQ();
}

bool GrpcServicesInitializer::Start() {
  // Starts the server after all drivers have been initiated.
  assistant_grpc_server_ = server_builder_.BuildAndStart();

  if (!assistant_grpc_server_) {
    LOG(ERROR) << "Failed to start a server for ChromeOS Assistant gRPC.";
    return false;
  }

  DVLOG(1) << "Started ChromeOS Assistant gRPC service";

  RegisterEventHandlers();
  StartCQ();
  customer_registration_client_->Start();
  return true;
}

void GrpcServicesInitializer::AddAlarmTimerEventObserver(
    GrpcServicesObserver<::assistant::api::OnAlarmTimerEventRequest>*
        observer) {
  alarm_timer_event_handler_driver_->AddObserver(observer);
}

void GrpcServicesInitializer::AddAssistantDisplayEventObserver(
    GrpcServicesObserver<::assistant::api::OnAssistantDisplayEventRequest>*
        observer) {
  assistant_display_event_handler_driver_->AddObserver(observer);
}

void GrpcServicesInitializer::AddConversationStateEventObserver(
    GrpcServicesObserver<::assistant::api::OnConversationStateEventRequest>*
        observer) {
  conversation_state_event_handler_driver_->AddObserver(observer);
}

void GrpcServicesInitializer::AddDeviceStateEventObserver(
    GrpcServicesObserver<::assistant::api::OnDeviceStateEventRequest>*
        observer) {
  device_state_event_handler_driver_->AddObserver(observer);
}

void GrpcServicesInitializer::AddMediaActionFallbackEventObserver(
    GrpcServicesObserver<::assistant::api::OnMediaActionFallbackEventRequest>*
        observer) {
  media_action_fallback_event_handler_driver_->AddObserver(observer);
}

void GrpcServicesInitializer::AddSpeakerIdEnrollmentEventObserver(
    GrpcServicesObserver<::assistant::api::OnSpeakerIdEnrollmentEventRequest>*
        observer) {
  speaker_id_enrollment_event_handler_driver_->AddObserver(observer);
}

void GrpcServicesInitializer::RemoveSpeakerIdEnrollmentEventObserver(
    GrpcServicesObserver<::assistant::api::OnSpeakerIdEnrollmentEventRequest>*
        observer) {
  speaker_id_enrollment_event_handler_driver_->RemoveObserver(observer);
}

ActionService* GrpcServicesInitializer::GetActionService() {
  return action_handler_driver_.get();
}

GrpcLibassistantClient& GrpcServicesInitializer::GrpcLibassistantClient() {
  return *libassistant_client_;
}

void GrpcServicesInitializer::InitDrivers(grpc::ServerBuilder* server_builder) {
  // Inits heartbeat driver.
  heartbeat_driver_ =
      std::make_unique<HeartbeatEventHandlerDriver>(&server_builder_);
  heartbeat_event_observation_.Observe(heartbeat_driver_.get());
  service_drivers_.emplace_back(heartbeat_driver_.get());

  // Inits action service.
  action_handler_driver_ = std::make_unique<ActionService>(
      &server_builder_, libassistant_client_.get(), assistant_service_address_);
  service_drivers_.emplace_back(action_handler_driver_.get());

  // Inits other event handler drivers.
  alarm_timer_event_handler_driver_ = std::make_unique<
      EventHandlerDriver<::assistant::api::AlarmTimerEventHandlerInterface>>(
      &server_builder_, libassistant_client_.get(), assistant_service_address_);
  service_drivers_.emplace_back(alarm_timer_event_handler_driver_.get());

  assistant_display_event_handler_driver_ = std::make_unique<EventHandlerDriver<
      ::assistant::api::AssistantDisplayEventHandlerInterface>>(
      &server_builder_, libassistant_client_.get(), assistant_service_address_);
  service_drivers_.emplace_back(assistant_display_event_handler_driver_.get());

  conversation_state_event_handler_driver_ =
      std::make_unique<EventHandlerDriver<
          ::assistant::api::ConversationStateEventHandlerInterface>>(
          &server_builder_, libassistant_client_.get(),
          assistant_service_address_);
  service_drivers_.emplace_back(conversation_state_event_handler_driver_.get());

  device_state_event_handler_driver_ = std::make_unique<
      EventHandlerDriver<::assistant::api::DeviceStateEventHandlerInterface>>(
      &server_builder_, libassistant_client_.get(), assistant_service_address_);
  service_drivers_.emplace_back(device_state_event_handler_driver_.get());

  media_action_fallback_event_handler_driver_ =
      std::make_unique<EventHandlerDriver<
          ::assistant::api::MediaActionFallbackEventHandlerInterface>>(
          &server_builder_, libassistant_client_.get(),
          assistant_service_address_);
  service_drivers_.emplace_back(
      media_action_fallback_event_handler_driver_.get());

  speaker_id_enrollment_event_handler_driver_ =
      std::make_unique<EventHandlerDriver<
          ::assistant::api::SpeakerIdEnrollmentEventHandlerInterface>>(
          &server_builder_, libassistant_client_.get(),
          assistant_service_address_);
  service_drivers_.emplace_back(
      speaker_id_enrollment_event_handler_driver_.get());
}

void GrpcServicesInitializer::InitLibassistGrpcClient() {
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
      std::make_unique<ash::libassistant::GrpcLibassistantClient>(channel);
}

void GrpcServicesInitializer::StartGrpcHttpConnectionClient(
    assistant_client::HttpConnectionFactory* factory) {
  const bool is_chromeos_device = base::SysInfo::IsRunningOnChromeOS();
  http_connection_client_ = std::make_unique<GrpcHttpConnectionClient>(
      factory, GetHttpConnectionServiceAddress(is_chromeos_device));
  http_connection_client_->Start();
}

void GrpcServicesInitializer::StopGrpcHttpConnectionClient() {
  http_connection_client_.reset();
}

void GrpcServicesInitializer::InitAssistantGrpcServer() {
  auto connect_type = GetGrpcLocalConnectType(assistant_service_address_);
  // Listen on the given address with the specified credentials.
  server_builder_.AddListeningPort(
      assistant_service_address_,
      ::grpc::experimental::LocalServerCredentials(connect_type));
  RegisterServicesAndInitCQ(&server_builder_);
}

void GrpcServicesInitializer::RegisterEventHandlers() {
  alarm_timer_event_handler_driver_->StartRegistration();
  assistant_display_event_handler_driver_->StartRegistration();
  conversation_state_event_handler_driver_->StartRegistration();
  device_state_event_handler_driver_->StartRegistration();
  media_action_fallback_event_handler_driver_->StartRegistration();
  speaker_id_enrollment_event_handler_driver_->StartRegistration();
}

}  // namespace ash::libassistant
