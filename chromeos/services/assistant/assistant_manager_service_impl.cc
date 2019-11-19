// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/assistant_manager_service_impl.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "ash/public/cpp/assistant/assistant_state_base.h"
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
#include "base/unguessable_token.h"
#include "chromeos/assistant/internal/internal_constants.h"
#include "chromeos/assistant/internal/internal_util.h"
#include "chromeos/assistant/internal/proto/google3/assistant/api/client_input/warmer_welcome_input.pb.h"
#include "chromeos/assistant/internal/proto/google3/assistant/api/client_op/device_args.pb.h"
#include "chromeos/dbus/util/version_loader.h"
#include "chromeos/services/assistant/assistant_manager_service_delegate.h"
#include "chromeos/services/assistant/constants.h"
#include "chromeos/services/assistant/media_session/assistant_media_session.h"
#include "chromeos/services/assistant/platform_api_impl.h"
#include "chromeos/services/assistant/public/features.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom-shared.h"
#include "chromeos/services/assistant/service_context.h"
#include "chromeos/services/assistant/utils.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "libassistant/shared/internal_api/alarm_timer_manager.h"
#include "libassistant/shared/internal_api/alarm_timer_types.h"
#include "libassistant/shared/internal_api/assistant_manager_delegate.h"
#include "libassistant/shared/internal_api/assistant_manager_internal.h"
#include "libassistant/shared/public/assistant_manager.h"
#include "libassistant/shared/public/media_manager.h"
#include "mojo/public/mojom/base/time.mojom.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
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

using ActionModule = assistant_client::ActionModule;
using Resolution = assistant_client::ConversationStateListener::Resolution;
using MediaStatus = assistant_client::MediaStatus;
using CommunicationErrorType =
    chromeos::assistant::AssistantManagerService::CommunicationErrorType;

namespace api = ::assistant::api;

