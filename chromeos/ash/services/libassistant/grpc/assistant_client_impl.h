// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_LIBASSISTANT_GRPC_ASSISTANT_CLIENT_IMPL_H_
#define CHROMEOS_ASH_SERVICES_LIBASSISTANT_GRPC_ASSISTANT_CLIENT_IMPL_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "chromeos/ash/services/libassistant/grpc/assistant_client.h"
#include "chromeos/ash/services/libassistant/grpc/external_services/grpc_services_initializer.h"
#include "chromeos/ash/services/libassistant/grpc/services_status_provider.h"

namespace ash::libassistant {

class GrpcLibassistantClient;
class ServicesStatusObserver;

class AssistantClientImpl : public AssistantClient {
 public:
  AssistantClientImpl(
      std::unique_ptr<assistant_client::AssistantManager> assistant_manager,
      const std::string& libassistant_service_address,
      const std::string& assistant_service_address);

  ~AssistantClientImpl() override;

  // AssistantClient:
  void StartServices(ServicesStatusObserver* services_status_observer) override;
  bool StartGrpcServices() override;
  void StartGrpcHttpConnectionClient(
      assistant_client::HttpConnectionFactory*) override;
  void AddExperimentIds(const std::vector<std::string>& exp_ids) override;
  void AddSpeakerIdEnrollmentEventObserver(
      GrpcServicesObserver<OnSpeakerIdEnrollmentEventRequest>* observer)
      override;
  void RemoveSpeakerIdEnrollmentEventObserver(
      GrpcServicesObserver<OnSpeakerIdEnrollmentEventRequest>* observer)
      override;
  void StartSpeakerIdEnrollment(
      const StartSpeakerIdEnrollmentRequest& request) override;
  void CancelSpeakerIdEnrollment(
      const CancelSpeakerIdEnrollmentRequest& request) override;
  void GetSpeakerIdEnrollmentInfo(
      const GetSpeakerIdEnrollmentInfoRequest& request,
      base::OnceCallback<void(bool user_model_exists)> on_done) override;
  void ResetAllDataAndShutdown() override;
  void SendDisplayRequest(const OnDisplayRequestRequest& request) override;
  void AddDisplayEventObserver(
      GrpcServicesObserver<OnAssistantDisplayEventRequest>* observer) override;
  void ResumeCurrentStream() override;
  void PauseCurrentStream() override;
  void SetExternalPlaybackState(const MediaStatus& status_proto) override;
  void AddDeviceStateEventObserver(
      GrpcServicesObserver<OnDeviceStateEventRequest>* observer) override;
  void AddMediaActionFallbackEventObserver(
      GrpcServicesObserver<OnMediaActionFallbackEventRequest>* observer)
      override;
  void SendVoicelessInteraction(
      const ::assistant::api::Interaction& interaction,
      const std::string& description,
      const ::assistant::api::VoicelessOptions& options,
      base::OnceCallback<void(bool)> on_done) override;
  void RegisterActionModule(
      assistant_client::ActionModule* action_module) override;
  void StartVoiceInteraction() override;
  void StopAssistantInteraction(bool cancel_conversation) override;
  void AddConversationStateEventObserver(
      GrpcServicesObserver<OnConversationStateEventRequest>* observer) override;

  // Settings-related setters:
  void SetAuthenticationInfo(const AuthTokens& tokens) override;
  void SetInternalOptions(const std::string& locale,
                          bool spoken_feedback_enabled) override;
  void UpdateAssistantSettings(
      const ::assistant::ui::SettingsUiUpdate& update,
      const std::string& user_id,
      base::OnceCallback<void(
          const ::assistant::api::UpdateAssistantSettingsResponse&)> on_done)
      override;
  void GetAssistantSettings(
      const ::assistant::ui::SettingsUiSelector& selector,
      const std::string& user_id,
      base::OnceCallback<
          void(const ::assistant::api::GetAssistantSettingsResponse&)> on_done)
      override;
  void SetLocaleOverride(const std::string& locale) override;
  std::string GetDeviceId() override;

  // Audio-related functionality:
  void EnableListening(bool listening_enabled) override;

  // Timer related:
  void AddTimeToTimer(const std::string& id,
                      const base::TimeDelta& duration) override;
  void PauseTimer(const std::string& timer_id) override;
  void RemoveTimer(const std::string& timer_id) override;
  void ResumeTimer(const std::string& timer_id) override;
  void GetTimers(
      base::OnceCallback<void(const std::vector<assistant::AssistantTimer>&)>
          on_done) override;
  void AddAlarmTimerEventObserver(
      GrpcServicesObserver<::assistant::api::OnAlarmTimerEventRequest>*
          observer) override;

 private:
  GrpcServicesInitializer grpc_services_;

  // Entry point for Libassistant V2 APIs, through which V2 methods can be
  // invoked. Created and owned by |GrpcServicesInitializer|.
  const raw_ref<GrpcLibassistantClient> libassistant_client_;

  // Invoked when all LibAssistant services are ready to query.
  base::OnceClosure services_ready_callback_;
};

}  // namespace ash::libassistant

#endif  // CHROMEOS_ASH_SERVICES_LIBASSISTANT_GRPC_ASSISTANT_CLIENT_IMPL_H_
