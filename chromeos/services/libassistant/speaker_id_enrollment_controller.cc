// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/libassistant/speaker_id_enrollment_controller.h"

#include "chromeos/services/libassistant/grpc/assistant_client.h"
#include "chromeos/services/libassistant/public/mojom/audio_input_controller.mojom.h"
#include "libassistant/shared/internal_api/assistant_manager_internal.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {
namespace libassistant {

using SpeakerIdEnrollmentState =
    ::assistant_client::SpeakerIdEnrollmentUpdate::State;

////////////////////////////////////////////////////////////////////////////////
// GetStatusWaiter
////////////////////////////////////////////////////////////////////////////////

// Helper class that will wait for the result of the
// |GetSpeakerIdEnrollmentStatus| call, and send it to the callback.
class SpeakerIdEnrollmentController::GetStatusWaiter : public AbortableTask {
 public:
  using Callback =
      SpeakerIdEnrollmentController::GetSpeakerIdEnrollmentStatusCallback;

  GetStatusWaiter() : task_runner_(base::SequencedTaskRunnerHandle::Get()) {}
  ~GetStatusWaiter() override { DCHECK(!callback_); }
  GetStatusWaiter(const GetStatusWaiter&) = delete;
  GetStatusWaiter& operator=(const GetStatusWaiter&) = delete;

  void Start(
      assistant_client::AssistantManagerInternal* assistant_manager_internal,
      const std::string& user_gaia_id,
      Callback callback) {
    callback_ = std::move(callback);

    VLOG(1) << "Assistant: Retrieving speaker enrollment status";

    if (!assistant_manager_internal) {
      SendErrorResponse();
      return;
    }

    assistant_manager_internal->GetSpeakerIdEnrollmentStatus(
        user_gaia_id,
        [this](const assistant_client::SpeakerIdEnrollmentStatus& status) {
          task_runner_->PostTask(FROM_HERE,
                                 base::BindOnce(&GetStatusWaiter::SendResponse,
                                                weak_ptr_factory_.GetWeakPtr(),
                                                status.user_model_exists));
        });
  }

  // AbortableTask implementation:
  bool IsFinished() override { return callback_.is_null(); }
  void Abort() override { SendErrorResponse(); }

 private:
  void SendErrorResponse() { SendResponse(false); }

  void SendResponse(bool user_model_exists) {
    VLOG(1) << "Assistant: Is user already enrolled? " << user_model_exists;

    std::move(callback_).Run(
        mojom::SpeakerIdEnrollmentStatus::New(user_model_exists));
  }

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  Callback callback_;
  base::WeakPtrFactory<GetStatusWaiter> weak_ptr_factory_{this};
};

////////////////////////////////////////////////////////////////////////////////
// EnrollmentSession
////////////////////////////////////////////////////////////////////////////////

// A single enrollment session, created when speaker id enrollment is started,
// and destroyed when it is done or cancelled.
class SpeakerIdEnrollmentController::EnrollmentSession {
 public:
  EnrollmentSession(
      ::mojo::PendingRemote<mojom::SpeakerIdEnrollmentClient> client,
      assistant_client::AssistantManagerInternal* assistant_manager_internal)
      : client_(std::move(client)),
        assistant_manager_internal_(assistant_manager_internal),
        mojom_task_runner_(base::SequencedTaskRunnerHandle::Get()) {
    DCHECK(assistant_manager_internal_);
  }
  EnrollmentSession(const EnrollmentSession&) = delete;
  EnrollmentSession& operator=(const EnrollmentSession&) = delete;
  ~EnrollmentSession() { Stop(); }

  void Start(const std::string& user_gaia_id, bool skip_cloud_enrollment) {
    VLOG(1) << "Assistant: Starting speaker id enrollment";

    assistant_client::SpeakerIdEnrollmentConfig client_config;
    client_config.user_id = user_gaia_id;
    client_config.skip_cloud_enrollment = skip_cloud_enrollment;

    assistant_manager_internal_->StartSpeakerIdEnrollment(
        client_config,
        [weak_ptr = weak_factory_.GetWeakPtr(),
         task_runner = mojom_task_runner_](
            const assistant_client::SpeakerIdEnrollmentUpdate& update) {
          task_runner->PostTask(
              FROM_HERE,
              base::BindOnce(&EnrollmentSession::OnSpeakerIdEnrollmentUpdate,
                             weak_ptr, update));
        });
  }