namespace chromeos {
namespace assistant {
namespace {

static bool is_first_init = true;

constexpr char kWiFiDeviceSettingId[] = "WIFI";
constexpr char kBluetoothDeviceSettingId[] = "BLUETOOTH";
constexpr char kVolumeLevelDeviceSettingId[] = "VOLUME_LEVEL";
constexpr char kScreenBrightnessDeviceSettingId[] = "BRIGHTNESS_LEVEL";
constexpr char kDoNotDisturbDeviceSettingId[] = "DO_NOT_DISTURB";
constexpr char kNightLightDeviceSettingId[] = "NIGHT_LIGHT_SWITCH";
constexpr char kIntentActionView[] = "android.intent.action.VIEW";

constexpr base::Feature kChromeOSAssistantDogfood{
    "ChromeOSAssistantDogfood", base::FEATURE_DISABLED_BY_DEFAULT};

constexpr char kServersideDogfoodExperimentId[] = "20347368";
constexpr char kServersideOpenAppExperimentId[] = "39651593";

constexpr char kNextTrackClientOp[] = "media.NEXT";
constexpr char kPauseTrackClientOp[] = "media.PAUSE";
constexpr char kPlayMediaClientOp[] = "media.PLAY_MEDIA";
constexpr char kPrevTrackClientOp[] = "media.PREVIOUS";
constexpr char kResumeTrackClientOp[] = "media.RESUME";
constexpr char kStopTrackClientOp[] = "media.STOP";

// The screen context query is locale independent. That is the same query
// applies to all locales.
constexpr char kScreenContextQuery[] = "screen context";

constexpr float kDefaultSliderStep = 0.1f;

bool IsScreenContextAllowed(ash::AssistantStateBase* assistant_state) {
  return assistant_state->allowed_state() ==
             ash::mojom::AssistantAllowedState::ALLOWED &&
         assistant_state->settings_enabled().value_or(false) &&
         assistant_state->context_enabled().value_or(false);
}

action::AppStatus GetActionAppStatus(mojom::AppStatus status) {
  switch (status) {
    case mojom::AppStatus::UNKNOWN:
      return action::UNKNOWN;
    case mojom::AppStatus::AVAILABLE:
      return action::AVAILABLE;
    case mojom::AppStatus::UNAVAILABLE:
      return action::UNAVAILABLE;
    case mojom::AppStatus::VERSION_MISMATCH:
      return action::VERSION_MISMATCH;
    case mojom::AppStatus::DISABLED:
      return action::DISABLED;
  }
}

ash::mojom::AssistantTimerState GetTimerState(
    assistant_client::Timer::State state) {
  switch (state) {
    case assistant_client::Timer::State::UNKNOWN:
      return ash::mojom::AssistantTimerState::kUnknown;
    case assistant_client::Timer::State::SCHEDULED:
      return ash::mojom::AssistantTimerState::kScheduled;
    case assistant_client::Timer::State::PAUSED:
      return ash::mojom::AssistantTimerState::kPaused;
    case assistant_client::Timer::State::FIRED:
      return ash::mojom::AssistantTimerState::kFired;
  }
}

CommunicationErrorType CommunicationErrorTypeFromLibassistantErrorCode(
    int error_code) {
  if (IsAuthError(error_code))
    return CommunicationErrorType::AuthenticationError;
  return CommunicationErrorType::Other;
}

}  // namespace

AssistantManagerServiceImpl::AssistantManagerServiceImpl(
    mojom::Client* client,
    ServiceContext* context,
    std::unique_ptr<AssistantManagerServiceDelegate> delegate,
    std::unique_ptr<network::SharedURLLoaderFactoryInfo>
        url_loader_factory_info,
    bool is_signed_out_mode)
    : client_(client),
      media_session_(std::make_unique<AssistantMediaSession>(client_, this)),
      action_module_(std::make_unique<action::CrosActionModule>(
          this,
          assistant::features::IsAppSupportEnabled(),
          assistant::features::IsRoutinesEnabled())),
      chromium_api_delegate_(std::move(url_loader_factory_info)),
      assistant_settings_manager_(
          std::make_unique<AssistantSettingsManagerImpl>(context, this)),
      context_(context),
      delegate_(std::move(delegate)),
      background_thread_("background thread"),
      is_signed_out_mode_(is_signed_out_mode),
      weak_factory_(this) {
  background_thread_.Start();

  platform_api_ = delegate_->CreatePlatformApi(
      media_session_.get(), background_thread_.task_runner());

  mojo::Remote<media_session::mojom::MediaControllerManager>
      media_controller_manager;
  client->RequestMediaControllerManager(
      media_controller_manager.BindNewPipeAndPassReceiver());
  media_controller_manager->CreateActiveMediaController(
      media_controller_.BindNewPipeAndPassReceiver());
}

AssistantManagerServiceImpl::~AssistantManagerServiceImpl() {
  background_thread_.Stop();
}

void AssistantManagerServiceImpl::Start(
    const base::Optional<std::string>& access_token,
    bool enable_hotword) {
  DCHECK(!assistant_manager_);
  DCHECK_EQ(GetState(), State::STOPPED);

  // Set the flag to avoid starting the service multiple times.
  SetStateAndInformObservers(State::STARTING);

  started_time_ = base::TimeTicks::Now();

  EnableHotword(enable_hotword);

  // LibAssistant creation will make file IO and sync wait. Post the creation to
  // background thread to avoid DCHECK.
  background_thread_.task_runner()->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&AssistantManagerServiceImpl::StartAssistantInternal,
                     base::Unretained(this), access_token),
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

void AssistantManagerServiceImpl::SetAccessToken(
    const std::string& access_token) {
  if (!assistant_manager_)
    return;

  DCHECK(!access_token.empty());

  VLOG(1) << "Set access token.";
  // Push the |access_token| we got as an argument into AssistantManager before
  // starting to ensure that all server requests will be authenticated once
  // it is started. |user_id| is used to pair a user to their |access_token|,
  // since we do not support multi-user in this example we can set it to a
  // dummy value like "0".
  assistant_manager_->SetAuthTokens(
      {std::pair<std::string, std::string>(kUserID, access_token)});
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

void AssistantManagerServiceImpl::UpdateInternalMediaPlayerStatus(
    media_session::mojom::MediaSessionAction action) {
  auto* media_manager = assistant_manager_->GetMediaManager();
  if (!media_manager)
    return;

  switch (action) {
    case media_session::mojom::MediaSessionAction::kPause:
      media_manager->Pause();
      break;
    case media_session::mojom::MediaSessionAction::kPlay:
      media_manager->Resume();
      break;
    case media_session::mojom::MediaSessionAction::kPreviousTrack:
    case media_session::mojom::MediaSessionAction::kNextTrack:
    case media_session::mojom::MediaSessionAction::kSeekBackward:
    case media_session::mojom::MediaSessionAction::kSeekForward:
    case media_session::mojom::MediaSessionAction::kSkipAd:
    case media_session::mojom::MediaSessionAction::kStop:
    case media_session::mojom::MediaSessionAction::kSeekTo:
    case media_session::mojom::MediaSessionAction::kScrubTo:
      NOTIMPLEMENTED();
      break;
  }
}

void AssistantManagerServiceImpl::WaitUntilStartIsFinishedForTesting() {
  // First we wait until |StartAssistantInternal| is finished.
  background_thread_.FlushForTesting();
  // Then we wait until |PostInitAssistant| finishes.
  // (which runs on the main thread).
  base::RunLoop().RunUntilIdle();
}

void AssistantManagerServiceImpl::AddMediaControllerObserver() {
  if (features::IsMediaSessionIntegrationEnabled()) {
    media_controller_->AddObserver(
        media_controller_observer_receiver_.BindNewPipeAndPassRemote());
  }
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
  alarm_timer_manager->RegisterRingingStateListener(
      [listener = std::move(listener_callback)] { listener.Run(); });
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

AssistantSettingsManager*
AssistantManagerServiceImpl::GetAssistantSettingsManager() {
  return assistant_settings_manager_.get();
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
  assistant_settings_manager_->SyncDeviceAppsStatus(
      base::BindOnce(&AssistantManagerServiceImpl::OnDeviceAppsEnabled,
                     weak_factory_.GetWeakPtr()));
}

void AssistantManagerServiceImpl::StartVoiceInteraction() {
  platform_api_->SetMicState(true);
  assistant_manager_->StartAssistantInteraction();
}

void AssistantManagerServiceImpl::StopActiveInteraction(
    bool cancel_conversation) {
  platform_api_->SetMicState(false);

  if (!assistant_manager_internal_) {
    VLOG(1) << "Stopping interaction without assistant manager.";
    return;
  }
  assistant_manager_internal_->StopAssistantInteractionInternal(
      cancel_conversation);
}

void AssistantManagerServiceImpl::StartWarmerWelcomeInteraction(
    int num_warmer_welcome_triggered,
    bool allow_tts) {
  DCHECK(assistant_manager_internal_ != nullptr);

  const std::string interaction =
      CreateWarmerWelcomeInteraction(num_warmer_welcome_triggered);

  assistant_client::VoicelessOptions options;
  options.is_user_initiated = true;
  options.modality =
      allow_tts ? assistant_client::VoicelessOptions::Modality::VOICE_MODALITY
                : assistant_client::VoicelessOptions::Modality::TYPING_MODALITY;

  auto interaction_type = allow_tts ? mojom::AssistantInteractionType::kVoice
                                    : mojom::AssistantInteractionType::kText;
  options.conversation_turn_id = NewPendingInteraction(
      interaction_type, mojom::AssistantQuerySource::kWarmerWelcome,
      /*query=*/std::string());

  assistant_manager_internal_->SendVoicelessInteraction(
      interaction, /*description=*/"warmer_welcome_trigger", options,
      [](auto) {});
}

// TODO(eyor): Add a method that can be called to clear the cached interaction
// when the UI is hidden/closed.
void AssistantManagerServiceImpl::StartCachedScreenContextInteraction() {
  if (!IsScreenContextAllowed(assistant_state()))
    return;

  // It is illegal to call this method without having first cached screen
  // context (see CacheScreenContext()).
  DCHECK(assistant_extra_);
  DCHECK(assistant_tree_);
  DCHECK(!assistant_screenshot_.empty());

  SendScreenContextRequest(assistant_extra_.get(), assistant_tree_.get(),
                           assistant_screenshot_);
}

void AssistantManagerServiceImpl::StartEditReminderInteraction(
    const std::string& client_id) {
  const std::string interaction = CreateEditReminderInteraction(client_id);
  assistant_client::VoicelessOptions voiceless_options;

  voiceless_options.is_user_initiated = true;
  assistant_manager_internal_->SendVoicelessInteraction(
      interaction, std::string(), voiceless_options, [](auto) {});
}

void AssistantManagerServiceImpl::StartMetalayerInteraction(
    const gfx::Rect& region) {
  if (!IsScreenContextAllowed(assistant_state()))
    return;

  assistant_screen_context_controller()->RequestScreenshot(
      region,
      base::BindOnce(&AssistantManagerServiceImpl::SendScreenContextRequest,
                     weak_factory_.GetWeakPtr(), /*assistant_extra=*/nullptr,
                     /*assistant_tree=*/nullptr));
}

void AssistantManagerServiceImpl::StartTextInteraction(
    const std::string& query,
    mojom::AssistantQuerySource source,
    bool allow_tts) {
  assistant_client::VoicelessOptions options;
  options.is_user_initiated = true;

  if (!allow_tts) {
    options.modality =
        assistant_client::VoicelessOptions::Modality::TYPING_MODALITY;
  }

  // Cache metadata about this interaction that can be resolved when the
  // associated conversation turn starts in LibAssistant.
  options.conversation_turn_id = NewPendingInteraction(
      mojom::AssistantInteractionType::kText, source, query);

  if (base::FeatureList::IsEnabled(
          assistant::features::kEnableTextQueriesWithClientDiscourseContext) &&
      assistant_extra_ && assistant_tree_) {
    // We don't send the screenshot, because the backend only needs the
    // view hierarchy to resolve contextual queries such as "Who is he?".
    assistant_manager_internal_->SendTextQueryWithClientDiscourseContext(
        query,
        CreateContextProto(
            AssistantBundle{assistant_extra_.get(), assistant_tree_.get()},
            is_first_client_discourse_context_query_),
        options);
    is_first_client_discourse_context_query_ = false;
  } else {
    std::string interaction = CreateTextQueryInteraction(query);
    assistant_manager_internal_->SendVoicelessInteraction(
        interaction, /*description=*/"text_query", options, [](auto) {});
  }
}

void AssistantManagerServiceImpl::AddAssistantInteractionSubscriber(
    mojo::PendingRemote<mojom::AssistantInteractionSubscriber> subscriber) {
  mojo::Remote<mojom::AssistantInteractionSubscriber> subscriber_remote(
      std::move(subscriber));
  interaction_subscribers_.Add(std::move(subscriber_remote));
}

void AssistantManagerServiceImpl::RetrieveNotification(
    mojom::AssistantNotificationPtr notification,
    int action_index) {
  const std::string& notification_id = notification->server_id;
  const std::string& consistency_token = notification->consistency_token;
  const std::string& opaque_token = notification->opaque_token;

  const std::string request_interaction =
      SerializeNotificationRequestInteraction(
          notification_id, consistency_token, opaque_token, action_index);

  assistant_client::VoicelessOptions options;
  options.is_user_initiated = true;

  assistant_manager_internal_->SendVoicelessInteraction(
      request_interaction, "RequestNotification", options, [](auto) {});
}

void AssistantManagerServiceImpl::DismissNotification(
    mojom::AssistantNotificationPtr notification) {
  const std::string& notification_id = notification->server_id;
  const std::string& consistency_token = notification->consistency_token;
  const std::string& opaque_token = notification->opaque_token;
  const std::string& grouping_key = notification->grouping_key;

  const std::string dismissed_interaction =
      SerializeNotificationDismissedInteraction(
          notification_id, consistency_token, opaque_token, {grouping_key});

  assistant_client::VoicelessOptions options;
  options.obfuscated_gaia_id = notification->obfuscated_gaia_id;

  assistant_manager_internal_->SendVoicelessInteraction(
      dismissed_interaction, "DismissNotification", options, [](auto) {});
}

void AssistantManagerServiceImpl::OnConversationTurnStartedInternal(
    const assistant_client::ConversationTurnMetadata& metadata) {
  ENSURE_MAIN_THREAD(
      &AssistantManagerServiceImpl::OnConversationTurnStartedInternal,
      metadata);

  platform_api_->OnConversationTurnStarted();

  // Retrieve the cached interaction metadata associated with this conversation
  // turn or construct a new instance if there's no match in the cache.
  mojom::AssistantInteractionMetadataPtr metadata_ptr;
  auto it = pending_interactions_.find(metadata.id);
  if (it != pending_interactions_.end()) {
    metadata_ptr = std::move(it->second);
    pending_interactions_.erase(it);
  } else {
    metadata_ptr = mojom::AssistantInteractionMetadata::New();
    metadata_ptr->type = metadata.is_mic_open
                             ? mojom::AssistantInteractionType::kVoice
                             : mojom::AssistantInteractionType::kText;
    metadata_ptr->source = mojom::AssistantQuerySource::kLibAssistantInitiated;
  }

  for (auto& it : interaction_subscribers_)
    it->OnInteractionStarted(metadata_ptr->Clone());
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
      for (auto& it : interaction_subscribers_) {
        it->OnInteractionFinished(
            mojom::AssistantInteractionResolution::kNormal);
      }

      RecordQueryResponseTypeUMA();
      break;
    // Interaction ended due to interruption.
    case Resolution::BARGE_IN:
    case Resolution::CANCELLED:
      for (auto& it : interaction_subscribers_) {
        it->OnInteractionFinished(
            mojom::AssistantInteractionResolution::kInterruption);
      }

      if (receive_inline_response_ || receive_modify_settings_proto_response_ ||
          !receive_url_response_.empty()) {
        RecordQueryResponseTypeUMA();
      }
      break;
    // Interaction ended due to mic timeout.
    case Resolution::TIMEOUT:
      for (auto& it : interaction_subscribers_) {
        it->OnInteractionFinished(
            mojom::AssistantInteractionResolution::kMicTimeout);
      }
      break;
    // Interaction ended due to error.
    case Resolution::COMMUNICATION_ERROR:
      for (auto& it : interaction_subscribers_) {
        it->OnInteractionFinished(
            mojom::AssistantInteractionResolution::kError);
      }
      break;
    // Interaction ended because the device was not selected to produce a
    // response. This occurs due to multi-device hotword loss.
    case Resolution::DEVICE_NOT_SELECTED:
      for (auto& it : interaction_subscribers_) {
        it->OnInteractionFinished(
            mojom::AssistantInteractionResolution::kMultiDeviceHotwordLoss);
      }
      break;
  }
}

void AssistantManagerServiceImpl::OnScheduleWait(int id, int time_ms) {
  ENSURE_MAIN_THREAD(&AssistantManagerServiceImpl::OnScheduleWait, id, time_ms);

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
    it->OnWaitStarted();
}

// TODO(b/113541754): Deprecate this API when the server provides a fallback.
void AssistantManagerServiceImpl::OnShowContextualQueryFallback() {
  // Show fallback text.
  OnShowText(l10n_util::GetStringUTF8(
      IDS_ASSISTANT_SCREEN_CONTEXT_QUERY_FALLBACK_TEXT));

  // Construct a fallback card.
  std::stringstream html;
  html << R"(
       <html>
         <head><meta CHARSET='utf-8'></head>
         <body>
           <style>
             * {
               cursor: default;
               font-family: Google Sans, sans-serif;
               user-select: none;
             }
             html, body { margin: 0; padding: 0; }
             div {
               border: 1px solid rgba(32, 33, 36, 0.08);
               border-radius: 12px;
               color: #5F6368;
               font-size: 13px;
               margin: 1px;
               padding: 16px;
               text-align: center;
             }
         </style>
         <div>)"
       << l10n_util::GetStringUTF8(
              IDS_ASSISTANT_SCREEN_CONTEXT_QUERY_FALLBACK_CARD)
       << "</div></body></html>";

  // Show fallback card.
  OnShowHtml(html.str(), /*fallback=*/"");
}

void AssistantManagerServiceImpl::OnShowHtml(const std::string& html,
                                             const std::string& fallback) {
  ENSURE_MAIN_THREAD(&AssistantManagerServiceImpl::OnShowHtml, html, fallback);

  receive_inline_response_ = true;

  for (auto& it : interaction_subscribers_)
    it->OnHtmlResponse(html, fallback);
}

void AssistantManagerServiceImpl::OnShowSuggestions(
    const std::vector<action::Suggestion>& suggestions) {
  ENSURE_MAIN_THREAD(&AssistantManagerServiceImpl::OnShowSuggestions,
                     suggestions);

  // Convert to mojom struct for IPC.
  std::vector<mojom::AssistantSuggestionPtr> ptrs;
  for (const action::Suggestion& suggestion : suggestions) {
    mojom::AssistantSuggestionPtr ptr = mojom::AssistantSuggestion::New();
    ptr->text = suggestion.text;
    ptr->icon_url = GURL(suggestion.icon_url);
    ptr->action_url = GURL(suggestion.action_url);
    ptrs.push_back(std::move(ptr));
  }

  for (auto& it : interaction_subscribers_)
    it->OnSuggestionsResponse(mojo::Clone(ptrs));
}

void AssistantManagerServiceImpl::OnShowText(const std::string& text) {
  ENSURE_MAIN_THREAD(&AssistantManagerServiceImpl::OnShowText, text);

  receive_inline_response_ = true;

  for (auto& it : interaction_subscribers_)
    it->OnTextResponse(text);
}

void AssistantManagerServiceImpl::OnOpenUrl(const std::string& url,
                                            bool is_background) {
  ENSURE_MAIN_THREAD(&AssistantManagerServiceImpl::OnOpenUrl, url,
                     is_background);

  receive_url_response_ = url;
  const GURL gurl = GURL(url);

  for (auto& it : interaction_subscribers_)
    it->OnOpenUrlResponse(gurl, is_background);
}

void AssistantManagerServiceImpl::OnShowNotification(
    const action::Notification& notification) {
  ENSURE_MAIN_THREAD(&AssistantManagerServiceImpl::OnShowNotification,
                     notification);

  mojom::AssistantNotificationPtr notification_ptr =
      mojom::AssistantNotification::New();
  notification_ptr->title = notification.title;
  notification_ptr->message = notification.text;
  notification_ptr->action_url = GURL(notification.action_url);
  notification_ptr->client_id = notification.notification_id;
  notification_ptr->server_id = notification.notification_id;
  notification_ptr->consistency_token = notification.consistency_token;
  notification_ptr->opaque_token = notification.opaque_token;
  notification_ptr->grouping_key = notification.grouping_key;
  notification_ptr->obfuscated_gaia_id = notification.obfuscated_gaia_id;

  if (notification.expiry_timestamp_ms) {
    notification_ptr->expiry_time =
        base::Time::FromJavaTime(notification.expiry_timestamp_ms);
  }

  // The server sometimes sends an empty |notification_id|, but our client
  // requires a non-empty |client_id| for notifications. Known instances in
  // which the server sends an empty |notification_id| are for Reminders.
  if (notification_ptr->client_id.empty())
    notification_ptr->client_id = base::UnguessableToken::Create().ToString();

  for (const auto& button : notification.buttons) {
    notification_ptr->buttons.push_back(mojom::AssistantNotificationButton::New(
        button.label, GURL(button.action_url)));
  }

  assistant_notification_controller()->AddOrUpdateNotification(
      notification_ptr.Clone());
}

void AssistantManagerServiceImpl::OnOpenAndroidApp(
    const action::AndroidAppInfo& app_info,
    const action::InteractionInfo& interaction) {
  ENSURE_MAIN_THREAD(&AssistantManagerServiceImpl::OnOpenAndroidApp, app_info,
                     interaction);
  mojom::AndroidAppInfoPtr app_info_ptr = mojom::AndroidAppInfo::New();
  app_info_ptr->package_name = app_info.package_name;
  for (auto& it : interaction_subscribers_) {
    it->OnOpenAppResponse(
        mojo::Clone(app_info_ptr),
        base::BindOnce(
            &AssistantManagerServiceImpl::HandleOpenAndroidAppResponse,
            weak_factory_.GetWeakPtr(), interaction));
  }
}

void AssistantManagerServiceImpl::OnVerifyAndroidApp(
    const std::vector<action::AndroidAppInfo>& apps_info,
    const action::InteractionInfo& interaction) {
  ENSURE_MAIN_THREAD(&AssistantManagerServiceImpl::OnVerifyAndroidApp,
                     apps_info, interaction);
  std::vector<mojom::AndroidAppInfoPtr> apps_info_list;
  for (auto& app_info : apps_info) {
    mojom::AndroidAppInfoPtr app_info_ptr = mojom::AndroidAppInfo::New();
    app_info_ptr->package_name = app_info.package_name;
    apps_info_list.push_back(std::move(app_info_ptr));
  }
  device_actions()->VerifyAndroidApp(
      std::move(apps_info_list),
      base::BindOnce(
          &AssistantManagerServiceImpl::HandleVerifyAndroidAppResponse,
          weak_factory_.GetWeakPtr(), interaction));
}

void AssistantManagerServiceImpl::OnOpenMediaAndroidIntent(
    const std::string play_media_args_proto,
    action::AndroidAppInfo* android_app_info) {
  DCHECK(main_task_runner()->RunsTasksInCurrentSequence());

  // Handle android media playback intent.
  mojom::AndroidAppInfoPtr app_info_ptr = mojom::AndroidAppInfo::New();
  app_info_ptr->package_name = android_app_info->package_name;
  app_info_ptr->action = kIntentActionView;
  if (!android_app_info->intent.empty()) {
    app_info_ptr->intent = android_app_info->intent;
  } else {
    std::string url = GetAndroidIntentUrlFromMediaArgs(play_media_args_proto);
    if (!url.empty()) {
      app_info_ptr->intent = url;
    }
  }
  for (auto& it : interaction_subscribers_) {
    it->OnOpenAppResponse(
        mojo::Clone(app_info_ptr),
        base::BindOnce(
            &AssistantManagerServiceImpl::HandleLaunchMediaIntentResponse,
            weak_factory_.GetWeakPtr()));
  }
}

void AssistantManagerServiceImpl::OnPlayMedia(
    const std::string play_media_args_proto) {
  ENSURE_MAIN_THREAD(&AssistantManagerServiceImpl::OnPlayMedia,
                     play_media_args_proto);

  std::unique_ptr<action::AndroidAppInfo> android_app_info =
      GetAndroidAppInfoFromMediaArgs(play_media_args_proto);
  if (android_app_info) {
    OnOpenMediaAndroidIntent(play_media_args_proto, android_app_info.get());
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
    media_controller_->Stop();
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
        it->OnSpeechRecognitionStarted();
      break;
    case assistant_client::ConversationStateListener::RecognitionState::
        INTERMEDIATE_RESULT:
      for (auto& it : interaction_subscribers_) {
        it->OnSpeechRecognitionIntermediateResult(
            recognition_result.high_confidence_text,
            recognition_result.low_confidence_text);
      }
      break;
    case assistant_client::ConversationStateListener::RecognitionState::
        END_OF_UTTERANCE:
      for (auto& it : interaction_subscribers_)
        it->OnSpeechRecognitionEndOfUtterance();
      break;
    case assistant_client::ConversationStateListener::RecognitionState::
        FINAL_RESULT:
      for (auto& it : interaction_subscribers_) {
        it->OnSpeechRecognitionFinalResult(
            recognition_result.recognized_speech);
      }
      break;
  }
}

void AssistantManagerServiceImpl::OnRespondingStarted(bool is_error_response) {
  ENSURE_MAIN_THREAD(&AssistantManagerServiceImpl::OnRespondingStarted,
                     is_error_response);

  for (auto& it : interaction_subscribers_)
    it->OnTtsStarted(is_error_response);
}

void AssistantManagerServiceImpl::OnSpeechLevelUpdated(
    const float speech_level) {
  ENSURE_MAIN_THREAD(&AssistantManagerServiceImpl::OnSpeechLevelUpdated,
                     speech_level);

  for (auto& it : interaction_subscribers_)
    it->OnSpeechLevelUpdated(speech_level);
}

void LogUnsupportedChange(api::client_op::ModifySettingArgs args) {
  LOG(ERROR) << "Unsupported change operation: " << args.change()
             << " for setting " << args.setting_id();
}

void HandleOnOffChange(api::client_op::ModifySettingArgs modify_setting_args,
                       std::function<void(bool)> on_off_handler) {
  switch (modify_setting_args.change()) {
    case api::client_op::ModifySettingArgs_Change_ON:
      on_off_handler(true);
      return;
    case api::client_op::ModifySettingArgs_Change_OFF:
      on_off_handler(false);
      return;

    // Currently there are no use-cases for toggling.  This could change in the
    // future.
    case api::client_op::ModifySettingArgs_Change_TOGGLE:
      break;

    case api::client_op::ModifySettingArgs_Change_SET:
    case api::client_op::ModifySettingArgs_Change_INCREASE:
    case api::client_op::ModifySettingArgs_Change_DECREASE:
    case api::client_op::ModifySettingArgs_Change_UNSPECIFIED:
      // This shouldn't happen.
      break;
  }
  LogUnsupportedChange(modify_setting_args);
}

// Helper function that converts a slider value sent from the server, either
// absolute or a delta, from a given unit (e.g., STEP), to a percentage.
double ConvertSliderValueToLevel(double value,
                                 api::client_op::ModifySettingArgs_Unit unit,
                                 double default_value) {
  switch (unit) {
    case api::client_op::ModifySettingArgs_Unit_RANGE:
      // "set volume to 20%".
      return value;
    case api::client_op::ModifySettingArgs_Unit_STEP:
      // "set volume to 20".  Treat the step as a percentage.
      return value / 100.0f;

    // Currently, factor (e.g., 'double the volume') and decibel units aren't
    // handled by the backend.  This could change in the future.
    case api::client_op::ModifySettingArgs_Unit_FACTOR:
    case api::client_op::ModifySettingArgs_Unit_DECIBEL:
      break;

    case api::client_op::ModifySettingArgs_Unit_NATIVE:
    case api::client_op::ModifySettingArgs_Unit_UNKNOWN_UNIT:
      // This shouldn't happen.
      break;
  }
  LOG(ERROR) << "Unsupported slider unit: " << unit;
  return default_value;
}

void HandleSliderChange(api::client_op::ModifySettingArgs modify_setting_args,
                        std::function<void(double)> set_value_handler,
                        std::function<double()> get_value_handler) {
  switch (modify_setting_args.change()) {
    case api::client_op::ModifySettingArgs_Change_SET: {
      // For unsupported units, set the value to the current value, for
      // visual feedback.
      double new_value = ConvertSliderValueToLevel(
          modify_setting_args.numeric_value(), modify_setting_args.unit(),
          get_value_handler());
      set_value_handler(new_value);
      return;
    }

    case api::client_op::ModifySettingArgs_Change_INCREASE:
    case api::client_op::ModifySettingArgs_Change_DECREASE: {
      double current_value = get_value_handler();
      double step = kDefaultSliderStep;
      if (modify_setting_args.numeric_value() != 0.0f) {
        // For unsupported units, use the default step percentage.
        step = ConvertSliderValueToLevel(modify_setting_args.numeric_value(),
                                         modify_setting_args.unit(),
                                         kDefaultSliderStep);
      }
      double new_value = (modify_setting_args.change() ==
                          api::client_op::ModifySettingArgs_Change_INCREASE)
                             ? std::min(current_value + step, 1.0)
                             : std::max(current_value - step, 0.0);
      set_value_handler(new_value);
      return;
    }

    case api::client_op::ModifySettingArgs_Change_ON:
    case api::client_op::ModifySettingArgs_Change_OFF:
    case api::client_op::ModifySettingArgs_Change_TOGGLE:
    case api::client_op::ModifySettingArgs_Change_UNSPECIFIED:
      // This shouldn't happen.
      break;
  }
  LogUnsupportedChange(modify_setting_args);
}

void AssistantManagerServiceImpl::OnModifySettingsAction(
    const std::string& modify_setting_args_proto) {
  api::client_op::ModifySettingArgs modify_setting_args;
  modify_setting_args.ParseFromString(modify_setting_args_proto);
  DCHECK(IsSettingSupported(modify_setting_args.setting_id()));
  receive_modify_settings_proto_response_ = true;

  if (modify_setting_args.setting_id() == kWiFiDeviceSettingId) {
    HandleOnOffChange(modify_setting_args, [&](bool enabled) {
      this->device_actions()->SetWifiEnabled(enabled);
    });
  }

  if (modify_setting_args.setting_id() == kBluetoothDeviceSettingId) {
    HandleOnOffChange(modify_setting_args, [&](bool enabled) {
      this->device_actions()->SetBluetoothEnabled(enabled);
    });
  }

  if (modify_setting_args.setting_id() == kVolumeLevelDeviceSettingId) {
    assistant_client::VolumeControl& volume_control =
        this->platform_api_->GetAudioOutputProvider().GetVolumeControl();

    HandleSliderChange(
        modify_setting_args,
        [&](double value) { volume_control.SetSystemVolume(value, true); },
        [&]() { return volume_control.GetSystemVolume(); });
  }

  if (modify_setting_args.setting_id() == kScreenBrightnessDeviceSettingId) {
    this->device_actions()->GetScreenBrightnessLevel(base::BindOnce(
        [](base::WeakPtr<chromeos::assistant::AssistantManagerServiceImpl>
               this_,
           api::client_op::ModifySettingArgs modify_setting_args, bool success,
           double current_value) {
          if (!success || !this_) {
            return;
          }
          HandleSliderChange(
              modify_setting_args,
              [&](double new_value) {
                this_->device_actions()->SetScreenBrightnessLevel(new_value,
                                                                  true);
              },
              [&]() { return current_value; });
        },
        weak_factory_.GetWeakPtr(), modify_setting_args));
  }

  if (modify_setting_args.setting_id() == kDoNotDisturbDeviceSettingId) {
    HandleOnOffChange(modify_setting_args, [&](bool enabled) {
      this->assistant_notification_controller()->SetQuietMode(enabled);
    });
  }

  if (modify_setting_args.setting_id() == kNightLightDeviceSettingId) {
    HandleOnOffChange(modify_setting_args, [&](bool enabled) {
      this->device_actions()->SetNightLightEnabled(enabled);
    });
  }
}

ActionModule::Result AssistantManagerServiceImpl::HandleModifySettingClientOp(
    const std::string& modify_setting_args_proto) {
  DVLOG(2) << "HandleModifySettingClientOp=" << modify_setting_args_proto;
  main_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&AssistantManagerServiceImpl::OnModifySettingsAction,
                     weak_factory_.GetWeakPtr(), modify_setting_args_proto));
  return ActionModule::Result::Ok();
}

