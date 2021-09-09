// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_LIBASSISTANT_GRPC_ASSISTANT_CLIENT_H_
#define CHROMEOS_SERVICES_LIBASSISTANT_GRPC_ASSISTANT_CLIENT_H_

#include <memory>

#include "base/callback_forward.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/device_state_event.pb.h"
#include "chromeos/services/libassistant/grpc/external_services/grpc_services_observer.h"

namespace assistant {
namespace api {
class CancelSpeakerIdEnrollmentRequest;
class GetSpeakerIdEnrollmentInfoRequest;
class Interaction;
class OnAssistantDisplayEventRequest;
class OnDeviceStateEventRequest;
class OnDisplayRequestRequest;
class OnSpeakerIdEnrollmentEventRequest;
class StartSpeakerIdEnrollmentRequest;
class VoicelessOptions;

namespace events {
class SpeakerIdEnrollmentEvent;
}  // namespace events
}  // namespace api
}  // namespace assistant

namespace assistant_client {
class AssistantManager;
class AssistantManagerInternal;
class ChromeOSApiDelegate;
}  // namespace assistant_client

namespace chromeos {
namespace libassistant {

// The Libassistant customer class which establishes a gRPC connection to
// Libassistant and provides an interface for interacting with gRPC Libassistant
// client. It helps to build request/response proto messages internally for each
// specific method below and call the appropriate gRPC (IPC) client method.
class AssistantClient {
 public:
  // Speaker Id Enrollment:
  using CancelSpeakerIdEnrollmentRequest =
      ::assistant::api::CancelSpeakerIdEnrollmentRequest;
  using GetSpeakerIdEnrollmentInfoRequest =
      ::assistant::api::GetSpeakerIdEnrollmentInfoRequest;
  using StartSpeakerIdEnrollmentRequest =
      ::assistant::api::StartSpeakerIdEnrollmentRequest;
  using SpeakerIdEnrollmentEvent =
      ::assistant::api::events::SpeakerIdEnrollmentEvent;
  using OnSpeakerIdEnrollmentEventRequest =
      ::assistant::api::OnSpeakerIdEnrollmentEventRequest;

  // Display:
  using OnAssistantDisplayEventRequest =
      ::assistant::api::OnAssistantDisplayEventRequest;
  using OnDisplayRequestRequest = ::assistant::api::OnDisplayRequestRequest;

  // Media:
  using MediaStatus = ::assistant::api::events::DeviceState::MediaStatus;
  using OnDeviceStateEventRequest = ::assistant::api::OnDeviceStateEventRequest;

  AssistantClient(
      std::unique_ptr<assistant_client::AssistantManager> assistant_manager,
      assistant_client::AssistantManagerInternal* assistant_manager_internal);
  AssistantClient(const AssistantClient&) = delete;
  AssistantClient& operator=(const AssistantClient&) = delete;
  virtual ~AssistantClient();

  virtual void StartServices() = 0;

  virtual void SetChromeOSApiDelegate(
      assistant_client::ChromeOSApiDelegate* delegate) = 0;

  // 1. Start a gRPC server which hosts the services that Libassistant depends
  // on (maybe called by Libassistant) or receive events from Libassistant.
  // 2. Register this client as a customer of Libassistant by sending
  // RegisterCustomerRequest to Libassistant periodically. All supported
  // services should be registered first before calling this method.
  virtual bool StartGrpcServices() = 0;

  virtual void AddExperimentIds(const std::vector<std::string>& exp_ids) = 0;

  virtual void SendVoicelessInteraction(
      const ::assistant::api::Interaction& interaction,
      const std::string& description,
      const ::assistant::api::VoicelessOptions& options,
      base::OnceCallback<void(bool)> on_done) = 0;

  // Speaker Id Enrollment methods.
  virtual void AddSpeakerIdEnrollmentEventObserver(
      GrpcServicesObserver<OnSpeakerIdEnrollmentEventRequest>* observer) = 0;
  virtual void RemoveSpeakerIdEnrollmentEventObserver(
      GrpcServicesObserver<OnSpeakerIdEnrollmentEventRequest>* observer) = 0;
  virtual void StartSpeakerIdEnrollment(
      const StartSpeakerIdEnrollmentRequest& request) = 0;
  virtual void CancelSpeakerIdEnrollment(
      const CancelSpeakerIdEnrollmentRequest& request) = 0;
  virtual void GetSpeakerIdEnrollmentInfo(
      const GetSpeakerIdEnrollmentInfoRequest& request,
      base::OnceCallback<void(bool user_model_exists)> on_done) = 0;

  virtual void ResetAllDataAndShutdown() = 0;

  // Display methods.
  virtual void OnDisplayRequest(const OnDisplayRequestRequest& request) = 0;
  virtual void AddDisplayEventObserver(
      GrpcServicesObserver<OnAssistantDisplayEventRequest>* observer) = 0;

  // Media methods.
  virtual void ResumeCurrentStream() = 0;
  virtual void PauseCurrentStream() = 0;
  // Sets the current media status of media playing outside of libassistant.
  // Setting external state will stop any internally playing media.
  virtual void SetExternalPlaybackState(const MediaStatus& status_proto) = 0;

  virtual void AddDeviceStateEventObserver(
      GrpcServicesObserver<OnDeviceStateEventRequest>* observer) = 0;

  // Will not return nullptr.
  assistant_client::AssistantManager* assistant_manager() {
    return assistant_manager_.get();
  }
  // Will not return nullptr.
  assistant_client::AssistantManagerInternal* assistant_manager_internal() {
    return assistant_manager_internal_;
  }

  // Creates an instance of AssistantClient, the returned instance could be
  // LibAssistant V1 or V2 based depending on the current flags. It should be
  // transparent to the caller.
  static std::unique_ptr<AssistantClient> Create(
      std::unique_ptr<assistant_client::AssistantManager> assistant_manager,
      assistant_client::AssistantManagerInternal* assistant_manager_internal);

 private:
  std::unique_ptr<assistant_client::AssistantManager> assistant_manager_;
  assistant_client::AssistantManagerInternal* assistant_manager_internal_ =
      nullptr;
};

}  // namespace libassistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_LIBASSISTANT_GRPC_ASSISTANT_CLIENT_H_
