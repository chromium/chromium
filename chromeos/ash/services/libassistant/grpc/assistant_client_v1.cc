// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/libassistant/grpc/assistant_client_v1.h"

#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chromeos/ash/services/assistant/public/cpp/features.h"
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

[[nodiscard]] assistant_client::InternalOptions* CreateInternalOptions(
    assistant_client::AssistantManagerInternal* assistant_manager_internal,
    const std::string& locale,
    bool spoken_feedback_enabled,
    bool dark_mode_enabled) {
  auto* options = assistant_manager_internal->CreateDefaultInternalOptions();
  auto proto = chromeos::assistant::CreateInternalOptionsProto(
      locale, spoken_feedback_enabled);
  chromeos::libassistant::PopulateInternalOptionsFromProto(proto, options);

  chromeos::assistant::SetDarkModeEnabledForV1(options, dark_mode_enabled);
  chromeos::assistant::SetTimezoneOverrideForV1(options);

  return options;
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

    // We will be checking the heartbeat signal sent back for Libassistant for
    // v2.
    if (!assistant::features::IsLibAssistantV2Enabled())
      assistant_client_->NotifyAllServicesReady();
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
      : task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {}
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

  void SendDisplayRequest(const std::string& display_request_bytes) {
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
  AssistantClientV1* assistant_client_ = nullptr;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::WeakPtrFactory<MediaManagerListener> weak_factory_{this};
};

////////////////////////////////////////////////////////////////////////////////
// AssistantClientV1::AssistantManagerDelegateImpl
////////////////////////////////////////////////////////////////////////////////

// Implementation of |AssistantManagerDelegate| that will forward all calls
// to the correct observers.
// It also keeps track of the last text query that was started, so we can
// pass its metadata to |OnConversationTurnStarted|.
class AssistantClientV1::AssistantManagerDelegateImpl
    : public assistant_client::AssistantManagerDelegate {
 public:
  explicit AssistantManagerDelegateImpl(AssistantClientV1* assistant_client)
      : assistant_client_(assistant_client),
        task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {}
  AssistantManagerDelegateImpl(const AssistantManagerDelegateImpl&) = delete;
  AssistantManagerDelegateImpl& operator=(const AssistantManagerDelegateImpl&) =
      delete;
  ~AssistantManagerDelegateImpl() override = default;

  // assistant_client::AssistantManagerDelegate overrides:
  void OnConversationTurnStartedInternal(
      const assistant_client::ConversationTurnMetadata& metadata) override {
    ENSURE_CALLING_SEQUENCE(
        &AssistantManagerDelegateImpl::OnConversationTurnStartedInternal,
        metadata);

    OnConversationStateEventRequest request;
    auto* turn_started = request.mutable_event()->mutable_on_turn_started();
    turn_started->set_turn_id(metadata.id);
    turn_started->set_is_mic_open(metadata.is_mic_open);
    assistant_client_->NotifyConversationStateEvent(request);
  }

  void OnNotificationRemoved(const std::string& grouping_key) override {
    ENSURE_CALLING_SEQUENCE(
        &AssistantManagerDelegateImpl::OnNotificationRemoved, grouping_key);

    OnDeviceStateEventRequest request;
    auto* notification_removed =
        request.mutable_event()->mutable_on_notification_removed();
    notification_removed->set_grouping_id(grouping_key);
    assistant_client_->NotifyDeviceStateEvent(request);
  }

  void OnCommunicationError(int error_code) override {
    ENSURE_CALLING_SEQUENCE(&AssistantManagerDelegateImpl::OnCommunicationError,
                            error_code);

    if (chromeos::assistant::IsAuthError(error_code)) {
      OnDeviceStateEventRequest request;
      auto* communication_error =
          request.mutable_event()->mutable_on_communication_error();
      communication_error->set_error_code(
          ::assistant::api::events::DeviceStateEvent::OnCommunicationError::
              AUTH_TOKEN_FAIL);
      assistant_client_->NotifyDeviceStateEvent(request);
    }
  }

 private:
  AssistantClientV1* assistant_client_ = nullptr;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::WeakPtrFactory<AssistantManagerDelegateImpl> weak_factory_{this};
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
      media_manager_listener_(std::make_unique<MediaManagerListener>(this)),
      assistant_manager_delegate_(
          std::make_unique<AssistantManagerDelegateImpl>(this)) {
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

  // Instead we will be checking the heartbeat signal sent back from Libassisant
  // in v2.
  if (!assistant::features::IsLibAssistantV2Enabled()) {
    services_status_observer_->OnServicesStatusChanged(
        ServicesStatus::ONLINE_BOOTING_UP);
  }
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

void AssistantClientV1::StartGrpcHttpConnectionClient(
    assistant_client::HttpConnectionFactory*) {
  NOTIMPLEMENTED();
}

void AssistantClientV1::AddExperimentIds(
    const std::vector<std::string>& exp_ids) {
  assistant_manager_internal()->AddExtraExperimentIds(exp_ids);
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
  auto callback =
      AdaptCallback<const assistant_client::SpeakerIdEnrollmentStatus&>(
          /*once_callback=*/std::move(on_done),
          /*transformer=*/[](const assistant_client::SpeakerIdEnrollmentStatus&
                                 status) { return status.user_model_exists; });

  assistant_manager_internal()->GetSpeakerIdEnrollmentStatus(
      request.cloud_enrollment_status_request().user_id(),
      ToStdFunction(BindToCurrentSequence(std::move(callback))));
}

void AssistantClientV1::ResetAllDataAndShutdown() {
  assistant_manager()->ResetAllDataAndShutdown();
}

void AssistantClientV1::SendDisplayRequest(
    const OnDisplayRequestRequest& request) {
  display_connection_->SendDisplayRequest(request.display_request_bytes());
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

void AssistantClientV1::AddMediaActionFallbackEventObserver(
    GrpcServicesObserver<OnMediaActionFallbackEventRequest>* observer) {
  media_action_fallback_event_observer_list_.AddObserver(observer);

  // Register handler for media actions.
  auto callback = base::BindRepeating(&AssistantClientV1::HandleMediaAction,
                                      weak_factory_.GetWeakPtr());
  assistant_manager_internal()->RegisterFallbackMediaHandler(
      ToStdFunctionRepeating(BindToCurrentSequenceRepeating(callback)));
}

void AssistantClientV1::SendVoicelessInteraction(
    const ::assistant::api::Interaction& interaction,
    const std::string& description,
    const ::assistant::api::VoicelessOptions& options,
    base::OnceCallback<void(bool)> on_done) {
  assistant_client::VoicelessOptions voiceless_options;
  chromeos::libassistant::PopulateVoicelessOptionsFromProto(options,
                                                            &voiceless_options);
  assistant_manager_internal()->SendVoicelessInteraction(
      interaction.SerializeAsString(), description, voiceless_options,
      [callback = std::move(on_done)](bool result) mutable {
        std::move(callback).Run(result);
      });
}

void AssistantClientV1::RegisterActionModule(
    assistant_client::ActionModule* action_module) {
  assistant_manager_internal()->RegisterActionModule(action_module);
}

void AssistantClientV1::StartVoiceInteraction() {
  assistant_manager()->StartAssistantInteraction();
}

void AssistantClientV1::StopAssistantInteraction(bool cancel_conversation) {
  assistant_manager_internal()->StopAssistantInteractionInternal(
      cancel_conversation);
}

void AssistantClientV1::AddConversationStateEventObserver(
    GrpcServicesObserver<OnConversationStateEventRequest>* observer) {
  conversation_state_event_observer_list_.AddObserver(observer);
  assistant_manager_internal()->SetAssistantManagerDelegate(
      assistant_manager_delegate_.get());
}

void AssistantClientV1::SetAuthenticationInfo(const AuthTokens& tokens) {
  assistant_manager()->SetAuthTokens(tokens);
}

void AssistantClientV1::SetInternalOptions(const std::string& locale,
                                           bool spoken_feedback_enabled) {
  // All options must have value before we can convey them to libassistant.
  DCHECK(dark_mode_enabled_.has_value());

  assistant_manager_internal()->SetOptions(
      *CreateInternalOptions(assistant_manager_internal(), locale,
                             spoken_feedback_enabled,
                             dark_mode_enabled_.value()),
      [](bool success) { DVLOG(2) << "set options: " << success; });
}

void AssistantClientV1::UpdateAssistantSettings(
    const SettingsUiUpdate& settings,
    const std::string& user_id,
    base::OnceCallback<void(const UpdateAssistantSettingsResponse&)> on_done) {
  std::string update_settings_ui_request =
      chromeos::assistant::SerializeUpdateSettingsUiRequest(
          settings.SerializeAsString());

  auto callback = AdaptCallback<const assistant_client::VoicelessResponse&>(
      /*once_callback=*/std::move(on_done),
      /*transformer=*/&ToUpdateSettingsResponseProto);

  assistant_manager_internal()->SendUpdateSettingsUiRequest(
      update_settings_ui_request, user_id,
      ToStdFunction(BindToCurrentSequence(std::move(callback))));
}

void AssistantClientV1::GetAssistantSettings(
    const ::assistant::ui::SettingsUiSelector& selector,
    const std::string& user_id,
    base::OnceCallback<void(const GetAssistantSettingsResponse&)> on_done) {
  std::string get_settins_ui_request =
      chromeos::assistant::SerializeGetSettingsUiRequest(
          selector.SerializeAsString());

  auto callback = AdaptCallback<const assistant_client::VoicelessResponse&>(
      /*once_callback=*/std::move(on_done),
      /*transformer=*/&ToGetSettingsResponseProto);

  assistant_manager_internal()->SendGetSettingsUiRequest(
      get_settins_ui_request, user_id,
      ToStdFunction(BindToCurrentSequence(std::move(callback))));
}

void AssistantClientV1::AddMediaManagerListener() {
  assistant_manager()->GetMediaManager()->AddListener(
      media_manager_listener_.get());
}

void AssistantClientV1::HandleMediaAction(
    const std::string& action_name,
    const std::string& media_action_args_proto) {
  OnMediaActionFallbackEventRequest request;
  auto* media_action = request.mutable_event()->mutable_on_media_action_event();
  media_action->set_action_name(action_name);
  media_action->set_action_args(media_action_args_proto);

  for (auto& observer : media_action_fallback_event_observer_list_) {
    observer.OnGrpcMessage(request);
  }
}

void AssistantClientV1::NotifyConversationStateEvent(
    const OnConversationStateEventRequest& request) {
  for (auto& observer : conversation_state_event_observer_list_) {
    observer.OnGrpcMessage(request);
  }
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

void AssistantClientV1::OnSpeakerIdEnrollmentUpdate(
    const SpeakerIdEnrollmentUpdate& update) {
  auto event_request = ConvertToGrpcEventRequest(update.state);
  for (auto& observer : speaker_event_observer_list_) {
    observer.OnGrpcMessage(event_request);
  }
}

void AssistantClientV1::SetLocaleOverride(const std::string& locale) {
  assistant_manager_internal()->SetLocaleOverride(locale);
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
  if (alarm_timer_manager())
    alarm_timer_manager()->AddTimeToTimer(id, duration.InSeconds());
}

void AssistantClientV1::PauseTimer(const std::string& timer_id) {
  if (alarm_timer_manager())
    alarm_timer_manager()->PauseTimer(timer_id);
}

void AssistantClientV1::RemoveTimer(const std::string& timer_id) {
  if (alarm_timer_manager())
    alarm_timer_manager()->RemoveEvent(timer_id);
}

void AssistantClientV1::ResumeTimer(const std::string& timer_id) {
  if (alarm_timer_manager())
    alarm_timer_manager()->ResumeTimer(timer_id);
}

void AssistantClientV1::GetTimers(
    base::OnceCallback<void(const std::vector<assistant::AssistantTimer>&)>
        on_done) {
  std::vector<assistant::AssistantTimer> timers;
  if (alarm_timer_manager()) {
    timers =
        GetAllCurrentTimersFromEvents(alarm_timer_manager()->GetAllEvents());
  }
  std::move(on_done).Run(std::move(timers));
}

void AssistantClientV1::AddAlarmTimerEventObserver(
    GrpcServicesObserver<::assistant::api::OnAlarmTimerEventRequest>*
        observer) {
  timer_event_observer_list_.AddObserver(observer);

  // We always want to know when a timer has started ringing.
  alarm_timer_manager()->RegisterRingingStateListener(
      ToStdFunctionRepeating(BindToCurrentSequenceRepeating(
          [](const base::WeakPtr<AssistantClientV1>& self) {
            if (self) {
              self->GetAndNotifyTimerStatus();
            }
          },
          weak_factory_.GetWeakPtr())));

  // In timers v2, we also want to know when timers are scheduled,
  // updated, and/or removed so that we can represent those states
  // in UI.
  alarm_timer_manager()->RegisterTimerActionListener(
      ToStdFunctionRepeating(BindToCurrentSequenceRepeating(
          [](const base::WeakPtr<AssistantClientV1>& self,
             const assistant_client::AlarmTimerManager::EventActionType&
                 ignore) {
            if (self) {
              self->GetAndNotifyTimerStatus();
            }
          },
          weak_factory_.GetWeakPtr())));
}

assistant_client::AlarmTimerManager* AssistantClientV1::alarm_timer_manager() {
  DCHECK(assistant_manager_internal());
  return assistant_manager_internal()->GetAlarmTimerManager();
}

void AssistantClientV1::GetAndNotifyTimerStatus() {
  GetTimers(base::BindOnce(
      [](const base::WeakPtr<AssistantClientV1>& self,
         const std::vector<assistant::AssistantTimer>& timers) {
        // Observers outlive `this`. Observers are added when
        // `OnAssistantClientRunning()`, and destroyed when
        // `OnDestroyingAssistantClient()`.
        if (self) {
          for (auto& observer : self->timer_event_observer_list_) {
            observer.OnGrpcMessage(
                CreateOnAlarmTimerEventRequestProtoForV1(timers));
          }
        }
      },
      weak_factory_.GetWeakPtr()));
}

}  // namespace ash::libassistant