bool AssistantManagerServiceImpl::IsSettingSupported(
    const std::string& setting_id) {
  DVLOG(2) << "IsSettingSupported=" << setting_id;
  return (setting_id == kWiFiDeviceSettingId ||
          setting_id == kBluetoothDeviceSettingId ||
          setting_id == kVolumeLevelDeviceSettingId ||
          setting_id == kScreenBrightnessDeviceSettingId ||
          setting_id == kDoNotDisturbDeviceSettingId ||
          setting_id == kNightLightDeviceSettingId);
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
    const base::Optional<std::string>& access_token) {
  DCHECK(background_thread_.task_runner()->BelongsToCurrentThread());
  base::AutoLock lock(new_assistant_manager_lock_);
  // There can only be one |AssistantManager| instance at any given time.
  DCHECK(!assistant_manager_);
  new_display_connection_ = std::make_unique<CrosDisplayConnection>(
      this, assistant::features::IsFeedbackUiEnabled(),
      assistant::features::IsMediaSessionIntegrationEnabled());

  new_assistant_manager_ = delegate_->CreateAssistantManager(
      platform_api_.get(), CreateLibAssistantConfig());
  new_assistant_manager_internal_ =
      delegate_->UnwrapAssistantManagerInternal(new_assistant_manager_.get());

  UpdateInternalOptions(new_assistant_manager_internal_);

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

  if (!is_signed_out_mode_) {
    new_assistant_manager_->SetAuthTokens(
        {std::pair<std::string, std::string>(kUserID, access_token.value())});
  }
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

  assistant_settings_manager_->UpdateServerDeviceSettings();

  if (base::FeatureList::IsEnabled(assistant::features::kAssistantAppSupport)) {
    device_actions()->AddAppListEventSubscriber(
        app_list_subscriber_receiver_.BindNewPipeAndPassRemote());
  }
}

