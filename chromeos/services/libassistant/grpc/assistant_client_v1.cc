// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/libassistant/grpc/assistant_client_v1.h"

#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/synchronization/lock.h"
#include "chromeos/assistant/internal/grpc_transport/request_utils.h"
#include "chromeos/assistant/internal/proto/shared/proto/conversation.pb.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/delegate/event_handler_interface.pb.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/device_state_event.pb.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/display_interface.pb.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/speaker_id_enrollment_event.pb.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/speaker_id_enrollment_interface.pb.h"
#include "chromeos/services/libassistant/callback_utils.h"
#include "chromeos/services/libassistant/grpc/utils/media_status_utils.h"
#include "libassistant/shared/internal_api/assistant_manager_internal.h"
#include "libassistant/shared/internal_api/display_connection.h"
#include "libassistant/shared/internal_api/fuchsia_api_helper.h"
#include "libassistant/shared/internal_api/speaker_id_enrollment.h"
#include "libassistant/shared/public/device_state_listener.h"
#include "libassistant/shared/public/media_manager.h"

namespace chromeos {
namespace libassistant {

using ::assistant::api::OnSpeakerIdEnrollmentEventRequest;
using ::assistant::api::events::SpeakerIdEnrollmentEvent;
using assistant_client::SpeakerIdEnrollmentUpdate;

using ::assistant::api::OnDeviceStateEventRequest;

namespace {

// A macro which ensures we are running on the calling sequence.
#define ENSURE_CALLING_SEQUENCE(method, ...)                                \
  if (!task_runner_->RunsTasksInCurrentSequence()) {                        \
    task_runner_->PostTask(                                                 \
        FROM_HERE,                                                          \
        base::BindOnce(method, weak_factory_.GetWeakPtr(), ##__VA_ARGS__)); \
    return;                                                                 \
  }

OnSpeakerIdEnrollmentEventRequest ConvertToGrpcEventRequest(
    const ::assistant_client::SpeakerIdEnrollmentUpdate::State& state) {
  OnSpeakerIdEnrollmentEventRequest request;
  SpeakerIdEnrollmentEvent* event = request.mutable_event();
  switch (state) {
    case SpeakerIdEnrollmentUpdate::State::INIT: {
      event->mutable_init_state();
      break;
    }
    case SpeakerIdEnrollmentUpdate::State::CHECK: {
      event->mutable_check_state();
      break;
    }
    case SpeakerIdEnrollmentUpdate::State::LISTEN: {
      event->mutable_listen_state();
      break;
    }
    case SpeakerIdEnrollmentUpdate::State::PROCESS: {
      event->mutable_process_state();
      break;
    }
    case SpeakerIdEnrollmentUpdate::State::UPLOAD: {
      event->mutable_upload_state();
      break;
    }
    case SpeakerIdEnrollmentUpdate::State::FETCH: {
      event->mutable_fetch_state();
      break;
    }
    case SpeakerIdEnrollmentUpdate::State::DONE: {
      event->mutable_done_state();
      break;
    }
    case SpeakerIdEnrollmentUpdate::State::FAILURE: {
      event->mutable_failure_state();
      break;
    }
  }
  return request;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
//   AssistantClientV1::DeviceStateListener
////////////////////////////////////////////////////////////////////////////////

class AssistantClientV1::DeviceStateListener
    : public assistant_client::DeviceStateListener {
 public:
  explicit DeviceStateListener(AssistantClientV1* assistant_client)
      : assistant_client_(assistant_client),
        task_runner_(base::SequencedTaskRunnerHandle::Get()) {}
  DeviceStateListener(const DeviceStateListener&) = delete;
  DeviceStateListener& operator=(const DeviceStateListener&) = delete;
  ~DeviceStateListener() override = default;

  // assistant_client::DeviceStateListener:
  // Called from the Libassistant thread.
  void OnStartFinished() override {
    ENSURE_CALLING_SEQUENCE(&DeviceStateListener::OnStartFinished);

    OnDeviceStateEventRequest request;
    request.mutable_event()
        ->mutable_on_state_changed()
        ->mutable_new_state()
        ->mutable_startup_state()
        ->set_finished(true);
    assistant_client_->NofifyDeviceStateEvent(request);

    // AssistantManager Start() has completed, add media manager listener.
    assistant_client_->AddMediaManagerListener();
  }

 private:
  AssistantClientV1* assistant_client_ = nullptr;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::WeakPtrFactory<DeviceStateListener> weak_factory_{this};
};

////////////////////////////////////////////////////////////////////////////////
//   AssistantClientV1::DisplayConnectionImpl
////////////////////////////////////////////////////////////////////////////////

class AssistantClientV1::DisplayConnectionImpl
    : public assistant_client::DisplayConnection {
 public:
  DisplayConnectionImpl()
      : task_runner_(base::SequencedTaskRunnerHandle::Get()) {}
  DisplayConnectionImpl(const DisplayConnectionImpl&) = delete;
  DisplayConnectionImpl& operator=(const DisplayConnectionImpl&) = delete;
  ~DisplayConnectionImpl() override = default;

  // assistant_client::DisplayConnection overrides:
  void SetDelegate(Delegate* delegate) override {
    ENSURE_CALLING_SEQUENCE(&DisplayConnectionImpl::SetDelegate, delegate);

    delegate_ = delegate;
  }

  void OnAssistantEvent(const std::string& assistant_event_bytes) override {
    ENSURE_CALLING_SEQUENCE(&DisplayConnectionImpl::OnAssistantEvent,
                            assistant_event_bytes);

    DCHECK(observer_);

    OnAssistantDisplayEventRequest request;
    auto* assistant_display_event = request.mutable_event();
    auto* on_assistant_event =
        assistant_display_event->mutable_on_assistant_event();
    on_assistant_event->set_assistant_event_bytes(assistant_event_bytes);
    observer_->OnGrpcMessage(request);
  }

  void SetObserver(
      GrpcServicesObserver<OnAssistantDisplayEventRequest>* observer) {
    DCHECK(!observer_);
    observer_ = observer;
  }

  void OnDisplayRequest(const std::string& display_request_bytes) {
    if (!delegate_) {
      LOG(ERROR) << "Can't send DisplayRequest before delegate is set.";
      return;
    }

    delegate_->OnDisplayRequest(display_request_bytes);
  }

 private:
  Delegate* delegate_ = nullptr;

  GrpcServicesObserver<OnAssistantDisplayEventRequest>* observer_ = nullptr;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::WeakPtrFactory<DisplayConnectionImpl> weak_factory_{this};
};

////////////////////////////////////////////////////////////////////////////////
//   AssistantClientV1::MediaManagerListener
////////////////////////////////////////////////////////////////////////////////

class AssistantClientV1::MediaManagerListener
    : public assistant_client::MediaManager::Listener {
 public:
  explicit MediaManagerListener(AssistantClientV1* assistant_client)
      : assistant_client_(assistant_client),
        task_runner_(base::SequencedTaskRunnerHandle::Get()) {}
  MediaManagerListener(const MediaManagerListener&) = delete;
  MediaManagerListener& operator=(const MediaManagerListener&) = delete;
  ~MediaManagerListener() override = default;

  // assistant_client::MediaManager::Listener:
  // Called from the Libassistant thread.
  void OnPlaybackStateChange(
      const assistant_client::MediaStatus& media_status) override {
    ENSURE_CALLING_SEQUENCE(&MediaManagerListener::OnPlaybackStateChange,
                            media_status);

    OnDeviceStateEventRequest request;
    auto* status = request.mutable_event()
                       ->mutable_on_state_changed()
                       ->mutable_new_state()
                       ->mutable_media_status();
    ConvertMediaStatusToV2FromV1(media_status, status);
    assistant_client_->NofifyDeviceStateEvent(request);
  }

 private:
  AssistantClientV1* assistant_client_ = nullptr;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::WeakPtrFactory<MediaManagerListener> weak_factory_{this};
};

////////////////////////////////////////////////////////////////////////////////
//   AssistantClientV1
////////////////////////////////////////////////////////////////////////////////

AssistantClientV1::AssistantClientV1(
    std::unique_ptr<assistant_client::AssistantManager> manager,
    assistant_client::AssistantManagerInternal* assistant_manager_internal)
    : AssistantClient(std::move(manager), assistant_manager_internal),
      device_state_listener_(std::make_unique<DeviceStateListener>(this)),
      display_connection_(std::make_unique<DisplayConnectionImpl>()),
      media_manager_listener_(std::make_unique<MediaManagerListener>(this)) {
  assistant_manager()->AddDeviceStateListener(device_state_listener_.get());
}

AssistantClientV1::~AssistantClientV1() = default;

void AssistantClientV1::StartServices() {
  assistant_manager()->Start();
}

void AssistantClientV1::SetChromeOSApiDelegate(
    assistant_client::ChromeOSApiDelegate* delegate) {
  assistant_manager_internal()
      ->GetFuchsiaApiHelperOrDie()
      ->SetChromeOSApiDelegate(delegate);
}

bool AssistantClientV1::StartGrpcServices() {
  return true;
}

void AssistantClientV1::AddExperimentIds(
    const std::vector<std::string>& exp_ids) {
  assistant_manager_internal()->AddExtraExperimentIds(exp_ids);
}

void AssistantClientV1::SendVoicelessInteraction(
    const ::assistant::api::Interaction& interaction,
    const std::string& description,
    const ::assistant::api::VoicelessOptions& options,
    base::OnceCallback<void(bool)> on_done) {
  assistant_client::VoicelessOptions voiceless_options;
  PopulateVoicelessOptionsFromProto(options, &voiceless_options);
  assistant_manager_internal()->SendVoicelessInteraction(
      interaction.SerializeAsString(), description, voiceless_options,
      [callback = std::move(on_done)](bool result) mutable {
        std::move(callback).Run(result);
      });
}

void AssistantClientV1::AddSpeakerIdEnrollmentEventObserver(
    GrpcServicesObserver<OnSpeakerIdEnrollmentEventRequest>* observer) {
  speaker_event_observer_list_.AddObserver(observer);
}

void AssistantClientV1::RemoveSpeakerIdEnrollmentEventObserver(
    GrpcServicesObserver<OnSpeakerIdEnrollmentEventRequest>* observer) {
  speaker_event_observer_list_.RemoveObserver(observer);
}

void AssistantClientV1::StartSpeakerIdEnrollment(
    const StartSpeakerIdEnrollmentRequest& request) {
  assistant_client::SpeakerIdEnrollmentConfig client_config;
  client_config.user_id = request.user_id();
  client_config.skip_cloud_enrollment = request.skip_cloud_enrollment();

  auto callback =
      base::BindRepeating(&AssistantClientV1::OnSpeakerIdEnrollmentUpdate,
                          weak_factory_.GetWeakPtr());

  assistant_manager_internal()->StartSpeakerIdEnrollment(
      client_config,
      ToStdFunctionRepeating(BindToCurrentSequenceRepeating(callback)));
}

void AssistantClientV1::CancelSpeakerIdEnrollment(
    const CancelSpeakerIdEnrollmentRequest& request) {
  assistant_manager_internal()->StopSpeakerIdEnrollment([]() {});
}

void AssistantClientV1::GetSpeakerIdEnrollmentInfo(
    const ::assistant::api::GetSpeakerIdEnrollmentInfoRequest& request,
    base::OnceCallback<void(bool user_model_exists)> on_done) {
  auto callback = base::BindOnce(
      [](base::OnceCallback<void(bool user_model_exists)> cb,
         const assistant_client::SpeakerIdEnrollmentStatus& status) {
        std::move(cb).Run(status.user_model_exists);
      },
      std::move(on_done));

  assistant_manager_internal()->GetSpeakerIdEnrollmentStatus(
      request.cloud_enrollment_status_request().user_id(),
      ToStdFunction(BindToCurrentSequence(std::move(callback))));
}

void AssistantClientV1::ResetAllDataAndShutdown() {
  assistant_manager()->ResetAllDataAndShutdown();
}

void AssistantClientV1::OnDisplayRequest(
    const OnDisplayRequestRequest& request) {
  display_connection_->OnDisplayRequest(request.display_request_bytes());
}

void AssistantClientV1::AddDisplayEventObserver(
    GrpcServicesObserver<OnAssistantDisplayEventRequest>* observer) {
  display_connection_->SetObserver(observer);
  assistant_manager_internal()->SetDisplayConnection(display_connection_.get());
}

void AssistantClientV1::ResumeCurrentStream() {
  assistant_manager()->GetMediaManager()->Resume();
}

void AssistantClientV1::PauseCurrentStream() {
  assistant_manager()->GetMediaManager()->Pause();
}

void AssistantClientV1::SetExternalPlaybackState(
    const MediaStatus& status_proto) {
  assistant_client::MediaStatus media_status;
  ConvertMediaStatusToV1FromV2(status_proto, &media_status);
  assistant_manager()->GetMediaManager()->SetExternalPlaybackState(
      media_status);
}

void AssistantClientV1::AddDeviceStateEventObserver(
    GrpcServicesObserver<OnDeviceStateEventRequest>* observer) {
  device_state_event_observer_list_.AddObserver(observer);
}

void AssistantClientV1::AddMediaManagerListener() {
  assistant_manager()->GetMediaManager()->AddListener(
      media_manager_listener_.get());
}

void AssistantClientV1::NofifyDeviceStateEvent(
    const OnDeviceStateEventRequest& request) {
  for (auto& observer : device_state_event_observer_list_) {
    observer.OnGrpcMessage(request);
  }
}

void AssistantClientV1::OnSpeakerIdEnrollmentUpdate(
    const SpeakerIdEnrollmentUpdate& update) {
  auto event_request = ConvertToGrpcEventRequest(update.state);
  for (auto& observer : speaker_event_observer_list_) {
    observer.OnGrpcMessage(event_request);
  }
}

}  // namespace libassistant
}  // namespace chromeos
