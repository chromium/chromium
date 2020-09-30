// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/assistant_manager_service_impl.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "ash/public/cpp/ambient/ambient_ui_model.h"
#include "ash/public/cpp/assistant/assistant_state_base.h"
#include "ash/public/cpp/assistant/controller/assistant_alarm_timer_controller.h"
#include "ash/public/cpp/assistant/controller/assistant_notification_controller.h"
#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/feature_list.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "chromeos/assistant/internal/internal_constants.h"
#include "chromeos/assistant/internal/proto/google3/assistant/api/client_op/device_args.pb.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/dbus/util/version_loader.h"
#include "chromeos/services/assistant/assistant_device_settings_delegate.h"
#include "chromeos/services/assistant/assistant_manager_service_delegate.h"
#include "chromeos/services/assistant/media_session/assistant_media_session.h"
#include "chromeos/services/assistant/platform_api_impl.h"
#include "chromeos/services/assistant/public/cpp/assistant_client.h"
#include "chromeos/services/assistant/public/cpp/device_actions.h"
#include "chromeos/services/assistant/public/cpp/features.h"
#include "chromeos/services/assistant/public/shared/utils.h"
#include "chromeos/services/assistant/service_context.h"
#include "chromeos/services/assistant/utils.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "libassistant/shared/internal_api/alarm_timer_manager.h"
#include "libassistant/shared/internal_api/alarm_timer_types.h"
#include "libassistant/shared/internal_api/assistant_manager_delegate.h"
#include "libassistant/shared/internal_api/assistant_manager_internal.h"
#include "libassistant/shared/public/assistant_manager.h"
#include "libassistant/shared/public/media_manager.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "ui/accessibility/mojom/ax_assistant_structure.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