void AssistantManagerServiceImpl::HandleLaunchMediaIntentResponse(
    bool app_opened) {
  // TODO(llin): Handle the response.
  NOTIMPLEMENTED();
}

void AssistantManagerServiceImpl::HandleOpenAndroidAppResponse(
    const action::InteractionInfo& interaction,
    bool app_opened) {
  std::string interaction_proto = CreateOpenProviderResponseInteraction(
      interaction.interaction_id, app_opened);

  assistant_client::VoicelessOptions options;
  options.obfuscated_gaia_id = interaction.user_id;

  assistant_manager_internal_->SendVoicelessInteraction(
      interaction_proto, "open_provider_response", options, [](auto) {});
}

void AssistantManagerServiceImpl::HandleVerifyAndroidAppResponse(
    const action::InteractionInfo& interaction,
    std::vector<mojom::AndroidAppInfoPtr> apps_info) {
  std::vector<action::AndroidAppInfo> action_apps_info;
  for (const auto& app_info : apps_info) {
    action_apps_info.push_back({app_info->package_name, app_info->version,
                                app_info->localized_app_name, app_info->intent,
                                GetActionAppStatus(app_info->status)});
  }
  std::string interaction_proto = CreateVerifyProviderResponseInteraction(
      interaction.interaction_id, action_apps_info);

  assistant_client::VoicelessOptions options;
  options.obfuscated_gaia_id = interaction.user_id;
  // Set the request to be user initiated so that a new conversation will be
  // created to handle the client OPs in the response of this request.
  options.is_user_initiated = true;

  assistant_manager_internal_->SendVoicelessInteraction(
      interaction_proto, "verify_provider_response", options, [](auto) {});
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
      assistant_settings_manager_->SyncSpeakerIdEnrollmentStatus();
  }

  const base::TimeDelta time_since_started =
      base::TimeTicks::Now() - started_time_;
  UMA_HISTOGRAM_TIMES("Assistant.ServiceReadyTime", time_since_started);

  SyncDeviceAppsStatus();

  RegisterFallbackMediaHandler();
  AddMediaControllerObserver();

  auto* media_manager = assistant_manager_->GetMediaManager();
  if (media_manager)
    media_manager->AddListener(this);

  RegisterAlarmsTimersListener();

  if (assistant_state()->arc_play_store_enabled().has_value())
    SetArcPlayStoreEnabled(assistant_state()->arc_play_store_enabled().value());
}

