// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_LIBASSISTANT_GRPC_EXTERNAL_SERVICES_GRPC_SERVICES_INITIALIZER_H_
#define CHROMEOS_ASH_SERVICES_LIBASSISTANT_GRPC_EXTERNAL_SERVICES_GRPC_SERVICES_INITIALIZER_H_

#include <memory>
#include <string>

#include "base/scoped_observation.h"
#include "chromeos/ash/services/libassistant/grpc/external_services/customer_registration_client.h"
#include "chromeos/ash/services/libassistant/grpc/external_services/event_handler_driver.h"
#include "chromeos/ash/services/libassistant/grpc/external_services/grpc_services_observer.h"
#include "chromeos/ash/services/libassistant/grpc/external_services/heartbeat_event_handler_driver.h"
#include "chromeos/ash/services/libassistant/grpc/grpc_client_thread.h"
#include "chromeos/ash/services/libassistant/grpc/services_initializer_base.h"
#include "chromeos/ash/services/libassistant/grpc/services_status_provider.h"
#include "third_party/grpc/src/include/grpcpp/server_builder.h"

namespace assistant {
namespace api {
class AlarmTimerEventHandlerInterface;
class AssistantDisplayEventHandlerInterface;
class ConversationStateEventHandlerInterface;
class DeviceStateEventHandlerInterface;
class MediaActionFallbackEventHandlerInterface;
class SpeakerIdEnrollmentEventHandlerInterface;
}  // namespace api
}  // namespace assistant

namespace assistant_client {
class HttpConnectionFactory;
}  // namespace assistant_client

namespace ash::libassistant {

class ActionService;
class GrpcHttpConnectionClient;
class GrpcLibassistantClient;

// Component responsible for:
// 1. Set up a gRPC client by establishing a new channel with Libassistant
// server.
// 2. Start and manage Libassistant V2 event observer gRPC services.
class GrpcServicesInitializer : public ServicesInitializerBase {
 public:
  GrpcServicesInitializer(const std::string& libassistant_service_address,
                          const std::string& assistant_service_address);
  GrpcServicesInitializer(const GrpcServicesInitializer&) = delete;
  GrpcServicesInitializer& operator=(const GrpcServicesInitializer&) = delete;
  ~GrpcServicesInitializer() override;

  // Start assistant gRPC server. All supported services must be registered
  // before this method is called. Client functionality is not impacted by this
  // call. Returns false if the attempt to start a gRPC server failed.
  bool Start();

  void StartGrpcHttpConnectionClient(assistant_client::HttpConnectionFactory*);
  void StopGrpcHttpConnectionClient();

  // Add observer for each handler driver.
  void AddAlarmTimerEventObserver(
      GrpcServicesObserver<::assistant::api::OnAlarmTimerEventRequest>*
          observer);
  void AddAssistantDisplayEventObserver(
      GrpcServicesObserver<::assistant::api::OnAssistantDisplayEventRequest>*
          observer);
  void AddConversationStateEventObserver(
      GrpcServicesObserver<::assistant::api::OnConversationStateEventRequest>*
          observer);
  void AddDeviceStateEventObserver(
      GrpcServicesObserver<::assistant::api::OnDeviceStateEventRequest>*
          observer);
  void AddMediaActionFallbackEventObserver(
      GrpcServicesObserver<::assistant::api::OnMediaActionFallbackEventRequest>*
          observer);
  void AddSpeakerIdEnrollmentEventObserver(
      GrpcServicesObserver<::assistant::api::OnSpeakerIdEnrollmentEventRequest>*
          observer);
  // SpeakerIdEnrollmentEvent requires a remove function because its lifecycle.
  void RemoveSpeakerIdEnrollmentEventObserver(
      GrpcServicesObserver<::assistant::api::OnSpeakerIdEnrollmentEventRequest>*
          observer);

  ActionService* GetActionService();

  // Expose a reference to |GrpcLibassistantClient|.
  GrpcLibassistantClient& GrpcLibassistantClient();

  ServicesStatusProvider& GetServicesStatusProvider() {
    return services_status_provider_;
  }

 private:
  // ServicesInitializerBase overrides:
  void InitDrivers(grpc::ServerBuilder* server_builder) override;

  // 1. Creates a channel object to obtain a handle to libassistant gRPC server
  // services.
  // 2. Creates a gRPC client using that channel and through which we can invoke
  // service methods implemented in the server.
  void InitLibassistGrpcClient();

  // 1. Adds a listening port where server can receive traffic (via
  // AddListeningPort).
  // 2. Adds a completion queue to handle async operations (via
  // AddCompletionQueue).
  // 3. Registers all supported services (via RegisterService).
  // This should be called before Start().
  void InitAssistantGrpcServer();

  void RegisterEventHandlers();

  // Address of assistant gRPC server.
  const std::string assistant_service_address_;
  // Address of Libassistant gRPC server.
  const std::string libassistant_service_address_;

  grpc::ServerBuilder server_builder_;
  std::unique_ptr<grpc::Server> assistant_grpc_server_;

  // The entrypoint through which we can query Libassistant V2 APIs.
  std::unique_ptr<ash::libassistant::GrpcLibassistantClient>
      libassistant_client_;

  std::unique_ptr<CustomerRegistrationClient> customer_registration_client_;

  std::unique_ptr<HeartbeatEventHandlerDriver> heartbeat_driver_;

  std::unique_ptr<ActionService> action_handler_driver_;

  std::unique_ptr<
      EventHandlerDriver<::assistant::api::AlarmTimerEventHandlerInterface>>
      alarm_timer_event_handler_driver_;

  std::unique_ptr<EventHandlerDriver<
      ::assistant::api::AssistantDisplayEventHandlerInterface>>
      assistant_display_event_handler_driver_;

  std::unique_ptr<EventHandlerDriver<
      ::assistant::api::ConversationStateEventHandlerInterface>>
      conversation_state_event_handler_driver_;

  std::unique_ptr<
      EventHandlerDriver<::assistant::api::DeviceStateEventHandlerInterface>>
      device_state_event_handler_driver_;

  std::unique_ptr<EventHandlerDriver<
      ::assistant::api::MediaActionFallbackEventHandlerInterface>>
      media_action_fallback_event_handler_driver_;

  std::unique_ptr<EventHandlerDriver<
      ::assistant::api::SpeakerIdEnrollmentEventHandlerInterface>>
      speaker_id_enrollment_event_handler_driver_;

  std::unique_ptr<GrpcHttpConnectionClient> http_connection_client_;

  // `heartbeat_event_observation_` observes `heartbeat_driver_`, and needs to
  // be destroyed before `heartbeat_driver_`.
  ServicesStatusProvider services_status_provider_;
  base::ScopedObservation<
      HeartbeatEventHandlerDriver,
      GrpcServicesObserver<::assistant::api::OnHeartbeatEventRequest>>
      heartbeat_event_observation_{&services_status_provider_};
};

}  // namespace ash::libassistant

#endif  // CHROMEOS_ASH_SERVICES_LIBASSISTANT_GRPC_EXTERNAL_SERVICES_GRPC_SERVICES_INITIALIZER_H_