// A macro which ensures we are running on the main thread.
#define ENSURE_MAIN_THREAD(method, ...)                                     \
  if (!main_task_runner()->RunsTasksInCurrentSequence()) {                  \
    main_task_runner()->PostTask(                                           \
        FROM_HERE,                                                          \
        base::BindOnce(method, weak_factory_.GetWeakPtr(), ##__VA_ARGS__)); \
    return;                                                                 \
  }

using assistant_client::ActionModule;
using assistant_client::MediaStatus;
using media_session::mojom::MediaSessionAction;
using media_session::mojom::MediaSessionInfo;
using media_session::mojom::MediaSessionInfoPtr;
using Resolution = assistant_client::ConversationStateListener::Resolution;
using CommunicationErrorType =
    chromeos::assistant::AssistantManagerService::CommunicationErrorType;

namespace api = ::assistant::api;

namespace chromeos {
namespace assistant {
namespace {

static bool is_first_init = true;

constexpr char kIntentActionView[] = "android.intent.action.VIEW";

constexpr base::Feature kChromeOSAssistantDogfood{
    "ChromeOSAssistantDogfood", base::FEATURE_DISABLED_BY_DEFAULT};

constexpr char kServersideDogfoodExperimentId[] = "20347368";
constexpr char kServersideOpenAppExperimentId[] = "39651593";
constexpr char kServersideResponseProcessingV2ExperimentId[] = "1793869";

constexpr char kNextTrackClientOp[] = "media.NEXT";
constexpr char kPauseTrackClientOp[] = "media.PAUSE";
constexpr char kPlayMediaClientOp[] = "media.PLAY_MEDIA";
constexpr char kPrevTrackClientOp[] = "media.PREVIOUS";
constexpr char kResumeTrackClientOp[] = "media.RESUME";
constexpr char kStopTrackClientOp[] = "media.STOP";

constexpr char kAndroidSettingsAppPackage[] = "com.android.settings";

ash::AssistantTimerState GetTimerState(assistant_client::Timer::State state) {
  switch (state) {
    case assistant_client::Timer::State::UNKNOWN:
      return ash::AssistantTimerState::kUnknown;
    case assistant_client::Timer::State::SCHEDULED:
      return ash::AssistantTimerState::kScheduled;
    case assistant_client::Timer::State::PAUSED:
      return ash::AssistantTimerState::kPaused;
    case assistant_client::Timer::State::FIRED:
      return ash::AssistantTimerState::kFired;
  }
}

CommunicationErrorType CommunicationErrorTypeFromLibassistantErrorCode(
    int error_code) {
  if (IsAuthError(error_code))
    return CommunicationErrorType::AuthenticationError;
  return CommunicationErrorType::Other;
}

std::vector<std::pair<std::string, std::string>> ToAuthTokensOrEmpty(
    const base::Optional<AssistantManagerService::UserInfo>& user) {
  if (!user.has_value())
    return {};

  DCHECK(!user.value().gaia_id.empty());
  DCHECK(!user.value().access_token.empty());
  return {std::make_pair(user.value().gaia_id, user.value().access_token)};
}

void UpdateInternalOptions(
    assistant_client::AssistantManagerInternal* assistant_manager_internal,
    const std::string& locale,
    bool spoken_feedback_enabled) {
  // NOTE: this method is called on multiple threads, it needs to be
  // thread-safe.
  auto* internal_options =
      assistant_manager_internal->CreateDefaultInternalOptions();
  SetAssistantOptions(internal_options, locale, spoken_feedback_enabled);

  internal_options->SetClientControlEnabled(
      assistant::features::IsRoutinesEnabled());

  if (!features::IsVoiceMatchDisabled())
    internal_options->EnableRequireVoiceMatchVerification();

  assistant_manager_internal->SetOptions(*internal_options, [](bool success) {
    DVLOG(2) << "set options: " << success;
  });
}

const char* ToTriggerSource(AssistantEntryPoint entry_point) {
  switch (entry_point) {
    case AssistantEntryPoint::kUnspecified:
      return kEntryPointUnspecified;
    case AssistantEntryPoint::kDeepLink:
      return kEntryPointDeepLink;
    case AssistantEntryPoint::kHotkey:
      return kEntryPointHotkey;
    case AssistantEntryPoint::kHotword:
      return kEntryPointHotword;
    case AssistantEntryPoint::kLongPressLauncher:
      return kEntryPointLongPressLauncher;
    case AssistantEntryPoint::kSetup:
      return kEntryPointSetup;
    case AssistantEntryPoint::kStylus:
      return kEntryPointStylus;
    case AssistantEntryPoint::kLauncherSearchResult:
      return kEntryPointLauncherSearchResult;
    case AssistantEntryPoint::kLauncherSearchBoxIcon:
      return kEntryPointLauncherSearchBoxIcon;
    case AssistantEntryPoint::kProactiveSuggestions:
      return kEntryPointProactiveSuggestions;
    case AssistantEntryPoint::kLauncherChip:
      return kEntryPointLauncherChip;
    case AssistantEntryPoint::kBloom:
      return kEntryPointUnspecified;  // TODO(jeroendh): Use Bloom specific
                                      // entrypoint
  }
}

}  // namespace

AssistantManagerServiceImpl::AssistantManagerServiceImpl(
    ServiceContext* context,
    std::unique_ptr<AssistantManagerServiceDelegate> delegate,
    std::unique_ptr<network::PendingSharedURLLoaderFactory>
        pending_url_loader_factory,
    base::Optional<std::string> s3_server_uri_override)
    : media_session_(std::make_unique<AssistantMediaSession>(this)),
      action_module_(std::make_unique<action::CrosActionModule>(
          this,
          features::IsAppSupportEnabled(),
          features::IsWaitSchedulingEnabled())),
      chromium_api_delegate_(std::move(pending_url_loader_factory)),
      assistant_settings_(
          std::make_unique<AssistantSettingsImpl>(context, this)),
      context_(context),
      delegate_(std::move(delegate)),
      background_thread_("background thread"),
      libassistant_config_(CreateLibAssistantConfig(s3_server_uri_override)),
      weak_factory_(this) {
  background_thread_.Start();

  platform_api_ = delegate_->CreatePlatformApi(
      media_session_.get(), background_thread_.task_runner());

  settings_delegate_ =
      std::make_unique<AssistantDeviceSettingsDelegate>(context);

  mojo::Remote<media_session::mojom::MediaControllerManager>
      media_controller_manager;
  AssistantClient::Get()->RequestMediaControllerManager(
      media_controller_manager.BindNewPipeAndPassReceiver());
  media_controller_manager->CreateActiveMediaController(
      media_controller_.BindNewPipeAndPassReceiver());
}

AssistantManagerServiceImpl::~AssistantManagerServiceImpl() {
  background_thread_.Stop();
}

void AssistantManagerServiceImpl::Start(const base::Optional<UserInfo>& user,
                                        bool enable_hotword) {
  DCHECK(!assistant_manager_);
  DCHECK_EQ(GetState(), State::STOPPED);

  // Set the flag to avoid starting the service multiple times.
  SetStateAndInformObservers(State::STARTING);

  started_time_ = base::TimeTicks::Now();

  EnableHotword(enable_hotword);

  // Check the AmbientModeState to keep us synced on |ambient_state|.
  if (chromeos::features::IsAmbientModeEnabled()) {
    auto* model = ash::AmbientUiModel::Get();
    DCHECK(model);

    EnableAmbientMode(model->ui_visibility() !=
                      ash::AmbientUiVisibility::kClosed);
  }

  // LibAssistant creation will make file IO and sync wait. Post the creation to
  // background thread to avoid DCHECK.
  background_thread_.task_runner()->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&AssistantManagerServiceImpl::StartAssistantInternal,
                     base::Unretained(this), user,
                     assistant_state()->locale().value()),
      base::BindOnce(&AssistantManagerServiceImpl::PostInitAssistant,
                     weak_factory_.GetWeakPtr()));
}

void AssistantManagerServiceImpl::Stop() {
  // We cannot cleanly stop the service if it is in the process of starting up.
  DCHECK_NE(GetState(), State::STARTING);

  SetStateAndInformObservers(State::STOPPED);

  // When user disables the feature, we also deletes all data.
  if (!assistant_state()->settings_enabled().value() && assistant_manager_)
    assistant_manager_->ResetAllDataAndShutdown();

  media_controller_observer_receiver_.reset();

  assistant_manager_internal_ = nullptr;
  assistant_manager_.reset(nullptr);
  display_connection_.reset(nullptr);
}

AssistantManagerService::State AssistantManagerServiceImpl::GetState() const {
  return state_;
}

void AssistantManagerServiceImpl::SetUser(
    const base::Optional<UserInfo>& user) {
  if (!assistant_manager_)
    return;

  VLOG(1) << "Set user information (Gaia ID and access token).";
  assistant_manager_->SetAuthTokens(ToAuthTokensOrEmpty(user));
}

void AssistantManagerServiceImpl::EnableAmbientMode(bool enabled) {
  // Update |action_module_| accordingly, as some actions, e.g. open URL
  // in the browser, are not supported in ambient mode.
  action_module_->SetAmbientModeEnabled(enabled);
}

void AssistantManagerServiceImpl::RegisterFallbackMediaHandler() {
  // This is a callback from LibAssistant, it is async from LibAssistant thread.
  // It is possible that when it reaches here, the assistant_manager_ has
  // been stopped.
  if (!assistant_manager_internal_)
    return;

  // Register handler for media actions.
  assistant_manager_internal_->RegisterFallbackMediaHandler(
      [this](std::string action_name, std::string media_action_args_proto) {
        if (action_name == kPlayMediaClientOp) {
          OnPlayMedia(media_action_args_proto);
        } else {
          OnMediaControlAction(action_name, media_action_args_proto);
        }
      });
}

void AssistantManagerServiceImpl::WaitUntilStartIsFinishedForTesting() {
  // First we wait until |StartAssistantInternal| is finished.
  background_thread_.FlushForTesting();
  // Then we wait until |PostInitAssistant| finishes.
  // (which runs on the main thread).
  base::RunLoop().RunUntilIdle();
}

void AssistantManagerServiceImpl::AddMediaControllerObserver() {
  if (!features::IsMediaSessionIntegrationEnabled())
    return;

  if (media_controller_observer_receiver_.is_bound())
    return;

  media_controller_->AddObserver(
      media_controller_observer_receiver_.BindNewPipeAndPassRemote());
}

void AssistantManagerServiceImpl::RemoveMediaControllerObserver() {
  if (!features::IsMediaSessionIntegrationEnabled())
    return;

  if (!media_controller_observer_receiver_.is_bound())
    return;

  media_controller_observer_receiver_.reset();
}

void AssistantManagerServiceImpl::RegisterAlarmsTimersListener() {
  if (!assistant_manager_internal_)
    return;

  auto* alarm_timer_manager =
      assistant_manager_internal_->GetAlarmTimerManager();

  // Can be nullptr during unittests.
  if (!alarm_timer_manager)
    return;

  auto listener_callback = base::BindRepeating(
      [](scoped_refptr<base::SequencedTaskRunner> task_runner,
         base::RepeatingClosure task) {
        task_runner->PostTask(FROM_HERE, task);
      },
      main_task_runner(),
      base::BindRepeating(
          &AssistantManagerServiceImpl::OnAlarmTimerStateChanged,
          weak_factory_.GetWeakPtr()));

  // We always want to know when a timer has started ringing.
  alarm_timer_manager->RegisterRingingStateListener(
      [listener = listener_callback] { listener.Run(); });

  if (features::IsTimersV2Enabled()) {
    // In timers v2, we also want to know when timers are scheduled, updated,
    // and/or removed so that we can represent those states in UI.
    alarm_timer_manager->RegisterTimerActionListener(
        [listener = listener_callback](
            assistant_client::AlarmTimerManager::EventActionType ignore) {
          listener.Run();
        });

    // Force sync initial alarm/timer state.
    OnAlarmTimerStateChanged();
  }
}

void AssistantManagerServiceImpl::EnableListening(bool enable) {
  if (!assistant_manager_)
    return;
  assistant_manager_->EnableListening(enable);
}

void AssistantManagerServiceImpl::EnableHotword(bool enable) {
  platform_api_->OnHotwordEnabled(enable);
}

void AssistantManagerServiceImpl::SetArcPlayStoreEnabled(bool enable) {
  DCHECK(GetState() == State::RUNNING);
  // Both LibAssistant and Chrome threads may access |display_connection_|.
  // |display_connection_| is thread safe.
  if (assistant::features::IsAppSupportEnabled())
    display_connection_->SetArcPlayStoreEnabled(enable);
}

void AssistantManagerServiceImpl::SetAssistantContextEnabled(bool enable) {
  DCHECK(GetState() == State::RUNNING);

  if (enable) {
    AddMediaControllerObserver();
  } else {
    RemoveMediaControllerObserver();
    ResetMediaState();
  }

  // Both LibAssistant and Chrome threads may access |display_connection_|.
  // |display_connection_| is thread safe.
  display_connection_->SetAssistantContextEnabled(enable);
}

AssistantSettings* AssistantManagerServiceImpl::GetAssistantSettings() {
  return assistant_settings_.get();
}

void AssistantManagerServiceImpl::AddCommunicationErrorObserver(
    CommunicationErrorObserver* observer) {
  error_observers_.AddObserver(observer);
}

void AssistantManagerServiceImpl::RemoveCommunicationErrorObserver(
    const CommunicationErrorObserver* observer) {
  error_observers_.RemoveObserver(observer);
}

void AssistantManagerServiceImpl::AddAndFireStateObserver(
    StateObserver* observer) {
  state_observers_.AddObserver(observer);
  observer->OnStateChanged(GetState());
}

void AssistantManagerServiceImpl::RemoveStateObserver(
    const StateObserver* observer) {
  state_observers_.RemoveObserver(observer);
}

void AssistantManagerServiceImpl::SyncDeviceAppsStatus() {
  assistant_settings_->SyncDeviceAppsStatus(
      base::BindOnce(&AssistantManagerServiceImpl::OnDeviceAppsEnabled,
                     weak_factory_.GetWeakPtr()));
}

void AssistantManagerServiceImpl::UpdateInternalMediaPlayerStatus(
    MediaSessionAction action) {
  if (!assistant_manager_)
    return;
  auto* media_manager = assistant_manager_->GetMediaManager();
  if (!media_manager)
    return;

  switch (action) {
    case MediaSessionAction::kPause:
      media_manager->Pause();
      break;
    case MediaSessionAction::kPlay:
      media_manager->Resume();
      break;
    case MediaSessionAction::kPreviousTrack:
    case MediaSessionAction::kNextTrack:
    case MediaSessionAction::kSeekBackward:
    case MediaSessionAction::kSeekForward:
    case MediaSessionAction::kSkipAd:
    case MediaSessionAction::kStop:
    case MediaSessionAction::kSeekTo:
    case MediaSessionAction::kScrubTo:
    case MediaSessionAction::kEnterPictureInPicture:
    case MediaSessionAction::kExitPictureInPicture:
    case MediaSessionAction::kSwitchAudioDevice:
      NOTIMPLEMENTED();
      break;
  }
}

void AssistantManagerServiceImpl::StartVoiceInteraction() {
  DCHECK(assistant_manager_);
  DVLOG(1) << __func__;

  platform_api_->SetMicState(true);
  assistant_manager_->StartAssistantInteraction();
}

void AssistantManagerServiceImpl::StopActiveInteraction(
    bool cancel_conversation) {
  DVLOG(1) << __func__;
  platform_api_->SetMicState(false);

  if (!assistant_manager_internal_) {
    VLOG(1) << "Stopping interaction without assistant manager.";
    return;
  }
  assistant_manager_internal_->StopAssistantInteractionInternal(
      cancel_conversation);
}

void AssistantManagerServiceImpl::StartEditReminderInteraction(
    const std::string& client_id) {
  SendVoicelessInteraction(CreateEditReminderInteraction(client_id),
                           /*description=*/std::string(),
                           /*is_user_initiated=*/true);
}

void AssistantManagerServiceImpl::StartScreenContextInteraction(
    ax::mojom::AssistantStructurePtr assistant_structure,
    const std::vector<uint8_t>& assistant_screenshot) {
  std::vector<std::string> context_protos;

  // Screen context can have the |assistant_structure|, or |assistant_extra| and
  // |assistant_tree| set to nullptr. This happens in the case where the screen
  // context is coming from the metalayer or there is no active window. For this
  // scenario, we don't create a context proto for the AssistantBundle that
  // consists of the |assistant_extra| and |assistant_tree|.
  if (assistant_structure && assistant_structure->assistant_extra &&
      assistant_structure->assistant_tree) {
    // Note: the value of |is_first_query| for screen context query is a no-op
    // because it is not used for metalayer and "What's on my screen" queries.
    context_protos.emplace_back(CreateContextProto(
        AssistantBundle{assistant_structure->assistant_extra.get(),
                        assistant_structure->assistant_tree.get()},
        /*is_first_query=*/true));
  }

  // Note: the value of |is_first_query| for screen context query is a no-op.
  context_protos.emplace_back(CreateContextProto(assistant_screenshot,
                                                 /*is_first_query=*/true));
  assistant_manager_internal_->SendScreenContextRequest(context_protos);
}

void AssistantManagerServiceImpl::StartTextInteraction(
    const std::string& query,
    AssistantQuerySource source,
    bool allow_tts) {
  DVLOG(1) << __func__;
  assistant_client::VoicelessOptions options;
  options.is_user_initiated = true;
  options.enable_on_device_assistant_for_voiceless =
      assistant::features::IsOnDeviceAssistantEnabled();

  if (!allow_tts) {
    options.modality =
        assistant_client::VoicelessOptions::Modality::TYPING_MODALITY;
  }

  // Cache metadata about this interaction that can be resolved when the
  // associated conversation turn starts in LibAssistant.
  options.conversation_turn_id =
      NewPendingInteraction(AssistantInteractionType::kText, source, query);

  std::string interaction = CreateTextQueryInteraction(query);
  assistant_manager_internal_->SendVoicelessInteraction(
      interaction, /*description=*/"text_query", options, [](auto) {});
}

void AssistantManagerServiceImpl::AddAssistantInteractionSubscriber(
    AssistantInteractionSubscriber* subscriber) {
  interaction_subscribers_.AddObserver(subscriber);
}

void AssistantManagerServiceImpl::RemoveAssistantInteractionSubscriber(
    AssistantInteractionSubscriber* subscriber) {
  interaction_subscribers_.RemoveObserver(subscriber);
}

void AssistantManagerServiceImpl::RetrieveNotification(
    const AssistantNotification& notification,
    int action_index) {
  const std::string& notification_id = notification.server_id;
  const std::string& consistency_token = notification.consistency_token;
  const std::string& opaque_token = notification.opaque_token;

  const std::string request_interaction =
      SerializeNotificationRequestInteraction(
          notification_id, consistency_token, opaque_token, action_index);

  SendVoicelessInteraction(request_interaction,
                           /*description=*/"RequestNotification",
                           /*is_user_initiated=*/true);
}

void AssistantManagerServiceImpl::DismissNotification(
    const AssistantNotification& notification) {
  // |assistant_manager_internal_| may not exist if we are dismissing
  // notifications as part of a shutdown sequence.
  if (!assistant_manager_internal_)
    return;

  const std::string& notification_id = notification.server_id;
  const std::string& consistency_token = notification.consistency_token;
  const std::string& opaque_token = notification.opaque_token;
  const std::string& grouping_key = notification.grouping_key;

  const std::string dismissed_interaction =
      SerializeNotificationDismissedInteraction(
          notification_id, consistency_token, opaque_token, {grouping_key});

  assistant_client::VoicelessOptions options;
  options.obfuscated_gaia_id = notification.obfuscated_gaia_id;

  assistant_manager_internal_->SendVoicelessInteraction(
      dismissed_interaction, /*description=*/"DismissNotification", options,
      [](auto) {});
}

void AssistantManagerServiceImpl::OnConversationTurnStartedInternal(
    const assistant_client::ConversationTurnMetadata& metadata) {
  ENSURE_MAIN_THREAD(
      &AssistantManagerServiceImpl::OnConversationTurnStartedInternal,
      metadata);

  platform_api_->OnConversationTurnStarted();

  // Retrieve the cached interaction metadata associated with this conversation
  // turn or construct a new instance if there's no match in the cache.
  std::unique_ptr<AssistantInteractionMetadata> metadata_ptr;
  auto it = pending_interactions_.find(metadata.id);
  if (it != pending_interactions_.end()) {
    metadata_ptr = std::move(it->second);
    pending_interactions_.erase(it);
  } else {
    metadata_ptr = std::make_unique<AssistantInteractionMetadata>();
    metadata_ptr->type = metadata.is_mic_open ? AssistantInteractionType::kVoice
                                              : AssistantInteractionType::kText;
    metadata_ptr->source = AssistantQuerySource::kLibAssistantInitiated;
  }

  for (auto& it : interaction_subscribers_)
    it.OnInteractionStarted(*metadata_ptr);
}

void AssistantManagerServiceImpl::OnConversationTurnFinished(
    Resolution resolution) {
  ENSURE_MAIN_THREAD(&AssistantManagerServiceImpl::OnConversationTurnFinished,
                     resolution);

  // TODO(updowndota): Find a better way to handle the edge cases.
  if (resolution != Resolution::NORMAL_WITH_FOLLOW_ON &&
      resolution != Resolution::CANCELLED &&
      resolution != Resolution::BARGE_IN) {
    platform_api_->SetMicState(false);
  }

  platform_api_->OnConversationTurnFinished();

  switch (resolution) {
    // Interaction ended normally.
    case Resolution::NORMAL:
    case Resolution::NORMAL_WITH_FOLLOW_ON:
    case Resolution::NO_RESPONSE:
      for (auto& it : interaction_subscribers_)
        it.OnInteractionFinished(AssistantInteractionResolution::kNormal);

      RecordQueryResponseTypeUMA();
      break;
    // Interaction ended due to interruption.
    case Resolution::BARGE_IN:
    case Resolution::CANCELLED:
      for (auto& it : interaction_subscribers_)
        it.OnInteractionFinished(AssistantInteractionResolution::kInterruption);

      if (receive_inline_response_ || receive_modify_settings_proto_response_ ||
          !receive_url_response_.empty()) {
        RecordQueryResponseTypeUMA();
      }
      break;
    // Interaction ended due to mic timeout.
    case Resolution::TIMEOUT:
      for (auto& it : interaction_subscribers_)
        it.OnInteractionFinished(AssistantInteractionResolution::kMicTimeout);
      break;
    // Interaction ended due to error.
    case Resolution::COMMUNICATION_ERROR:
      for (auto& it : interaction_subscribers_)
        it.OnInteractionFinished(AssistantInteractionResolution::kError);
      break;
    // Interaction ended because the device was not selected to produce a
    // response. This occurs due to multi-device hotword loss.
    case Resolution::DEVICE_NOT_SELECTED:
      for (auto& it : interaction_subscribers_) {
        it.OnInteractionFinished(
            AssistantInteractionResolution::kMultiDeviceHotwordLoss);
      }
      break;
    // This is only applicable in longform barge-in mode, which we do not use.
    case Resolution::LONGFORM_KEEP_MIC_OPEN:
      NOTREACHED();
      break;
  }
}

void AssistantManagerServiceImpl::OnScheduleWait(int id, int time_ms) {
  ENSURE_MAIN_THREAD(&AssistantManagerServiceImpl::OnScheduleWait, id, time_ms);
  DCHECK(features::IsWaitSchedulingEnabled());

  // Schedule a wait for |time_ms|, notifying the CrosActionModule when the wait
  // has finished so that it can inform LibAssistant to resume execution.
  main_task_runner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          [](const base::WeakPtr<AssistantManagerServiceImpl>& weak_ptr,
             int id) {
            if (weak_ptr) {
              weak_ptr->action_module_->OnScheduledWaitDone(
                  id, /*cancelled=*/false);
            }
          },
          weak_factory_.GetWeakPtr(), id),
      base::TimeDelta::FromMilliseconds(time_ms));

  // Notify subscribers that a wait has been started.
  for (auto& it : interaction_subscribers_)
    it.OnWaitStarted();
}