void AssistantManagerServiceImpl::OnAndroidAppListRefreshed(
    std::vector<mojom::AndroidAppInfoPtr> apps_info) {
  std::vector<action::AndroidAppInfo> android_apps_info;
  for (const auto& app_info : apps_info) {
    android_apps_info.push_back({app_info->package_name, app_info->version,
                                 app_info->localized_app_name,
                                 app_info->intent});
  }
  display_connection_->OnAndroidAppListRefreshed(android_apps_info);
}

void AssistantManagerServiceImpl::UpdateInternalOptions(
    assistant_client::AssistantManagerInternal* assistant_manager_internal) {
  // Build internal options
  auto* internal_options =
      assistant_manager_internal->CreateDefaultInternalOptions();
  SetAssistantOptions(internal_options, assistant_state()->locale().value(),
                      spoken_feedback_enabled_);

  internal_options->SetClientControlEnabled(
      assistant::features::IsRoutinesEnabled());

  if (is_signed_out_mode_) {
    internal_options->SetUserCredentialMode(
        assistant_client::InternalOptions::UserCredentialMode::SIGNED_OUT);
  }

  if (!features::IsVoiceMatchDisabled())
    internal_options->EnableRequireVoiceMatchVerification();

  assistant_manager_internal->SetOptions(*internal_options, [](bool success) {
    DVLOG(2) << "set options: " << success;
  });
}

