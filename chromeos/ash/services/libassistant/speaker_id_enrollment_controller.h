// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_LIBASSISTANT_SPEAKER_ID_ENROLLMENT_CONTROLLER_H_
#define CHROMEOS_ASH_SERVICES_LIBASSISTANT_SPEAKER_ID_ENROLLMENT_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "chromeos/ash/services/libassistant/abortable_task_list.h"
#include "chromeos/ash/services/libassistant/grpc/assistant_client_observer.h"
#include "chromeos/ash/services/libassistant/public/mojom/audio_input_controller.mojom-forward.h"
#include "chromeos/ash/services/libassistant/public/mojom/speaker_id_enrollment_controller.mojom.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/delegate/event_handler_interface.pb.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace ash::libassistant {

class AssistantClient;

class SpeakerIdEnrollmentController
    : public mojom::SpeakerIdEnrollmentController,
      public AssistantClientObserver {
 public:
  explicit SpeakerIdEnrollmentController(
      mojom::AudioInputController* audio_input);
  SpeakerIdEnrollmentController(const SpeakerIdEnrollmentController&) = delete;
  SpeakerIdEnrollmentController& operator=(
      const SpeakerIdEnrollmentController&) = delete;
  ~SpeakerIdEnrollmentController() override;

  void Bind(mojo::PendingReceiver<mojom::SpeakerIdEnrollmentController>
                pending_receiver);

  // mojom::SpeakerIdEnrollmentController implementation:
  void StartSpeakerIdEnrollment(
      const std::string& user_gaia_id,
      bool skip_cloud_enrollment,
      ::mojo::PendingRemote<mojom::SpeakerIdEnrollmentClient> client) override;
  void StopSpeakerIdEnrollment() override;
  void GetSpeakerIdEnrollmentStatus(
      const std::string& user_gaia_id,
      GetSpeakerIdEnrollmentStatusCallback callback) override;

  // AssistantClientObserver implementation:
  void OnAssistantClientStarted(AssistantClient* assistant_client) override;
  void OnDestroyingAssistantClient(AssistantClient* assistant_client) override;

  bool IsSpeakerIdEnrollmentInProgressForTesting() const {
    return !!active_enrollment_session_;
  }

  void OnGrpcMessageForTesting(
      const ::assistant::api::OnSpeakerIdEnrollmentEventRequest& request);

  void SendGetStatusResponseForTesting(bool user_model_exists);

 private:
  class EnrollmentSession;
  class GetStatusWaiter;

  mojo::Receiver<mojom::SpeakerIdEnrollmentController> receiver_{this};
  const raw_ptr<mojom::AudioInputController> audio_input_;

  std::unique_ptr<EnrollmentSession> active_enrollment_session_;
  // Contains all pending callbacks for GetSpeakerIdEnrollmentStatus requests.
  AbortableTaskList pending_response_waiters_;

  raw_ptr<AssistantClient> assistant_client_ = nullptr;
};

}  // namespace ash::libassistant

#endif  // CHROMEOS_ASH_SERVICES_LIBASSISTANT_SPEAKER_ID_ENROLLMENT_CONTROLLER_H_