// TODO(b/113541754): Deprecate this API when the server provides a fallback.
void AssistantManagerServiceImpl::OnShowContextualQueryFallback() {
  // Show fallback text.
  OnShowText(l10n_util::GetStringUTF8(
      IDS_ASSISTANT_SCREEN_CONTEXT_QUERY_FALLBACK_TEXT));
}

void AssistantManagerServiceImpl::OnShowHtml(const std::string& html,
                                             const std::string& fallback) {
  ENSURE_MAIN_THREAD(&AssistantManagerServiceImpl::OnShowHtml, html, fallback);

  receive_inline_response_ = true;

  for (auto& it : interaction_subscribers_)
    it.OnHtmlResponse(html, fallback);
}

void AssistantManagerServiceImpl::OnShowSuggestions(
    const std::vector<action::Suggestion>& suggestions) {
  ENSURE_MAIN_THREAD(&AssistantManagerServiceImpl::OnShowSuggestions,
                     suggestions);

  std::vector<AssistantSuggestion> result;
  for (const action::Suggestion& suggestion : suggestions) {
    AssistantSuggestion assistant_suggestion;
    assistant_suggestion.id = base::UnguessableToken::Create();
    assistant_suggestion.text = suggestion.text;
    assistant_suggestion.icon_url = GURL(suggestion.icon_url);
    assistant_suggestion.action_url = GURL(suggestion.action_url);
    result.push_back(std::move(assistant_suggestion));
  }

  for (auto& it : interaction_subscribers_)
    it.OnSuggestionsResponse(result);
}