void AssistantManagerServiceImpl::MediaSessionChanged(
    const base::Optional<base::UnguessableToken>& request_id) {
  if (request_id.has_value())
    media_session_audio_focus_id_ = std::move(request_id.value());
}

void AssistantManagerServiceImpl::MediaSessionInfoChanged(
    media_session::mojom::MediaSessionInfoPtr info) {
  media_session_info_ptr_ = std::move(info);
  UpdateMediaState();
}

void AssistantManagerServiceImpl::MediaSessionMetadataChanged(
    const base::Optional<media_session::MediaMetadata>& metadata) {
  media_metadata_ = std::move(metadata);
  UpdateMediaState();
}


void AssistantManagerServiceImpl::OnPlaybackStateChange(
    const MediaStatus& status) {
  if (media_session_)
    media_session_->NotifyMediaSessionMetadataChanged(status);
}

void AssistantManagerServiceImpl::OnAlarmTimerStateChanged() {
  ENSURE_MAIN_THREAD(&AssistantManagerServiceImpl::OnAlarmTimerStateChanged);
  // Currently, we only handle ringing events here. After some AlarmTimerManager
  // API improvement, we will be handling other alarm/timer events.
  auto* alarm_timer_manager =
      assistant_manager_internal_->GetAlarmTimerManager();
  // TODO(llin): Use GetAllEvents after the AlarmTimerManager API improvement is
  // ready (b/128701326).
  const assistant_client::AlarmTimerEvent& ringing_event =
      alarm_timer_manager->GetRingingEvent();

  switch (ringing_event.type) {
    case assistant_client::AlarmTimerEvent::NONE:
      assistant_alarm_timer_controller()->OnAlarmTimerStateChanged(nullptr);
      break;
    case assistant_client::AlarmTimerEvent::TIMER: {
      ash::mojom::AssistantAlarmTimerEventPtr alarm_timer_event_ptr =
          ash::mojom::AssistantAlarmTimerEvent::New();
      alarm_timer_event_ptr->type =
          ash::mojom::AssistantAlarmTimerEventType::kTimer;

      if (ringing_event.type == assistant_client::AlarmTimerEvent::TIMER) {
        alarm_timer_event_ptr->data = ash::mojom::AlarmTimerData::New();
        ash::mojom::AssistantTimerPtr timer_data_ptr =
            ash::mojom::AssistantTimer::New();
        timer_data_ptr->state = GetTimerState(ringing_event.timer_data.state);
        timer_data_ptr->timer_id = ringing_event.timer_data.timer_id;
        alarm_timer_event_ptr->data->set_timer_data(std::move(timer_data_ptr));
      }

      assistant_alarm_timer_controller()->OnAlarmTimerStateChanged(
          std::move(alarm_timer_event_ptr));
      break;
    }
    case assistant_client::AlarmTimerEvent::ALARM:
      // TODO(llin): Handle alarm.
      NOTREACHED();
      break;
  }
}

