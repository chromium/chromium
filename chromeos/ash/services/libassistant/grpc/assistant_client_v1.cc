// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/libassistant/grpc/assistant_client_v1.h"

#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chromeos/ash/services/libassistant/callback_utils.h"
#include "chromeos/ash/services/libassistant/grpc/assistant_client.h"
#include "chromeos/ash/services/libassistant/grpc/utils/media_status_utils.h"
#include "chromeos/ash/services/libassistant/grpc/utils/settings_utils.h"
#include "chromeos/ash/services/libassistant/grpc/utils/timer_utils.h"
#include "chromeos/ash/services/libassistant/public/cpp/assistant_timer.h"
#include "chromeos/assistant/internal/grpc_transport/request_utils.h"
#include "chromeos/assistant/internal/internal_util.h"
#include "chromeos/assistant/internal/libassistant/shared_headers.h"
#include "chromeos/assistant/internal/proto/shared/proto/conversation.pb.h"
#include "chromeos/assistant/internal/proto/shared/proto/settings_ui.pb.h"
#include "chromeos/assistant/internal/proto/shared/proto/update_settings_ui.pb.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/config_settings_interface.pb.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/delegate/event_handler_interface.pb.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/device_state_event.pb.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/display_interface.pb.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/speaker_id_enrollment_event.pb.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/speaker_id_enrollment_interface.pb.h"