  void Stop() {
    if (done_)
      return;

    VLOG(1) << "Assistant: Stopping speaker id enrollment";
    assistant_manager_internal_->StopSpeakerIdEnrollment([]() {});
  }

 private:
  void OnSpeakerIdEnrollmentUpdate(
      const assistant_client::SpeakerIdEnrollmentUpdate& update) {
    switch (update.state) {
      case SpeakerIdEnrollmentState::LISTEN:
        VLOG(1) << "Assistant: Speaker id enrollment is listening";
        client_->OnListeningHotword();
        break;
      case SpeakerIdEnrollmentState::PROCESS:
        VLOG(1) << "Assistant: Speaker id enrollment is processing";
        client_->OnProcessingHotword();
        break;
      case SpeakerIdEnrollmentState::DONE:
        VLOG(1) << "Assistant: Speaker id enrollment is done";
        client_->OnSpeakerIdEnrollmentDone();
        done_ = true;
        break;
      case SpeakerIdEnrollmentState::FAILURE:
        VLOG(1) << "Assistant: Speaker id enrollment is done (with failure)";
        client_->OnSpeakerIdEnrollmentFailure();
        done_ = true;
        break;
      case SpeakerIdEnrollmentState::INIT:
      case SpeakerIdEnrollmentState::CHECK:
      case SpeakerIdEnrollmentState::UPLOAD:
      case SpeakerIdEnrollmentState::FETCH:
        break;
    }
  }

  ::mojo::Remote<mojom::SpeakerIdEnrollmentClient> client_;
  assistant_client::AssistantManagerInternal* const assistant_manager_internal_;
  scoped_refptr<base::SequencedTaskRunner> mojom_task_runner_;
  bool done_ = false;
  base::WeakPtrFactory<EnrollmentSession> weak_factory_{this};
};

////////////////////////////////////////////////////////////////////////////////
// SpeakerIdEnrollmentController
////////////////////////////////////////////////////////////////////////////////

SpeakerIdEnrollmentController::SpeakerIdEnrollmentController(
    mojom::AudioInputController* audio_input)
    : audio_input_(audio_input) {}
SpeakerIdEnrollmentController::~SpeakerIdEnrollmentController() = default;

void SpeakerIdEnrollmentController::Bind(
    mojo::PendingReceiver<mojom::SpeakerIdEnrollmentController>
        pending_receiver) {
  receiver_.Bind(std::move(pending_receiver));
}

void SpeakerIdEnrollmentController::StartSpeakerIdEnrollment(
    const std::string& user_gaia_id,
    bool skip_cloud_enrollment,
    ::mojo::PendingRemote<mojom::SpeakerIdEnrollmentClient> client) {
  if (!assistant_manager_internal_)
    return;

  // Force mic state to open, otherwise the training might not open the
  // microphone. See b/139329513.
  audio_input_->SetMicOpen(true);

  // If there is an ongoing enrollment session, abort it first.
  if (active_enrollment_session_)
    active_enrollment_session_->Stop();

  active_enrollment_session_ = std::make_unique<EnrollmentSession>(
      std::move(client), assistant_manager_internal_);
  active_enrollment_session_->Start(user_gaia_id, skip_cloud_enrollment);
}

void SpeakerIdEnrollmentController::StopSpeakerIdEnrollment() {
  if (!assistant_manager_internal_)
    return;

  if (!active_enrollment_session_)
    return;

  audio_input_->SetMicOpen(false);

  active_enrollment_session_->Stop();
  active_enrollment_session_ = nullptr;
}

void SpeakerIdEnrollmentController::GetSpeakerIdEnrollmentStatus(
    const std::string& user_gaia_id,
    GetSpeakerIdEnrollmentStatusCallback callback) {
  auto* waiter =
      pending_response_waiters_.Add(std::make_unique<GetStatusWaiter>());
  waiter->Start(assistant_manager_internal_, user_gaia_id, std::move(callback));
}

void SpeakerIdEnrollmentController::OnAssistantClientStarted(
    AssistantClient* assistant_client) {
  assistant_manager_internal_ = assistant_client->assistant_manager_internal();
}

void SpeakerIdEnrollmentController::OnDestroyingAssistantClient(
    AssistantClient* assistant_client) {
  active_enrollment_session_ = nullptr;
  assistant_manager_internal_ = nullptr;
  pending_response_waiters_.AbortAll();
}

}  // namespace libassistant
}  // namespace chromeos
