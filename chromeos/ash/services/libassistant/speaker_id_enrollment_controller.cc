// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/libassistant/speaker_id_enrollment_controller.h"

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chromeos/ash/services/libassistant/grpc/assistant_client.h"
#include "chromeos/ash/services/libassistant/grpc/external_services/grpc_services_observer.h"
#include "chromeos/ash/services/libassistant/public/mojom/audio_input_controller.mojom.h"
#include "chromeos/assistant/internal/libassistant/shared_headers.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/delegate/event_handler_interface.pb.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/speaker_id_enrollment_event.pb.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/speaker_id_enrollment_interface.pb.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash::libassistant {

using ::assistant::api::OnSpeakerIdEnrollmentEventRequest;

////////////////////////////////////////////////////////////////////////////////
// GetStatusWaiter
////////////////////////////////////////////////////////////////////////////////

// Helper class that will wait for the result of the
// |GetSpeakerIdEnrollmentStatus| call, and send it to the callback.
class SpeakerIdEnrollmentController::GetStatusWaiter : public AbortableTask {
 public:
  using Callback =
      SpeakerIdEnrollmentController::GetSpeakerIdEnrollmentStatusCallback;

  GetStatusWaiter() = default;
  ~GetStatusWaiter() override { DCHECK(!callback_); }
  GetStatusWaiter(const GetStatusWaiter&) = delete;
  GetStatusWaiter& operator=(const GetStatusWaiter&) = delete;

  void Start(AssistantClient* assistant_client,
             const std::string& user_gaia_id,
             Callback callback) {
    callback_ = std::move(callback);

    VLOG(1) << "Assistant: Retrieving speaker enrollment status";

    if (!assistant_client) {
      SendErrorResponse();
      return;
    }

    ::assistant::api::GetSpeakerIdEnrollmentInfoRequest request;
    auto* user_model_request = request.mutable_user_model_status_request();
    user_model_request->set_user_id(user_gaia_id);
    assistant_client->GetSpeakerIdEnrollmentInfo(
        request, base::BindOnce(&GetStatusWaiter::SendResponse,
                                weak_ptr_factory_.GetWeakPtr()));
  }

  // AbortableTask implementation:
  bool IsFinished() override { return callback_.is_null(); }
  void Abort() override { SendErrorResponse(); }

  void SendResponseForTesting(bool user_model_exists) {
    SendResponse(user_model_exists);
  }

 private:
  void SendErrorResponse() { SendResponse(false); }

  void SendResponse(bool user_model_exists) {
    VLOG(1) << "Assistant: Is user already enrolled? " << user_model_exists;

    std::move(callback_).Run(
        mojom::SpeakerIdEnrollmentStatus::New(user_model_exists));
  }

  Callback callback_;
  base::WeakPtrFactory<GetStatusWaiter> weak_ptr_factory_{this};
};

////////////////////////////////////////////////////////////////////////////////
// EnrollmentSession
////////////////////////////////////////////////////////////////////////////////