void AssistantManagerServiceImpl::OnShowText(const std::string& text) {
  ENSURE_MAIN_THREAD(&AssistantManagerServiceImpl::OnShowText, text);

  receive_inline_response_ = true;

  for (auto& it : interaction_subscribers_)
    it.OnTextResponse(text);
}

void AssistantManagerServiceImpl::OnOpenUrl(const std::string& url,
                                            bool is_background) {
  ENSURE_MAIN_THREAD(&AssistantManagerServiceImpl::OnOpenUrl, url,
                     is_background);

  receive_url_response_ = url;
  const GURL gurl = GURL(url);

  for (auto& it : interaction_subscribers_)
    it.OnOpenUrlResponse(gurl, is_background);
}

void AssistantManagerServiceImpl::OnShowNotification(
    const action::Notification& notification) {
  ENSURE_MAIN_THREAD(&AssistantManagerServiceImpl::OnShowNotification,
                     notification);

  AssistantNotification assistant_notification;
  assistant_notification.title = notification.title;
  assistant_notification.message = notification.text;
  assistant_notification.action_url = GURL(notification.action_url);
  assistant_notification.client_id = notification.notification_id;
  assistant_notification.server_id = notification.notification_id;
  assistant_notification.consistency_token = notification.consistency_token;
  assistant_notification.opaque_token = notification.opaque_token;
  assistant_notification.grouping_key = notification.grouping_key;
  assistant_notification.obfuscated_gaia_id = notification.obfuscated_gaia_id;
  assistant_notification.from_server = true;

  if (notification.expiry_timestamp_ms) {
    assistant_notification.expiry_time =
        base::Time::FromJavaTime(notification.expiry_timestamp_ms);
  }

  // The server sometimes sends an empty |notification_id|, but our client
  // requires a non-empty |client_id| for notifications. Known instances in
  // which the server sends an empty |notification_id| are for Reminders.
  if (assistant_notification.client_id.empty()) {
    assistant_notification.client_id =
        base::UnguessableToken::Create().ToString();
  }

  for (const auto& button : notification.buttons) {
    assistant_notification.buttons.push_back(
        {button.label, GURL(button.action_url),
         /*remove_notification_on_click=*/true});
  }

  assistant_notification_controller()->AddOrUpdateNotification(
      std::move(assistant_notification));
}