void AssistantManagerServiceImpl::CacheScreenContext(
    CacheScreenContextCallback callback) {
  if (!IsScreenContextAllowed(assistant_state())) {
    std::move(callback).Run();
    return;
  }

  // Our callback should be run only after both view hierarchy and screenshot
  // data have been cached from their respective providers.
  auto on_done = base::BarrierClosure(2, std::move(callback));

  client_->RequestAssistantStructure(
      base::BindOnce(&AssistantManagerServiceImpl::CacheAssistantStructure,
                     weak_factory_.GetWeakPtr(), on_done));

  assistant_screen_context_controller()->RequestScreenshot(
      gfx::Rect(),
      base::BindOnce(&AssistantManagerServiceImpl::CacheAssistantScreenshot,
                     weak_factory_.GetWeakPtr(), on_done));
}

void AssistantManagerServiceImpl::ClearScreenContextCache() {
  assistant_extra_.reset();
  assistant_tree_.reset();
  assistant_screenshot_.clear();
  is_first_client_discourse_context_query_ = true;
}

void AssistantManagerServiceImpl::OnAccessibilityStatusChanged(
    bool spoken_feedback_enabled) {
  if (spoken_feedback_enabled_ == spoken_feedback_enabled)
    return;

  spoken_feedback_enabled_ = spoken_feedback_enabled;

  // When |spoken_feedback_enabled_| changes we need to update our internal
  // options to turn on/off A11Y features in LibAssistant.
  if (assistant_manager_internal_)
    UpdateInternalOptions(assistant_manager_internal_);
}

