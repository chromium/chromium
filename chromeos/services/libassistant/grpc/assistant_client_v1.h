// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_LIBASSISTANT_GRPC_ASSISTANT_CLIENT_V1_H_
#define CHROMEOS_SERVICES_LIBASSISTANT_GRPC_ASSISTANT_CLIENT_V1_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chromeos/services/libassistant/grpc/assistant_client.h"
#include "libassistant/shared/internal_api/assistant_manager_internal.h"

namespace chromeos {
namespace libassistant {

class AssistantClientV1 : public AssistantClient {
 public:
  AssistantClientV1(
      std::unique_ptr<assistant_client::AssistantManager> assistant_manager,
      assistant_client::AssistantManagerInternal* assistant_manager_internal);
  ~AssistantClientV1() override;

  // chromeos::libassistant::AssistantClient:
  void StartServices() override;
  void SetChromeOSApiDelegate(
      assistant_client::ChromeOSApiDelegate* delegate) override;
  bool StartGrpcServices() override;
  void AddExperimentIds(const std::vector<std::string>& exp_ids) override;
  void SendVoicelessInteraction(
      const ::assistant::api::Interaction& interaction,
      const std::string& description,
      const ::assistant::api::VoicelessOptions& options,
      base::OnceCallback<void(bool)> on_done) override;
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
  void OnDisplayRequest(const OnDisplayRequestRequest& request) override;
  void AddDisplayEventObserver(
      GrpcServicesObserver<OnAssistantDisplayEventRequest>* observer) override;
  void ResumeCurrentStream() override;
  void PauseCurrentStream() override;
  void SetExternalPlaybackState(const MediaStatus& status_proto) override;
  void AddDeviceStateEventObserver(
      GrpcServicesObserver<OnDeviceStateEventRequest>* observer) override;

 private:
  class DeviceStateListener;
  class DisplayConnectionImpl;
  class MediaManagerListener;

  void AddMediaManagerListener();

  void NofifyDeviceStateEvent(const OnDeviceStateEventRequest& request);

  void OnSpeakerIdEnrollmentUpdate(
      const assistant_client::SpeakerIdEnrollmentUpdate& update);

  std::unique_ptr<DeviceStateListener> device_state_listener_;
  std::unique_ptr<DisplayConnectionImpl> display_connection_;
  std::unique_ptr<MediaManagerListener> media_manager_listener_;

  base::ObserverList<GrpcServicesObserver<OnSpeakerIdEnrollmentEventRequest>>
      speaker_event_observer_list_;

  base::ObserverList<GrpcServicesObserver<OnDeviceStateEventRequest>>
      device_state_event_observer_list_;

  base::WeakPtrFactory<AssistantClientV1> weak_factory_{this};
};

}  // namespace libassistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_LIBASSISTANT_GRPC_ASSISTANT_CLIENT_V1_H_