void AssistantManagerServiceImpl::OnOpenAndroidApp(
    const AndroidAppInfo& app_info,
    const InteractionInfo& interaction) {
  ENSURE_MAIN_THREAD(&AssistantManagerServiceImpl::OnOpenAndroidApp, app_info,
                     interaction);
  bool success = false;
  for (auto& it : interaction_subscribers_)
    success |= it.OnOpenAppResponse(app_info);

  std::string interaction_proto = CreateOpenProviderResponseInteraction(
      interaction.interaction_id, success);
  assistant_client::VoicelessOptions options;
  options.obfuscated_gaia_id = interaction.user_id;

  assistant_manager_internal_->SendVoicelessInteraction(
      interaction_proto, /*description=*/"open_provider_response", options,
      [](auto) {});
}

void AssistantManagerServiceImpl::OnVerifyAndroidApp(
    const std::vector<AndroidAppInfo>& apps_info,
    const InteractionInfo& interaction) {
  ENSURE_MAIN_THREAD(&AssistantManagerServiceImpl::OnVerifyAndroidApp,
                     apps_info, interaction);
  std::vector<AndroidAppInfo> result_apps_info;
  for (auto& app_info : apps_info) {
    AndroidAppInfo result_app_info(app_info);
    AppStatus status = device_actions()->GetAndroidAppStatus(app_info);
    result_app_info.status = status;
    result_apps_info.emplace_back(result_app_info);
  }
  std::string interaction_proto = CreateVerifyProviderResponseInteraction(
      interaction.interaction_id, result_apps_info);

  assistant_client::VoicelessOptions options;
  options.obfuscated_gaia_id = interaction.user_id;
  // Set the request to be user initiated so that a new conversation will be
  // created to handle the client OPs in the response of this request.
  options.is_user_initiated = true;

  assistant_manager_internal_->SendVoicelessInteraction(
      interaction_proto, /*description=*/"verify_provider_response", options,
      [](auto) {});
}

void AssistantManagerServiceImpl::OnOpenMediaAndroidIntent(
    const std::string& play_media_args_proto,
    AndroidAppInfo* app_info) {
  DCHECK(main_task_runner()->RunsTasksInCurrentSequence());

  // Handle android media playback intent.
  app_info->action = kIntentActionView;
  if (app_info->intent.empty()) {
    std::string url = GetAndroidIntentUrlFromMediaArgs(play_media_args_proto);
    if (!url.empty())
      app_info->intent = url;
  }
  for (auto& it : interaction_subscribers_) {
    bool success = it.OnOpenAppResponse(*app_info);
    HandleLaunchMediaIntentResponse(success);
  }
}

void AssistantManagerServiceImpl::OnPlayMedia(
    const std::string& play_media_args_proto) {
  ENSURE_MAIN_THREAD(&AssistantManagerServiceImpl::OnPlayMedia,
                     play_media_args_proto);

  std::unique_ptr<AndroidAppInfo> app_info =
      GetAppInfoFromMediaArgs(play_media_args_proto);
  if (app_info) {
    OnOpenMediaAndroidIntent(play_media_args_proto, app_info.get());
  } else {
    std::string url = GetWebUrlFromMediaArgs(play_media_args_proto);
    // Fallack to web URL.
    if (!url.empty())
      OnOpenUrl(url, /*in_background=*/false);
  }
}

void AssistantManagerServiceImpl::OnMediaControlAction(
    const std::string& action_name,
    const std::string& media_action_args_proto) {
  ENSURE_MAIN_THREAD(&AssistantManagerServiceImpl::OnMediaControlAction,
                     action_name, media_action_args_proto);

  if (action_name == kPauseTrackClientOp) {
    media_controller_->Suspend();
    return;
  }

  if (action_name == kResumeTrackClientOp) {
    media_controller_->Resume();
    return;
  }

  if (action_name == kNextTrackClientOp) {
    media_controller_->NextTrack();
    return;
  }

  if (action_name == kPrevTrackClientOp) {
    media_controller_->PreviousTrack();
    return;
  }

  if (action_name == kStopTrackClientOp) {
    media_controller_->Suspend();
    return;
  }
  // TODO(llin): Handle media.SEEK_RELATIVE.
}