void AssistantManagerServiceImpl::OnDeviceAppsEnabled(bool enabled) {
  display_connection_->SetDeviceAppsEnabled(enabled);
  action_module_->SetAppSupportEnabled(
      assistant::features::IsAppSupportEnabled() && enabled);
}

void AssistantManagerServiceImpl::StopAlarmTimerRinging() {
  if (!assistant_manager_internal_)
    return;

  assistant_manager_internal_->GetAlarmTimerManager()->StopRinging();
}

void AssistantManagerServiceImpl::CreateTimer(base::TimeDelta duration) {
  if (!assistant_manager_internal_)
    return;

  assistant_manager_internal_->GetAlarmTimerManager()->CreateTimer(
      duration.InSeconds(), /*label=*/std::string());
}

void AssistantManagerServiceImpl::CacheAssistantStructure(
    base::OnceClosure on_done,
    ax::mojom::AssistantExtraPtr assistant_extra,
    std::unique_ptr<ui::AssistantTree> assistant_tree) {
  assistant_extra_ = std::move(assistant_extra);
  assistant_tree_ = std::move(assistant_tree);
  std::move(on_done).Run();
}

void AssistantManagerServiceImpl::CacheAssistantScreenshot(
    base::OnceClosure on_done,
    const std::vector<uint8_t>& assistant_screenshot) {
  assistant_screenshot_ = assistant_screenshot;
  std::move(on_done).Run();
}

void AssistantManagerServiceImpl::SendScreenContextRequest(
    ax::mojom::AssistantExtra* assistant_extra,
    ui::AssistantTree* assistant_tree,
    const std::vector<uint8_t>& assistant_screenshot) {
  if (assistant::features::IsScreenContextQueryEnabled()) {
    assistant_client::VoicelessOptions options;
    options.is_user_initiated = true;

    assistant_manager_internal_->SendTextQueryWithClientDiscourseContext(
        kScreenContextQuery,
        CreateContextProto(
            AssistantBundle{assistant_extra_.get(), assistant_tree_.get()},
            assistant_screenshot),
        options);
    return;
  }

  std::vector<std::string> context_protos;

  // Screen context can have the assistant_extra and assistant_tree set to
  // nullptr. This happens in the case where the screen context is coming from
  // the metalayer. For this scenario, we don't create a context proto for the
  // AssistantBundle that consists of the assistant_extra and assistant_tree.
  if (assistant_extra && assistant_tree) {
    // Note: the value of is_first_query for screen context query is a no-op
    // because it is not used for metalayer and "What's on my screen" queries.
    context_protos.emplace_back(
        CreateContextProto(AssistantBundle{assistant_extra, assistant_tree},
                           /*is_first_query=*/true));
  }

  // Note: the value of is_first_query for screen context query is a no-op.
  context_protos.emplace_back(CreateContextProto(assistant_screenshot,
                                                 /*is_first_query=*/true));
  assistant_manager_internal_->SendScreenContextRequest(context_protos);
}

std::string AssistantManagerServiceImpl::GetLastSearchSource() {
  base::AutoLock lock(last_search_source_lock_);
  auto search_source = last_search_source_;
  last_search_source_ = std::string();
  return search_source;
}

void AssistantManagerServiceImpl::FillServerExperimentIds(
    std::vector<std::string>* server_experiment_ids) {
  if (base::FeatureList::IsEnabled(kChromeOSAssistantDogfood)) {
    server_experiment_ids->emplace_back(kServersideDogfoodExperimentId);
  }

  if (base::FeatureList::IsEnabled(assistant::features::kAssistantAppSupport))
    server_experiment_ids->emplace_back(kServersideOpenAppExperimentId);
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
    mojom::AssistantFeedbackPtr assistant_feedback) {
  const std::string interaction = CreateSendFeedbackInteraction(
      assistant_feedback->assistant_debug_info_allowed,
      assistant_feedback->description, assistant_feedback->screenshot_png);
  assistant_client::VoicelessOptions voiceless_options;

  voiceless_options.is_user_initiated = false;

  assistant_manager_internal_->SendVoicelessInteraction(
      interaction, "send feedback with details", voiceless_options,
      [](auto) {});
}

void AssistantManagerServiceImpl::UpdateMediaState() {
  if (media_session_info_ptr_) {
    if (media_session_info_ptr_->is_sensitive) {
      // Do not update media state if the session is considered to be sensitive
      // (off the record profile).
      return;
    }

    if (media_session_info_ptr_->state ==
            media_session::mojom::MediaSessionInfo::SessionState::kSuspended &&
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
  if (media_session_ && media_session_->internal_audio_focus_id() ==
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
          media_session::mojom::MediaSessionInfo::SessionState::kInactive) {
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

std::string AssistantManagerServiceImpl::NewPendingInteraction(
    mojom::AssistantInteractionType interaction_type,
    mojom::AssistantQuerySource source,
    const std::string& query) {
  auto id = base::NumberToString(next_interaction_id_++);
  pending_interactions_[id] =
      mojom::AssistantInteractionMetadata::New(interaction_type, source, query);
  return id;
}

ash::mojom::AssistantAlarmTimerController*
AssistantManagerServiceImpl::assistant_alarm_timer_controller() {
  return context_->assistant_alarm_timer_controller();
}

ash::mojom::AssistantNotificationController*
AssistantManagerServiceImpl::assistant_notification_controller() {
  return context_->assistant_notification_controller();
}

ash::mojom::AssistantScreenContextController*
AssistantManagerServiceImpl::assistant_screen_context_controller() {
  return context_->assistant_screen_context_controller();
}

ash::AssistantStateBase* AssistantManagerServiceImpl::assistant_state() {
  return context_->assistant_state();
}

mojom::DeviceActions* AssistantManagerServiceImpl::device_actions() {
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
