// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_LIBASSISTANT_GRPC_ASSISTANT_CLIENT_H_
#define CHROMEOS_ASH_SERVICES_LIBASSISTANT_GRPC_ASSISTANT_CLIENT_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation_traits.h"
#include "chromeos/ash/services/libassistant/grpc/external_services/grpc_services_observer.h"
#include "chromeos/ash/services/libassistant/grpc/services_status_observer.h"
#include "chromeos/ash/services/libassistant/public/cpp/assistant_timer.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/device_state_event.pb.h"

namespace assistant {
namespace api {
class CancelSpeakerIdEnrollmentRequest;
class GetSpeakerIdEnrollmentInfoRequest;
class GetSpeakerIdEnrollmentInfoResponse;
class GetAssistantSettingsResponse;
class Interaction;
class OnAlarmTimerEventRequest;
class OnAssistantDisplayEventRequest;
class OnConversationStateEventRequest;
class OnDeviceStateEventRequest;
class OnDisplayRequestRequest;
class OnMediaActionFallbackEventRequest;
class OnSpeakerIdEnrollmentEventRequest;
class StartSpeakerIdEnrollmentRequest;
class UpdateAssistantSettingsResponse;
class VoicelessOptions;

namespace events {
class SpeakerIdEnrollmentEvent;
}  // namespace events
}  // namespace api
}  // namespace assistant

namespace assistant {
namespace ui {
class SettingsUiSelector;
class SettingsUiUpdate;
}  // namespace ui
}  // namespace assistant

namespace assistant_client {
class ActionModule;
class AssistantManager;
class HttpConnectionFactory;
}  // namespace assistant_client

namespace ash::libassistant {

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
  using GetSpeakerIdEnrollmentInfoResponse =
      ::assistant::api::GetSpeakerIdEnrollmentInfoResponse;
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
  using OnMediaActionFallbackEventRequest =
      ::assistant::api::OnMediaActionFallbackEventRequest;

  // Conversation:
  using OnConversationStateEventRequest =
      ::assistant::api::OnConversationStateEventRequest;

  // Each authentication token exists of a [gaia_id, access_token] tuple.
  using AuthTokens = std::vector<std::pair<std::string, std::string>>;

  AssistantClient(
      std::unique_ptr<assistant_client::AssistantManager> assistant_manager);
  AssistantClient(const AssistantClient&) = delete;
  AssistantClient& operator=(const AssistantClient&) = delete;
  virtual ~AssistantClient();

  // Starts Libassistant services. |services_status_observer| will get notified
  // on new status change.
  virtual void StartServices(
      ServicesStatusObserver* services_status_observer) = 0;

  virtual void StartGrpcHttpConnectionClient(
      assistant_client::HttpConnectionFactory*) = 0;

  // 1. Start a gRPC server which hosts the services that Libassistant depends
  // on (maybe called by Libassistant) or receive events from Libassistant.
  // 2. Register this client as a customer of Libassistant by sending
  // RegisterCustomerRequest to Libassistant periodically. All supported
  // services should be registered first before calling this method.
  virtual bool StartGrpcServices() = 0;

  virtual void AddExperimentIds(const std::vector<std::string>& exp_ids) = 0;

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
  virtual void SendDisplayRequest(const OnDisplayRequestRequest& request) = 0;
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
  virtual void AddMediaActionFallbackEventObserver(
      GrpcServicesObserver<OnMediaActionFallbackEventRequest>* observer) = 0;

  // Conversation methods.
  virtual void SendVoicelessInteraction(
      const ::assistant::api::Interaction& interaction,
      const std::string& description,
      const ::assistant::api::VoicelessOptions& options,
      base::OnceCallback<void(bool)> on_done) = 0;
  virtual void RegisterActionModule(
      assistant_client::ActionModule* action_module) = 0;
  virtual void StartVoiceInteraction() = 0;
  virtual void StopAssistantInteraction(bool cancel_conversation) = 0;
  virtual void AddConversationStateEventObserver(
      GrpcServicesObserver<OnConversationStateEventRequest>* observer) = 0;

  // Settings-related functionality during bootup:
  virtual void SetAuthenticationInfo(const AuthTokens& tokens) = 0;
  virtual void SetInternalOptions(const std::string& locale,
                                  bool spoken_feedback_enabled) = 0;

  // Settings-related functionality after fully started:
  virtual void UpdateAssistantSettings(
      const ::assistant::ui::SettingsUiUpdate& settings,
      const std::string& user_id,
      base::OnceCallback<
          void(const ::assistant::api::UpdateAssistantSettingsResponse&)>
          on_done) = 0;
  virtual void GetAssistantSettings(
      const ::assistant::ui::SettingsUiSelector& selector,
      const std::string& user_id,
      base::OnceCallback<void(
          const ::assistant::api::GetAssistantSettingsResponse&)> on_done) = 0;
  virtual void SetLocaleOverride(const std::string& locale) = 0;
  virtual std::string GetDeviceId() = 0;

  // Audio-related functionality:
  // Enables or disables audio input pipeline.
  virtual void EnableListening(bool listening_enabled) = 0;

  // Alarm/timer-related functionality:
  // Adds extra time to the timer.
  virtual void AddTimeToTimer(const std::string& id,
                              const base::TimeDelta& duration) = 0;
  // Pauses the specified timer. This will be a no-op if the |timer_id| is
  // invalid.
  virtual void PauseTimer(const std::string& timer_id) = 0;
  // Removes and cancels the timer.
  virtual void RemoveTimer(const std::string& timer_id) = 0;
  // Resumes the specified timer (expected to be in paused state).
  virtual void ResumeTimer(const std::string& timer_id) = 0;
  // Returns the list of all currently scheduled, ringing or paused timers in
  // the callback.
  virtual void GetTimers(
      base::OnceCallback<void(const std::vector<assistant::AssistantTimer>&)>
          on_done) = 0;

  // Registers |observer| to get notified on any alarm/timer status change.
  virtual void AddAlarmTimerEventObserver(
      GrpcServicesObserver<::assistant::api::OnAlarmTimerEventRequest>*
          observer) = 0;

  // Will not return nullptr.
  assistant_client::AssistantManager* assistant_manager() {
    return assistant_manager_.get();
  }

  // Creates an instance of AssistantClient.
  static std::unique_ptr<AssistantClient> Create(
      std::unique_ptr<assistant_client::AssistantManager> assistant_manager);

 protected:
  void ResetAssistantManager();

 private:
  std::unique_ptr<assistant_client::AssistantManager> assistant_manager_;
};

}  // namespace ash::libassistant

namespace base {

template <>
struct ScopedObservationTraits<
    ash::libassistant::AssistantClient,
    ash::libassistant::GrpcServicesObserver<
        ::assistant::api::OnSpeakerIdEnrollmentEventRequest>> {
  static void AddObserver(
      ash::libassistant::AssistantClient* source,
      ash::libassistant::GrpcServicesObserver<
          ::assistant::api::OnSpeakerIdEnrollmentEventRequest>* observer) {
    source->AddSpeakerIdEnrollmentEventObserver(observer);
  }
  static void RemoveObserver(
      ash::libassistant::AssistantClient* source,
      ash::libassistant::GrpcServicesObserver<
          ::assistant::api::OnSpeakerIdEnrollmentEventRequest>* observer) {
    source->RemoveSpeakerIdEnrollmentEventObserver(observer);
  }
};

}  // namespace base

#endif  // CHROMEOS_ASH_SERVICES_LIBASSISTANT_GRPC_ASSISTANT_CLIENT_H_