void AssistantManagerServiceImpl::OnRecognitionStateChanged(
    assistant_client::ConversationStateListener::RecognitionState state,
    const assistant_client::ConversationStateListener::RecognitionResult&
        recognition_result) {
  ENSURE_MAIN_THREAD(&AssistantManagerServiceImpl::OnRecognitionStateChanged,
                     state, recognition_result);

  switch (state) {
    case assistant_client::ConversationStateListener::RecognitionState::STARTED:
      for (auto& it : interaction_subscribers_)
        it.OnSpeechRecognitionStarted();
      break;
    case assistant_client::ConversationStateListener::RecognitionState::
        INTERMEDIATE_RESULT:
      for (auto& it : interaction_subscribers_) {
        it.OnSpeechRecognitionIntermediateResult(
            recognition_result.high_confidence_text,
            recognition_result.low_confidence_text);
      }
      break;
    case assistant_client::ConversationStateListener::RecognitionState::
        END_OF_UTTERANCE:
      for (auto& it : interaction_subscribers_)
        it.OnSpeechRecognitionEndOfUtterance();
      break;
    case assistant_client::ConversationStateListener::RecognitionState::
        FINAL_RESULT:
      for (auto& it : interaction_subscribers_) {
        it.OnSpeechRecognitionFinalResult(recognition_result.recognized_speech);
      }
      break;
  }
}

void AssistantManagerServiceImpl::OnRespondingStarted(bool is_error_response) {
  ENSURE_MAIN_THREAD(&AssistantManagerServiceImpl::OnRespondingStarted,
                     is_error_response);

  for (auto& it : interaction_subscribers_)
    it.OnTtsStarted(is_error_response);
}

void AssistantManagerServiceImpl::OnSpeechLevelUpdated(
    const float speech_level) {
  ENSURE_MAIN_THREAD(&AssistantManagerServiceImpl::OnSpeechLevelUpdated,
                     speech_level);

  for (auto& it : interaction_subscribers_)
    it.OnSpeechLevelUpdated(speech_level);
}

void AssistantManagerServiceImpl::OnModifyDeviceSetting(
    const api::client_op::ModifySettingArgs& modify_setting_args) {
  ENSURE_MAIN_THREAD(&AssistantManagerServiceImpl::OnModifyDeviceSetting,
                     modify_setting_args);
  receive_modify_settings_proto_response_ = true;

  settings_delegate_->HandleModifyDeviceSetting(modify_setting_args);
}

void AssistantManagerServiceImpl::OnGetDeviceSettings(
    int interaction_id,
    const api::client_op::GetDeviceSettingsArgs& args) {
  std::vector<DeviceSetting> result =
      settings_delegate_->GetDeviceSettings(args);

  SendVoicelessInteraction(
      CreateGetDeviceSettingInteraction(interaction_id, result),
      /*description=*/"get_settings_result",
      /*is_user_initiated=*/true);
}

bool AssistantManagerServiceImpl::IsSettingSupported(
    const std::string& setting_id) {
  DVLOG(2) << "IsSettingSupported=" << setting_id;

  return settings_delegate_->IsSettingSupported(setting_id);
}

bool AssistantManagerServiceImpl::SupportsModifySettings() {
  return true;
}

void AssistantManagerServiceImpl::OnNotificationRemoved(
    const std::string& grouping_key) {
  ENSURE_MAIN_THREAD(&AssistantManagerServiceImpl::OnNotificationRemoved,
                     grouping_key);

  if (grouping_key.empty()) {
    assistant_notification_controller()->RemoveAllNotifications(
        /*from_server=*/true);
  } else {
    assistant_notification_controller()->RemoveNotificationByGroupingKey(
        grouping_key, /*from_server=*/
        true);
  }
}

void AssistantManagerServiceImpl::OnCommunicationError(int error_code) {
  ENSURE_MAIN_THREAD(&AssistantManagerServiceImpl::OnCommunicationError,
                     error_code);

  CommunicationErrorType type =
      CommunicationErrorTypeFromLibassistantErrorCode(error_code);

  for (auto& observer : error_observers_)
    observer.OnCommunicationError(type);
}

void AssistantManagerServiceImpl::StartAssistantInternal(
    const base::Optional<UserInfo>& user,
    const std::string& locale) {
  DCHECK(background_thread_.task_runner()->BelongsToCurrentThread());
  // NOTE: as the above showed, this is running in a different thread, we can
  // only access |new_xxx| members here.
  base::AutoLock lock(new_assistant_manager_lock_);
  // There can only be one |AssistantManager| instance at any given time.
  DCHECK(!assistant_manager_);
  new_display_connection_ = std::make_unique<CrosDisplayConnection>(
      this, /*feedback_ui_enabled=*/true,
      assistant::features::IsMediaSessionIntegrationEnabled());

  new_assistant_manager_ = delegate_->CreateAssistantManager(
      platform_api_.get(), libassistant_config_);
  new_assistant_manager_internal_ =
      delegate_->UnwrapAssistantManagerInternal(new_assistant_manager_.get());

  UpdateInternalOptions(new_assistant_manager_internal_, locale,
                        spoken_feedback_enabled_);

  new_assistant_manager_internal_->SetLocaleOverride(
      GetLocaleOrDefault(assistant_state()->locale().value()));
  new_assistant_manager_internal_->SetDisplayConnection(
      new_display_connection_.get());
  new_assistant_manager_internal_->RegisterActionModule(action_module_.get());
  new_assistant_manager_internal_->SetAssistantManagerDelegate(this);
  new_assistant_manager_internal_->GetFuchsiaApiHelperOrDie()
      ->SetFuchsiaApiDelegate(&chromium_api_delegate_);
  new_assistant_manager_->AddConversationStateListener(this);
  new_assistant_manager_->AddDeviceStateListener(this);

  std::vector<std::string> server_experiment_ids;
  FillServerExperimentIds(&server_experiment_ids);

  if (server_experiment_ids.size() > 0) {
    new_assistant_manager_internal_->AddExtraExperimentIds(
        server_experiment_ids);
  }

  // When |access_token| does not contain a value, we will start Libassistant
  // in signed-out mode by calling SetAuthTokens() with an empty vector.
  new_assistant_manager_->SetAuthTokens(ToAuthTokensOrEmpty(user));
  new_assistant_manager_->Start();
}

void AssistantManagerServiceImpl::PostInitAssistant() {
  DCHECK(main_task_runner()->RunsTasksInCurrentSequence());
  DCHECK_EQ(GetState(), State::STARTING);

  {
    base::AutoLock lock(new_assistant_manager_lock_);

    // It is possible that multiple |StartAssistantInternal| finished on
    // background thread, before any of the matching |PostInitAssistant| had
    // run. Because we only hold the last created instance in
    // |new_assistant_manager_|, it is possible that |new_assistant_manager_| be
    // null if we moved it in previous |PostInitAssistant| runs.
    if (!new_assistant_manager_) {
      return;
    }

    display_connection_ = std::move(new_display_connection_);
    assistant_manager_ = std::move(new_assistant_manager_);
    assistant_manager_internal_ = new_assistant_manager_internal_;
    new_assistant_manager_internal_ = nullptr;
  }

  const base::TimeDelta time_since_started =
      base::TimeTicks::Now() - started_time_;
  UMA_HISTOGRAM_TIMES("Assistant.ServiceStartTime", time_since_started);

  SetStateAndInformObservers(State::STARTED);

  assistant_settings_->UpdateServerDeviceSettings();

  if (base::FeatureList::IsEnabled(assistant::features::kAssistantAppSupport) &&
      !scoped_app_list_event_subscriber_.IsObservingSources()) {
    scoped_app_list_event_subscriber_.Add(device_actions());
  }
}