namespace ash::libassistant {

namespace {

using ::assistant::api::GetAssistantSettingsResponse;
using ::assistant::api::OnAlarmTimerEventRequest;
using ::assistant::api::OnDeviceStateEventRequest;
using ::assistant::api::OnSpeakerIdEnrollmentEventRequest;
using ::assistant::api::UpdateAssistantSettingsResponse;
using ::assistant::api::events::SpeakerIdEnrollmentEvent;
using ::assistant::ui::SettingsUiUpdate;
using assistant_client::SpeakerIdEnrollmentUpdate;

// A macro which ensures we are running on the calling sequence.
#define ENSURE_CALLING_SEQUENCE(method, ...)                                \
  if (!task_runner_->RunsTasksInCurrentSequence()) {                        \
    task_runner_->PostTask(                                                 \
        FROM_HERE,                                                          \
        base::BindOnce(method, weak_factory_.GetWeakPtr(), ##__VA_ARGS__)); \
    return;                                                                 \
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
        task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {}
  DeviceStateListener(const DeviceStateListener&) = delete;
  DeviceStateListener& operator=(const DeviceStateListener&) = delete;
  ~DeviceStateListener() override = default;

  // assistant_client::DeviceStateListener:
  // Called from the Libassistant thread.
  void OnStartFinished() override {
    ENSURE_CALLING_SEQUENCE(&DeviceStateListener::OnStartFinished);

    // Now |AssistantManager| is fully started, add media manager listener.
    assistant_client_->AddMediaManagerListener();
  }

 private:
  raw_ptr<AssistantClientV1, ExperimentalAsh> assistant_client_ = nullptr;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::WeakPtrFactory<DeviceStateListener> weak_factory_{this};
};

////////////////////////////////////////////////////////////////////////////////
//   AssistantClientV1::MediaManagerListener
////////////////////////////////////////////////////////////////////////////////

class AssistantClientV1::MediaManagerListener
    : public assistant_client::MediaManager::Listener {
 public:
  explicit MediaManagerListener(AssistantClientV1* assistant_client)
      : assistant_client_(assistant_client),
        task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {}
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
    assistant_client_->NotifyDeviceStateEvent(request);
  }

 private:
  raw_ptr<AssistantClientV1, ExperimentalAsh> assistant_client_ = nullptr;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::WeakPtrFactory<MediaManagerListener> weak_factory_{this};
};

////////////////////////////////////////////////////////////////////////////////
//   AssistantClientV1
////////////////////////////////////////////////////////////////////////////////

AssistantClientV1::AssistantClientV1(
    std::unique_ptr<assistant_client::AssistantManager> manager)
    : AssistantClient(std::move(manager)),
      device_state_listener_(std::make_unique<DeviceStateListener>(this)),
      media_manager_listener_(std::make_unique<MediaManagerListener>(this)) {
  assistant_manager()->AddDeviceStateListener(device_state_listener_.get());
}

AssistantClientV1::~AssistantClientV1() {
  // Some listeners (e.g. MediaManagerListener) require that they outlive
  // `assistant_manager_`. Reset `assistant_manager_` in the parent class first
  // before any listener in this class gets destructed.
  ResetAssistantManager();
}

void AssistantClientV1::StartServices(
    ServicesStatusObserver* services_status_observer) {
  DCHECK(services_status_observer);
  services_status_observer_ = services_status_observer;
}

bool AssistantClientV1::StartGrpcServices() {
  return true;
}

void AssistantClientV1::StartGrpcHttpConnectionClient(
    assistant_client::HttpConnectionFactory*) {
  NOTIMPLEMENTED();
}

void AssistantClientV1::AddExperimentIds(
    const std::vector<std::string>& exp_ids) {
  NOTREACHED_NORETURN();
}

void AssistantClientV1::AddSpeakerIdEnrollmentEventObserver(
    GrpcServicesObserver<OnSpeakerIdEnrollmentEventRequest>* observer) {
  NOTREACHED_NORETURN();
}

void AssistantClientV1::RemoveSpeakerIdEnrollmentEventObserver(
    GrpcServicesObserver<OnSpeakerIdEnrollmentEventRequest>* observer) {
  NOTREACHED_NORETURN();
}

void AssistantClientV1::StartSpeakerIdEnrollment(
    const StartSpeakerIdEnrollmentRequest& request) {
  NOTREACHED_NORETURN();
}

void AssistantClientV1::CancelSpeakerIdEnrollment(
    const CancelSpeakerIdEnrollmentRequest& request) {
  NOTREACHED_NORETURN();
}

void AssistantClientV1::GetSpeakerIdEnrollmentInfo(
    const ::assistant::api::GetSpeakerIdEnrollmentInfoRequest& request,
    base::OnceCallback<void(bool user_model_exists)> on_done) {
  NOTREACHED_NORETURN();
}

void AssistantClientV1::ResetAllDataAndShutdown() {
  assistant_manager()->ResetAllDataAndShutdown();
}

void AssistantClientV1::SendDisplayRequest(
    const OnDisplayRequestRequest& request) {
  NOTREACHED_NORETURN();
}

void AssistantClientV1::AddDisplayEventObserver(
    GrpcServicesObserver<OnAssistantDisplayEventRequest>* observer) {
  NOTREACHED_NORETURN();
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

void AssistantClientV1::AddMediaActionFallbackEventObserver(
    GrpcServicesObserver<OnMediaActionFallbackEventRequest>* observer) {
  NOTREACHED_NORETURN();
}

void AssistantClientV1::SendVoicelessInteraction(
    const ::assistant::api::Interaction& interaction,
    const std::string& description,
    const ::assistant::api::VoicelessOptions& options,
    base::OnceCallback<void(bool)> on_done) {
  NOTREACHED_NORETURN();
}

void AssistantClientV1::RegisterActionModule(
    assistant_client::ActionModule* action_module) {
  NOTREACHED_NORETURN();
}

void AssistantClientV1::StartVoiceInteraction() {
  assistant_manager()->StartAssistantInteraction();
}

void AssistantClientV1::StopAssistantInteraction(bool cancel_conversation) {
  NOTREACHED_NORETURN();
}

void AssistantClientV1::AddConversationStateEventObserver(
    GrpcServicesObserver<OnConversationStateEventRequest>* observer) {
  NOTREACHED_NORETURN();
}

void AssistantClientV1::SetAuthenticationInfo(const AuthTokens& tokens) {
  assistant_manager()->SetAuthTokens(tokens);
}

void AssistantClientV1::SetInternalOptions(const std::string& locale,
                                           bool spoken_feedback_enabled) {
  NOTREACHED_NORETURN();
}

void AssistantClientV1::UpdateAssistantSettings(
    const SettingsUiUpdate& settings,
    const std::string& user_id,
    base::OnceCallback<void(const UpdateAssistantSettingsResponse&)> on_done) {
  NOTREACHED_NORETURN();
}

void AssistantClientV1::GetAssistantSettings(
    const ::assistant::ui::SettingsUiSelector& selector,
    const std::string& user_id,
    base::OnceCallback<void(const GetAssistantSettingsResponse&)> on_done) {
  NOTREACHED_NORETURN();
}

void AssistantClientV1::AddMediaManagerListener() {
  assistant_manager()->GetMediaManager()->AddListener(
      media_manager_listener_.get());
}

void AssistantClientV1::NotifyDeviceStateEvent(
    const OnDeviceStateEventRequest& request) {
  for (auto& observer : device_state_event_observer_list_) {
    observer.OnGrpcMessage(request);
  }
}

void AssistantClientV1::NotifyAllServicesReady() {
  services_status_observer_->OnServicesStatusChanged(
      ServicesStatus::ONLINE_ALL_SERVICES_AVAILABLE);
}

void AssistantClientV1::SetLocaleOverride(const std::string& locale) {
  NOTREACHED_NORETURN();
}

void AssistantClientV1::SetDeviceAttributes(bool enable_dark_mode) {
  // We don't actually do anything here besides caching the passed in value
  // because dark mode is set through |SetOptions| for V1.
  dark_mode_enabled_ = enable_dark_mode;
}

std::string AssistantClientV1::GetDeviceId() {
  return assistant_manager()->GetDeviceId();
}

void AssistantClientV1::EnableListening(bool listening_enabled) {
  assistant_manager()->EnableListening(listening_enabled);
}

void AssistantClientV1::AddTimeToTimer(const std::string& id,
                                       const base::TimeDelta& duration) {
  NOTREACHED_NORETURN();
}

void AssistantClientV1::PauseTimer(const std::string& timer_id) {
  NOTREACHED_NORETURN();
}

void AssistantClientV1::RemoveTimer(const std::string& timer_id) {
  NOTREACHED_NORETURN();
}

void AssistantClientV1::ResumeTimer(const std::string& timer_id) {
  NOTREACHED_NORETURN();
}

void AssistantClientV1::GetTimers(
    base::OnceCallback<void(const std::vector<assistant::AssistantTimer>&)>
        on_done) {
  NOTREACHED_NORETURN();
}

void AssistantClientV1::AddAlarmTimerEventObserver(
    GrpcServicesObserver<::assistant::api::OnAlarmTimerEventRequest>*
        observer) {
  NOTREACHED_NORETURN();
}

}  // namespace ash::libassistant