// A single enrollment session, created when speaker id enrollment is started,
// and destroyed when it is done or cancelled.
class SpeakerIdEnrollmentController::EnrollmentSession
    : public GrpcServicesObserver<OnSpeakerIdEnrollmentEventRequest> {
 public:
  using SpeakerIdEnrollmentEvent =
      ::assistant::api::events::SpeakerIdEnrollmentEvent;

  EnrollmentSession(
      ::mojo::PendingRemote<mojom::SpeakerIdEnrollmentClient> client,
      AssistantClient* assistant_client)
      : client_(std::move(client)), assistant_client_(assistant_client) {
    DCHECK(assistant_client_);
    scoped_assistant_client_observation_.Observe(assistant_client_.get());
  }
  EnrollmentSession(const EnrollmentSession&) = delete;
  EnrollmentSession& operator=(const EnrollmentSession&) = delete;
  ~EnrollmentSession() override { Stop(); }

  // GrpcServicesObserver:
  // Invoked when a Speaker Id Enrollment event has been received.
  void OnGrpcMessage(const ::assistant::api::OnSpeakerIdEnrollmentEventRequest&
                         request) override {
    switch (request.event().type_case()) {
      case SpeakerIdEnrollmentEvent::kListenState:
        VLOG(1) << "Assistant: Speaker id enrollment is listening";
        client_->OnListeningHotword();
        break;
      case SpeakerIdEnrollmentEvent::kProcessState:
        VLOG(1) << "Assistant: Speaker id enrollment is processing";
        client_->OnProcessingHotword();
        break;
      case SpeakerIdEnrollmentEvent::kDoneState:
        VLOG(1) << "Assistant: Speaker id enrollment is done";
        client_->OnSpeakerIdEnrollmentDone();
        done_ = true;
        break;
      case SpeakerIdEnrollmentEvent::kFailureState:
        VLOG(1) << "Assistant: Speaker id enrollment is done (with failure)";
        client_->OnSpeakerIdEnrollmentFailure();
        done_ = true;
        break;
      case SpeakerIdEnrollmentEvent::kInitState:
      case SpeakerIdEnrollmentEvent::kCheckState:
      case SpeakerIdEnrollmentEvent::kRecognizeState:
      case SpeakerIdEnrollmentEvent::kUploadState:
      case SpeakerIdEnrollmentEvent::kFetchState:
      case SpeakerIdEnrollmentEvent::TYPE_NOT_SET:
        break;
    }
  }

  void Start(const std::string& user_gaia_id, bool skip_cloud_enrollment) {
    VLOG(1) << "Assistant: Starting speaker id enrollment";

    ::assistant::api::StartSpeakerIdEnrollmentRequest request;
    request.set_user_id(user_gaia_id);
    request.set_skip_cloud_enrollment(skip_cloud_enrollment);
    assistant_client_->StartSpeakerIdEnrollment(request);
  }

  void Stop() {
    if (done_)
      return;

    VLOG(1) << "Assistant: Stopping speaker id enrollment";
    ::assistant::api::CancelSpeakerIdEnrollmentRequest request;
    assistant_client_->CancelSpeakerIdEnrollment(request);
  }

 private:
  ::mojo::Remote<mojom::SpeakerIdEnrollmentClient> client_;
  const raw_ptr<AssistantClient> assistant_client_;
  bool done_ = false;
  base::ScopedObservation<
      AssistantClient,
      GrpcServicesObserver<OnSpeakerIdEnrollmentEventRequest>>
      scoped_assistant_client_observation_{this};
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
  if (!assistant_client_)
    return;

  // Force mic state to open, otherwise the training might not open the
  // microphone. See b/139329513.
  audio_input_->SetMicOpen(true);

  // If there is an ongoing enrollment session, abort it first.
  if (active_enrollment_session_)
    active_enrollment_session_->Stop();

  active_enrollment_session_ =
      std::make_unique<EnrollmentSession>(std::move(client), assistant_client_);
  active_enrollment_session_->Start(user_gaia_id, skip_cloud_enrollment);
}

void SpeakerIdEnrollmentController::StopSpeakerIdEnrollment() {
  if (!assistant_client_)
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
  waiter->Start(assistant_client_, user_gaia_id, std::move(callback));
}

void SpeakerIdEnrollmentController::OnAssistantClientStarted(
    AssistantClient* assistant_client) {
  assistant_client_ = assistant_client;
}

void SpeakerIdEnrollmentController::OnDestroyingAssistantClient(
    AssistantClient* assistant_client) {
  active_enrollment_session_ = nullptr;
  assistant_client_ = nullptr;
  pending_response_waiters_.AbortAll();
}

void SpeakerIdEnrollmentController::OnGrpcMessageForTesting(
    const ::assistant::api::OnSpeakerIdEnrollmentEventRequest& request) {
  active_enrollment_session_->OnGrpcMessage(std::move(request));
}

void SpeakerIdEnrollmentController::SendGetStatusResponseForTesting(
    bool user_model_exists) {
  auto* waiter =
      reinterpret_cast<SpeakerIdEnrollmentController::GetStatusWaiter*>(
          pending_response_waiters_.GetFirstTaskForTesting());
  waiter->SendResponseForTesting(user_model_exists);
}

}  // namespace ash::libassistant