void AssistantManagerServiceImpl::HandleLaunchMediaIntentResponse(
    bool app_opened) {
  // TODO(llin): Handle the response.
  NOTIMPLEMENTED();
}

// This method runs on the LibAssistant thread.
// This method is triggered as the callback of libassistant bootup checkin.
void AssistantManagerServiceImpl::OnStartFinished() {
  ENSURE_MAIN_THREAD(&AssistantManagerServiceImpl::OnStartFinished);

  // It is possible the |assistant_manager_| was destructed before the
  // rescheduled main thread task got a chance to run. We check this and also
  // try to avoid double run by checking |GetState()|.
  if (!assistant_manager_ || (GetState() == State::RUNNING))
    return;

  SetStateAndInformObservers(State::RUNNING);

  if (is_first_init) {
    is_first_init = false;
    // Only sync status at the first init to prevent unexpected corner cases.
    if (assistant_state()->hotword_enabled().value())
      assistant_settings_->SyncSpeakerIdEnrollmentStatus();
  }

  const base::TimeDelta time_since_started =
      base::TimeTicks::Now() - started_time_;
  UMA_HISTOGRAM_TIMES("Assistant.ServiceReadyTime", time_since_started);

  SyncDeviceAppsStatus();

  RegisterFallbackMediaHandler();

  SetAssistantContextEnabled(assistant_state()->IsScreenContextAllowed());

  auto* media_manager = assistant_manager_->GetMediaManager();
  if (media_manager)
    media_manager->AddListener(this);

  RegisterAlarmsTimersListener();

  if (assistant_state()->arc_play_store_enabled().has_value())
    SetArcPlayStoreEnabled(assistant_state()->arc_play_store_enabled().value());
}

void AssistantManagerServiceImpl::OnAndroidAppListRefreshed(
    const std::vector<AndroidAppInfo>& apps_info) {
  std::vector<AndroidAppInfo> filtered_apps_info;
  for (const auto& app_info : apps_info) {
    // TODO(b/146355799): Remove the special handling for Android settings app.
    if (app_info.package_name == kAndroidSettingsAppPackage)
      continue;

    filtered_apps_info.emplace_back(app_info);
  }
  display_connection_->OnAndroidAppListRefreshed(filtered_apps_info);
}

void AssistantManagerServiceImpl::OnPlaybackStateChange(
    const MediaStatus& status) {
  media_session_->NotifyMediaSessionMetadataChanged(status);
}

void AssistantManagerServiceImpl::MediaSessionInfoChanged(
    MediaSessionInfoPtr info) {
  media_session_info_ptr_ = std::move(info);
  UpdateMediaState();
}

void AssistantManagerServiceImpl::MediaSessionMetadataChanged(
    const base::Optional<media_session::MediaMetadata>& metadata) {
  media_metadata_ = std::move(metadata);
  UpdateMediaState();
}

void AssistantManagerServiceImpl::MediaSessionChanged(
    const base::Optional<base::UnguessableToken>& request_id) {
  if (request_id.has_value())
    media_session_audio_focus_id_ = std::move(request_id.value());
}

// TODO(dmblack): Handle non-firing (e.g. paused or scheduled) timers.
void AssistantManagerServiceImpl::OnAlarmTimerStateChanged() {
  ENSURE_MAIN_THREAD(&AssistantManagerServiceImpl::OnAlarmTimerStateChanged);

  // |assistant_manager_internal_| may not exist if we are receiving this event
  // as part of a shutdown sequence. When this occurs, we notify our alarm/timer
  // controller to clear its cache to remain in sync with LibAssistant.
  if (!assistant_manager_internal_) {
    assistant_alarm_timer_controller()->OnTimerStateChanged({});
    return;
  }

  std::vector<ash::AssistantTimerPtr> timers;

  auto* manager = assistant_manager_internal_->GetAlarmTimerManager();
  for (const auto& event : manager->GetAllEvents()) {
    // Note that we currently only handle timers, alarms are unsupported.
    if (event.type != assistant_client::AlarmTimerEvent::TIMER)
      continue;

    // We always handle timers that have fired. Only for timers v2, however, do
    // we handle scheduled/paused timers so we can represent those states in UI.
    if (event.timer_data.state != assistant_client::Timer::State::FIRED &&
        !features::IsTimersV2Enabled()) {
      continue;
    }

    auto timer = std::make_unique<ash::AssistantTimer>();
    timer->id = event.timer_data.timer_id;
    timer->label = event.timer_data.label;
    timer->state = GetTimerState(event.timer_data.state);
    timer->original_duration = base::TimeDelta::FromMilliseconds(
        event.timer_data.original_duration_ms);

    // LibAssistant provides |fire_time_ms| as an offset from unix epoch.
    timer->fire_time =
        base::Time::UnixEpoch() +
        base::TimeDelta::FromMilliseconds(event.timer_data.fire_time_ms);

    // If the |timer| is paused, LibAssistant will specify the amount of time
    // remaining. Otherwise we calculate it based on |fire_time|.
    timer->remaining_time = timer->state == ash::AssistantTimerState::kPaused
                                ? base::TimeDelta::FromMilliseconds(
                                      event.timer_data.remaining_duration_ms)
                                : timer->fire_time - base::Time::Now();

    timers.push_back(std::move(timer));
  }

  assistant_alarm_timer_controller()->OnTimerStateChanged(std::move(timers));
}

void AssistantManagerServiceImpl::OnAccessibilityStatusChanged(
    bool spoken_feedback_enabled) {
  if (spoken_feedback_enabled_ == spoken_feedback_enabled)
    return;

  spoken_feedback_enabled_ = spoken_feedback_enabled;

  // When |spoken_feedback_enabled_| changes we need to update our internal
  // options to turn on/off A11Y features in LibAssistant.
  if (assistant_manager_internal_) {
    UpdateInternalOptions(assistant_manager_internal_,
                          assistant_state()->locale().value(),
                          spoken_feedback_enabled_);
  }
}

void AssistantManagerServiceImpl::OnDeviceAppsEnabled(bool enabled) {
  // The device apps state sync should only be sent after service is running.
  // Check state here to prevent timing issue when the service is restarting.
  if (GetState() != State::RUNNING)
    return;

  display_connection_->SetDeviceAppsEnabled(enabled);
  action_module_->SetAppSupportEnabled(
      assistant::features::IsAppSupportEnabled() && enabled);
}

void AssistantManagerServiceImpl::AddTimeToTimer(const std::string& id,
                                                 base::TimeDelta duration) {
  if (!assistant_manager_internal_)
    return;

  assistant_manager_internal_->GetAlarmTimerManager()->AddTimeToTimer(
      id, duration.InSeconds());
}

void AssistantManagerServiceImpl::PauseTimer(const std::string& id) {
  if (assistant_manager_internal_)
    assistant_manager_internal_->GetAlarmTimerManager()->PauseTimer(id);
}

void AssistantManagerServiceImpl::RemoveAlarmOrTimer(const std::string& id) {
  if (assistant_manager_internal_)
    assistant_manager_internal_->GetAlarmTimerManager()->RemoveEvent(id);
}

void AssistantManagerServiceImpl::ResumeTimer(const std::string& id) {
  if (assistant_manager_internal_)
    assistant_manager_internal_->GetAlarmTimerManager()->ResumeTimer(id);
}

void AssistantManagerServiceImpl::NotifyEntryIntoAssistantUi(
    AssistantEntryPoint entry_point) {
  base::AutoLock lock(last_trigger_source_lock_);
  last_trigger_source_ = ToTriggerSource(entry_point);
}

std::string AssistantManagerServiceImpl::ConsumeLastTriggerSource() {
  base::AutoLock lock(last_trigger_source_lock_);
  auto trigger_source = last_trigger_source_;
  last_trigger_source_ = std::string();
  return trigger_source;
}

void AssistantManagerServiceImpl::SendVoicelessInteraction(
    const std::string& interaction,
    const std::string& description,
    bool is_user_initiated) {
  assistant_client::VoicelessOptions voiceless_options;

  voiceless_options.is_user_initiated = is_user_initiated;

  assistant_manager_internal_->SendVoicelessInteraction(
      interaction, description, voiceless_options, [](auto) {});
}

std::string AssistantManagerServiceImpl::GetLastSearchSource() {
  return ConsumeLastTriggerSource();
}

void AssistantManagerServiceImpl::FillServerExperimentIds(
    std::vector<std::string>* server_experiment_ids) {
  if (base::FeatureList::IsEnabled(kChromeOSAssistantDogfood)) {
    server_experiment_ids->emplace_back(kServersideDogfoodExperimentId);
  }

  if (base::FeatureList::IsEnabled(features::kAssistantAppSupport))
    server_experiment_ids->emplace_back(kServersideOpenAppExperimentId);

  if (features::IsResponseProcessingV2Enabled()) {
    server_experiment_ids->emplace_back(
        kServersideResponseProcessingV2ExperimentId);
  }
}

void AssistantManagerServiceImpl::RecordQueryResponseTypeUMA() {
  auto response_type = AssistantQueryResponseType::kUnspecified;

  if (receive_modify_settings_proto_response_) {
    response_type = AssistantQueryResponseType::kDeviceAction;
  } else if (!receive_url_response_.empty()) {
    if (receive_url_response_.find("www.google.com/search?") !=
        std::string::npos) {
      response_type = AssistantQueryResponseType::kSearchFallback;
    } else {
      response_type = AssistantQueryResponseType::kTargetedAction;
    }
  } else if (receive_inline_response_) {
    response_type = AssistantQueryResponseType::kInlineElement;
  }

  UMA_HISTOGRAM_ENUMERATION("Assistant.QueryResponseType", response_type);

  // Reset the flags.
  receive_inline_response_ = false;
  receive_modify_settings_proto_response_ = false;
  receive_url_response_.clear();
}

void AssistantManagerServiceImpl::SendAssistantFeedback(
    const AssistantFeedback& assistant_feedback) {
  const std::string interaction = CreateSendFeedbackInteraction(
      assistant_feedback.assistant_debug_info_allowed,
      assistant_feedback.description, assistant_feedback.screenshot_png);

  SendVoicelessInteraction(interaction,
                           /*description=*/"send feedback with details",
                           /*is_user_initiated=*/false);
}

void AssistantManagerServiceImpl::UpdateMediaState() {
  if (media_session_info_ptr_) {
    if (media_session_info_ptr_->is_sensitive) {
      // Do not update media state if the session is considered to be sensitive
      // (off the record profile).
      return;
    }

    if (media_session_info_ptr_->state ==
            MediaSessionInfo::SessionState::kSuspended &&
        media_session_info_ptr_->playback_state ==
            media_session::mojom::MediaPlaybackState::kPlaying) {
      // It is an intermediate state caused by some providers override the
      // playback state. We considered it as invalid and skip reporting the
      // state.
      return;
    }
  }

  // MediaSession Integrated providers (include the libassistant internal
  // media provider) will trigger media state change event. Only update the
  // external media status if the state changes is triggered by external
  // providers.
  if (media_session_->internal_audio_focus_id() ==
      media_session_audio_focus_id_) {
    return;
  }

  MediaStatus media_status;

  // Set media metadata.
  if (media_metadata_.has_value()) {
    media_status.metadata.title =
        base::UTF16ToUTF8(media_metadata_.value().title);
  }

  // Set playback state.
  media_status.playback_state = MediaStatus::IDLE;
  if (media_session_info_ptr_ &&
      media_session_info_ptr_->state !=
          MediaSessionInfo::SessionState::kInactive) {
    switch (media_session_info_ptr_->playback_state) {
      case media_session::mojom::MediaPlaybackState::kPlaying:
        media_status.playback_state = MediaStatus::PLAYING;
        break;
      case media_session::mojom::MediaPlaybackState::kPaused:
        media_status.playback_state = MediaStatus::PAUSED;
        break;
    }
  }

  auto* media_manager = assistant_manager_->GetMediaManager();
  if (media_manager)
    media_manager->SetExternalPlaybackState(media_status);
}

void AssistantManagerServiceImpl::ResetMediaState() {
  auto* media_manager = assistant_manager_->GetMediaManager();
  if (media_manager) {
    MediaStatus media_status;
    media_manager->SetExternalPlaybackState(media_status);
  }
}

std::string AssistantManagerServiceImpl::NewPendingInteraction(
    AssistantInteractionType interaction_type,
    AssistantQuerySource source,
    const std::string& query) {
  auto id = base::NumberToString(next_interaction_id_++);
  pending_interactions_.emplace(
      id, std::make_unique<AssistantInteractionMetadata>(interaction_type,
                                                         source, query));
  return id;
}

ash::AssistantAlarmTimerController*
AssistantManagerServiceImpl::assistant_alarm_timer_controller() {
  return context_->assistant_alarm_timer_controller();
}

ash::AssistantNotificationController*
AssistantManagerServiceImpl::assistant_notification_controller() {
  return context_->assistant_notification_controller();
}

ash::AssistantScreenContextController*
AssistantManagerServiceImpl::assistant_screen_context_controller() {
  return context_->assistant_screen_context_controller();
}

ash::AssistantStateBase* AssistantManagerServiceImpl::assistant_state() {
  return context_->assistant_state();
}

DeviceActions* AssistantManagerServiceImpl::device_actions() {
  return context_->device_actions();
}

scoped_refptr<base::SequencedTaskRunner>
AssistantManagerServiceImpl::main_task_runner() {
  return context_->main_task_runner();
}

void AssistantManagerServiceImpl::SetStateAndInformObservers(State new_state) {
  state_ = new_state;

  for (auto& observer : state_observers_)
    observer.OnStateChanged(state_);
}

}  // namespace assistant
}  // namespace chromeos
