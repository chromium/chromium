// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/assistant_manager_service_impl.h"

#include <algorithm>
#include <utility>

#include "ash/public/interfaces/constants.mojom.h"
#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/feature_list.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/sys_info.h"
#include "base/task/post_task.h"
#include "build/util/webkit_version.h"
#include "chromeos/assistant/internal/internal_constants.h"
#include "chromeos/assistant/internal/internal_util.h"
#include "chromeos/assistant/internal/proto/google3/assistant/api/client_op/device_args.pb.h"
#include "chromeos/dbus/util/version_loader.h"
#include "chromeos/services/assistant/public/proto/assistant_device_settings_ui.pb.h"
#include "chromeos/services/assistant/public/proto/settings_ui.pb.h"
#include "chromeos/services/assistant/service.h"
#include "chromeos/services/assistant/utils.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "libassistant/shared/internal_api/assistant_manager_delegate.h"
#include "libassistant/shared/internal_api/assistant_manager_internal.h"
#include "libassistant/shared/public/media_manager.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

using ActionModule = assistant_client::ActionModule;
using Resolution = assistant_client::ConversationStateListener::Resolution;
using SpeakerIdEnrollmentState =
    assistant_client::SpeakerIdEnrollmentUpdate::State;

namespace api = ::assistant::api;

namespace chromeos {
namespace assistant {
namespace {

constexpr char kUserID[] = "0";
constexpr char kWiFiDeviceSettingId[] = "WIFI";
constexpr char kBluetoothDeviceSettingId[] = "BLUETOOTH";
constexpr char kVolumeLevelDeviceSettingId[] = "VOLUME_LEVEL";
constexpr char kScreenBrightnessDeviceSettingId[] = "BRIGHTNESS_LEVEL";
constexpr char kDoNotDisturbDeviceSettingId[] = "DO_NOT_DISTURB";
constexpr char kNightLightDeviceSettingId[] = "NIGHT_LIGHT_SWITCH";
constexpr char kTimerFireNotificationGroupId[] = "assistant/timer_fire";
constexpr char kQueryDeeplinkPrefix[] = "googleassistant://send-query?q=";
constexpr base::Feature kAssistantTimerNotificationFeature{
    "ChromeOSAssistantTimerNotification", base::FEATURE_ENABLED_BY_DEFAULT};
constexpr base::Feature kChromeOSAssistantDogfood{
    "ChromeOSAssistantDogfood", base::FEATURE_DISABLED_BY_DEFAULT};
constexpr char kServersideDogfoodExperimentId[] = "20347368";

constexpr float kDefaultSliderStep = 0.1f;
}  // namespace

AssistantManagerServiceImpl::AssistantManagerServiceImpl(
    service_manager::Connector* connector,
    device::mojom::BatteryMonitorPtr battery_monitor,
    Service* service,
    bool enable_hotword,
    network::NetworkConnectionTracker* network_connection_tracker)
    : enable_hotword_(enable_hotword),
      action_module_(std::make_unique<action::CrosActionModule>(this)),
      main_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      assistant_settings_manager_(
          std::make_unique<AssistantSettingsManagerImpl>(this)),
      display_connection_(std::make_unique<CrosDisplayConnection>(this)),
      voice_interaction_observer_binding_(this),
      service_(service),
      background_thread_("background thread"),
      weak_factory_(this) {
  background_thread_.Start();
  platform_api_ = std::make_unique<PlatformApiImpl>(
      connector, std::move(battery_monitor), enable_hotword,
      background_thread_.task_runner(), network_connection_tracker);
  connector->BindInterface(ash::mojom::kServiceName,
                           &voice_interaction_controller_);
  connector->BindInterface(ash::mojom::kServiceName,
                           &ash_message_center_controller_);

  // TODO(b/112281490): Combine this observer with the one in service.cc.
  ash::mojom::VoiceInteractionObserverPtr ptr;
  voice_interaction_observer_binding_.Bind(mojo::MakeRequest(&ptr));
  voice_interaction_controller_->AddObserver(std::move(ptr));

  // Initialize |assistant_enabled_| to the value in settings.
  voice_interaction_controller_->IsSettingEnabled(base::BindOnce(
      &AssistantManagerServiceImpl::OnVoiceInteractionSettingsEnabled,
      weak_factory_.GetWeakPtr()));

  // Initialize |context_enabled_| to the value in settings.
  voice_interaction_controller_->IsContextEnabled(base::BindOnce(
      &AssistantManagerServiceImpl::OnVoiceInteractionContextEnabled,
      weak_factory_.GetWeakPtr()));
}

AssistantManagerServiceImpl::~AssistantManagerServiceImpl() {}

void AssistantManagerServiceImpl::Start(const std::string& access_token,
                                        base::OnceClosure post_init_callback) {
  // Set the flag to avoid starting the service multiple times.
  state_ = State::STARTED;

  started_time_ = base::TimeTicks::Now();

  // LibAssistant creation will make file IO and sync wait. Post the creation to
  // background thread to avoid DCHECK.
  background_thread_.task_runner()->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&AssistantManagerServiceImpl::StartAssistantInternal,
                     base::Unretained(this), access_token,
                     chromeos::version_loader::GetARCVersion()),
      base::BindOnce(&AssistantManagerServiceImpl::PostInitAssistant,
                     base::Unretained(this), std::move(post_init_callback)));
}

void AssistantManagerServiceImpl::Stop() {
  state_ = State::STOPPED;

  assistant_manager_internal_ = nullptr;
  assistant_manager_.reset(nullptr);
}

AssistantManagerService::State AssistantManagerServiceImpl::GetState() const {
  return state_;
}

void AssistantManagerServiceImpl::SetAccessToken(
    const std::string& access_token) {
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
      [this](std::string play_media_args_proto) {
        std::string url = GetWebUrlFromMediaArgs(play_media_args_proto);
        if (!url.empty()) {
          OnOpenUrl(url);
        }
      });
}

void AssistantManagerServiceImpl::EnableListening(bool enable) {
  assistant_manager_->EnableListening(enable);
}

AssistantSettingsManager*
AssistantManagerServiceImpl::GetAssistantSettingsManager() {
  return assistant_settings_manager_.get();
}

void AssistantManagerServiceImpl::SendGetSettingsUiRequest(
    const std::string& selector,
    GetSettingsUiResponseCallback callback) {
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());

  std::string serialized_proto = SerializeGetSettingsUiRequest(selector);
  assistant_manager_internal_->SendGetSettingsUiRequest(
      serialized_proto, std::string(), [
        repeating_callback =
            base::AdaptCallbackForRepeating(std::move(callback)),
        weak_ptr = weak_factory_.GetWeakPtr(),
        task_runner = main_thread_task_runner_
      ](const assistant_client::VoicelessResponse& response) {
        // This callback may be called from server multiple times. We should
        // only process non-empty response.
        std::string settings = UnwrapGetSettingsUiResponse(response);
        if (!settings.empty()) {
          task_runner->PostTask(
              FROM_HERE,
              base::BindOnce(
                  &AssistantManagerServiceImpl::HandleGetSettingsResponse,
                  std::move(weak_ptr), repeating_callback, settings));
        }
      });
}

void AssistantManagerServiceImpl::SendUpdateSettingsUiRequest(
    const std::string& update,
    UpdateSettingsUiResponseCallback callback) {
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());

  std::string serialized_proto = SerializeUpdateSettingsUiRequest(update);
  assistant_manager_internal_->SendUpdateSettingsUiRequest(
      serialized_proto, std::string(), [
        repeating_callback =
            base::AdaptCallbackForRepeating(std::move(callback)),
        weak_ptr = weak_factory_.GetWeakPtr(),
        task_runner = main_thread_task_runner_
      ](const assistant_client::VoicelessResponse& response) {
        // This callback may be called from server multiple times. We should
        // only process non-empty response.
        std::string update = UnwrapUpdateSettingsUiResponse(response);
        if (!update.empty()) {
          task_runner->PostTask(
              FROM_HERE,
              base::BindOnce(
                  &AssistantManagerServiceImpl::HandleUpdateSettingsResponse,
                  std::move(weak_ptr), repeating_callback, update));
        }
      });
}

void AssistantManagerServiceImpl::StartSpeakerIdEnrollment(
    bool skip_cloud_enrollment,
    mojom::SpeakerIdEnrollmentClientPtr client) {
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());

  speaker_id_enrollment_client_ = std::move(client);

  assistant_client::SpeakerIdEnrollmentConfig client_config;
  client_config.user_id = kUserID;
  client_config.skip_cloud_enrollment = skip_cloud_enrollment;

  assistant_manager_internal_->StartSpeakerIdEnrollment(client_config, [
    weak_ptr = weak_factory_.GetWeakPtr(),
    task_runner = main_thread_task_runner_
  ](const assistant_client::SpeakerIdEnrollmentUpdate& update) {
    task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(
            &AssistantManagerServiceImpl::HandleSpeakerIdEnrollmentUpdate,
            weak_ptr, update));
  });
}

void AssistantManagerServiceImpl::StopSpeakerIdEnrollment(
    AssistantSettingsManager::StopSpeakerIdEnrollmentCallback callback) {
  assistant_manager_internal_->StopSpeakerIdEnrollment([
    repeating_callback = base::AdaptCallbackForRepeating(std::move(callback)),
    task_runner = main_thread_task_runner_,
    weak_ptr = weak_factory_.GetWeakPtr()
  ]() {
    task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(
            &AssistantManagerServiceImpl::HandleStopSpeakerIdEnrollment,
            std::move(weak_ptr), repeating_callback));
  });
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

void AssistantManagerServiceImpl::StartCachedScreenContextInteraction() {
  if (!assistant_enabled_ || !context_enabled_)
    return;

  // It is illegal to call this method without having first cached screen
  // context (see CacheScreenContext()).
  DCHECK(assistant_extra_);
  DCHECK(assistant_tree_);
  DCHECK(!assistant_screenshot_.empty());

  SendScreenContextRequest(std::move(assistant_extra_),
                           std::move(assistant_tree_), assistant_screenshot_);
}

void AssistantManagerServiceImpl::StartMetalayerInteraction(
    const gfx::Rect& region) {
  if (!assistant_enabled_ || !context_enabled_)
    return;

  service_->assistant_controller()->RequestScreenshot(
      region,
      base::BindOnce(&AssistantManagerServiceImpl::SendScreenContextRequest,
                     weak_factory_.GetWeakPtr(), /*assistant_extra=*/nullptr,
                     /*assistant_tree=*/nullptr));
}

void AssistantManagerServiceImpl::SendTextQuery(const std::string& query) {
  assistant_client::VoicelessOptions options;
  options.is_user_initiated = true;
  options.modality =
      assistant_client::VoicelessOptions::Modality::TYPING_MODALITY;

  std::string interaction = CreateTextQueryInteraction(query);
  assistant_manager_internal_->SendVoicelessInteraction(
      interaction, /*description=*/"text_query", options, [](auto) {});
}

void AssistantManagerServiceImpl::AddAssistantInteractionSubscriber(
    mojom::AssistantInteractionSubscriberPtr subscriber) {
  interaction_subscribers_.AddPtr(std::move(subscriber));
}

void AssistantManagerServiceImpl::AddAssistantNotificationSubscriber(
    mojom::AssistantNotificationSubscriberPtr subscriber) {
  notification_subscribers_.AddPtr(std::move(subscriber));
}

void AssistantManagerServiceImpl::RetrieveNotification(
    mojom::AssistantNotificationPtr notification,
    int action_index) {
  const std::string& notification_id = notification->notification_id;
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
  const std::string& notification_id = notification->notification_id;
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

void AssistantManagerServiceImpl::OnConversationTurnStarted(bool is_mic_open) {
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &AssistantManagerServiceImpl::OnConversationTurnStartedOnMainThread,
          weak_factory_.GetWeakPtr(), is_mic_open));
}

void AssistantManagerServiceImpl::OnConversationTurnFinished(
    Resolution resolution) {
  // TODO(updowndota): Find a better way to handle the edge cases.
  if (resolution != Resolution::NORMAL_WITH_FOLLOW_ON &&
      resolution != Resolution::CANCELLED &&
      resolution != Resolution::BARGE_IN) {
    platform_api_->SetMicState(false);
  }
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &AssistantManagerServiceImpl::OnConversationTurnFinishedOnMainThread,
          weak_factory_.GetWeakPtr(), resolution));
}

// TODO(b/113541754): Deprecate this API when the server provides a fallback.
void AssistantManagerServiceImpl::OnShowContextualQueryFallback() {
  // Show fallback text.
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&AssistantManagerServiceImpl::OnShowTextOnMainThread,
                     weak_factory_.GetWeakPtr(),
                     l10n_util::GetStringUTF8(
                         IDS_ASSISTANT_SCREEN_CONTEXT_QUERY_FALLBACK_TEXT)));

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
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&AssistantManagerServiceImpl::OnShowHtmlOnMainThread,
                     weak_factory_.GetWeakPtr(), html.str(), /*fallback=*/""));
}

void AssistantManagerServiceImpl::OnShowHtml(const std::string& html,
                                             const std::string& fallback) {
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&AssistantManagerServiceImpl::OnShowHtmlOnMainThread,
                     weak_factory_.GetWeakPtr(), html, fallback));
}

void AssistantManagerServiceImpl::OnShowSuggestions(
    const std::vector<action::Suggestion>& suggestions) {
  // Convert to mojom struct for IPC.
  std::vector<mojom::AssistantSuggestionPtr> ptrs;
  for (const action::Suggestion& suggestion : suggestions) {
    mojom::AssistantSuggestionPtr ptr = mojom::AssistantSuggestion::New();
    ptr->text = suggestion.text;
    ptr->icon_url = GURL(suggestion.icon_url);
    ptr->action_url = GURL(suggestion.action_url);
    ptrs.push_back(std::move(ptr));
  }

  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &AssistantManagerServiceImpl::OnShowSuggestionsOnMainThread,
          weak_factory_.GetWeakPtr(), std::move(ptrs)));
}

void AssistantManagerServiceImpl::OnShowText(const std::string& text) {
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&AssistantManagerServiceImpl::OnShowTextOnMainThread,
                     weak_factory_.GetWeakPtr(), text));
}

void AssistantManagerServiceImpl::OnOpenUrl(const std::string& url) {
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&AssistantManagerServiceImpl::OnOpenUrlOnMainThread,
                     weak_factory_.GetWeakPtr(), url));
}

void AssistantManagerServiceImpl::OnShowNotification(
    const action::Notification& notification) {
  mojom::AssistantNotificationPtr notification_ptr =
      mojom::AssistantNotification::New();
  notification_ptr->title = notification.title;
  notification_ptr->message = notification.text;
  notification_ptr->action_url = GURL(notification.action_url);
  notification_ptr->notification_id = notification.notification_id;
  notification_ptr->consistency_token = notification.consistency_token;
  notification_ptr->opaque_token = notification.opaque_token;
  notification_ptr->grouping_key = notification.grouping_key;
  notification_ptr->obfuscated_gaia_id = notification.obfuscated_gaia_id;
  for (const auto& button : notification.buttons) {
    notification_ptr->buttons.push_back(mojom::AssistantNotificationButton::New(
        button.label, GURL(button.action_url)));
  }

  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &AssistantManagerServiceImpl::OnShowNotificationOnMainThread,
          weak_factory_.GetWeakPtr(), std::move(notification_ptr)));
}

void AssistantManagerServiceImpl::OnRecognitionStateChanged(
    assistant_client::ConversationStateListener::RecognitionState state,
    const assistant_client::ConversationStateListener::RecognitionResult&
        recognition_result) {
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &AssistantManagerServiceImpl::OnRecognitionStateChangedOnMainThread,
          weak_factory_.GetWeakPtr(), state, recognition_result));
}

void AssistantManagerServiceImpl::OnRespondingStarted(bool is_error_response) {
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &AssistantManagerServiceImpl::OnRespondingStartedOnMainThread,
          weak_factory_.GetWeakPtr(), is_error_response));
}

void AssistantManagerServiceImpl::OnSpeechLevelUpdated(
    const float speech_level) {
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &AssistantManagerServiceImpl::OnSpeechLevelUpdatedOnMainThread,
          weak_factory_.GetWeakPtr(), speech_level));
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

  if (modify_setting_args.setting_id() == kWiFiDeviceSettingId) {
    HandleOnOffChange(modify_setting_args, [&](bool enabled) {
      this->service_->device_actions()->SetWifiEnabled(enabled);
    });
  }

  if (modify_setting_args.setting_id() == kBluetoothDeviceSettingId) {
    HandleOnOffChange(modify_setting_args, [&](bool enabled) {
      this->service_->device_actions()->SetBluetoothEnabled(enabled);
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
    this->service_->device_actions()->GetScreenBrightnessLevel(base::BindOnce(
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
                this_->service_->device_actions()->SetScreenBrightnessLevel(
                    new_value, true);
              },
              [&]() { return current_value; });
        },
        weak_factory_.GetWeakPtr(), modify_setting_args));
  }

  if (modify_setting_args.setting_id() == kDoNotDisturbDeviceSettingId) {
    HandleOnOffChange(modify_setting_args, [&](bool enabled) {
      ash_message_center_controller_->SetQuietMode(enabled);
    });
  }

  if (modify_setting_args.setting_id() == kNightLightDeviceSettingId) {
    HandleOnOffChange(modify_setting_args, [&](bool enabled) {
      this->service_->device_actions()->SetNightLightEnabled(enabled);
    });
  }
}

ActionModule::Result AssistantManagerServiceImpl::HandleModifySettingClientOp(
    const std::string& modify_setting_args_proto) {
  DVLOG(2) << "HandleModifySettingClientOp=" << modify_setting_args_proto;
  main_thread_task_runner_->PostTask(
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
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &AssistantManagerServiceImpl::OnNotificationRemovedOnMainThread,
          weak_factory_.GetWeakPtr(), grouping_key));
}

void AssistantManagerServiceImpl::OnCommunicationError(int error_code) {
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &AssistantManagerServiceImpl::OnCommunicationErrorOnMainThread,
          weak_factory_.GetWeakPtr(), error_code));
}

void AssistantManagerServiceImpl::OnVoiceInteractionSettingsEnabled(
    bool enabled) {
  assistant_enabled_ = enabled;
}

void AssistantManagerServiceImpl::OnVoiceInteractionContextEnabled(
    bool enabled) {
  context_enabled_ = enabled;
}

void AssistantManagerServiceImpl::OnVoiceInteractionHotwordEnabled(
    bool enabled) {
  enable_hotword_ = enabled;
  platform_api_->OnHotwordEnabled(enabled);
}

void AssistantManagerServiceImpl::OnLocaleChanged(const std::string& locale) {
  if (locale == locale_)
    return;

  locale_ = locale;

  // When |locale_| changes we need to update our internal options to
  // synchronize our LibAssistant locale configuration.
  if (assistant_manager_internal_) {
    background_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&AssistantManagerServiceImpl::UpdateInternalOptions,
                       weak_factory_.GetWeakPtr()));
  }
}

void AssistantManagerServiceImpl::StartAssistantInternal(
    const std::string& access_token,
    const std::string& arc_version) {
  DCHECK(background_thread_.task_runner()->BelongsToCurrentThread());

  arc_version_ = arc_version;

  assistant_manager_.reset(assistant_client::AssistantManager::Create(
      platform_api_.get(), CreateLibAssistantConfig(!enable_hotword_)));
  assistant_manager_internal_ =
      UnwrapAssistantManagerInternal(assistant_manager_.get());

  UpdateInternalOptions();

  assistant_manager_internal_->SetDisplayConnection(display_connection_.get());
  assistant_manager_internal_->RegisterActionModule(action_module_.get());
  assistant_manager_internal_->SetAssistantManagerDelegate(this);
  assistant_manager_->AddConversationStateListener(this);
  assistant_manager_->AddDeviceStateListener(this);

  std::vector<std::string> server_experiment_ids;
  FillServerExperimentIds(server_experiment_ids);

  if (server_experiment_ids.size() > 0)
    assistant_manager_internal_->AddExtraExperimentIds(server_experiment_ids);

  SetAccessToken(access_token);

  assistant_manager_->Start();
}

void AssistantManagerServiceImpl::PostInitAssistant(
    base::OnceClosure post_init_callback) {
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());

  state_ = State::RUNNING;

  const base::TimeDelta time_since_started =
      base::TimeTicks::Now() - started_time_;
  UMA_HISTOGRAM_TIMES("Assistant.ServiceStartTime", time_since_started);

  std::move(post_init_callback).Run();
  UpdateDeviceSettings();
}

std::string AssistantManagerServiceImpl::BuildUserAgent(
    const std::string& arc_version) const {
  std::string user_agent;
  base::StringAppendF(&user_agent,
                      "Mozilla/5.0 (X11; CrOS %s %s; %s) "
                      "AppleWebKit/%d.%d (KHTML, like Gecko)",
                      base::SysInfo::OperatingSystemArchitecture().c_str(),
                      base::SysInfo::OperatingSystemVersion().c_str(),
                      base::SysInfo::GetLsbReleaseBoard().c_str(),
                      WEBKIT_VERSION_MAJOR, WEBKIT_VERSION_MINOR);

  if (!arc_version.empty()) {
    base::StringAppendF(&user_agent, " ARC/%s", arc_version.c_str());
  }
  return user_agent;
}

void AssistantManagerServiceImpl::UpdateDeviceSettings() {
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());

  const std::string device_id = assistant_manager_->GetDeviceId();
  if (device_id.empty())
    return;

  // Update device id and device type.
  assistant::SettingsUiUpdate update;
  assistant::AssistantDeviceSettingsUpdate* device_settings_update =
      update.mutable_assistant_device_settings_update()
          ->add_assistant_device_settings_update();
  device_settings_update->set_device_id(device_id);
  device_settings_update->set_assistant_device_type(
      assistant::AssistantDevice::CROS);

  VLOG(1) << "Update assistant device locale: "
          << base::i18n::GetConfiguredLocale();
  device_settings_update->mutable_device_settings()->set_locale(
      locale_.empty() ? base::i18n::GetConfiguredLocale() : locale_);

  // Device settings update result is not handled because it is not included in
  // the SettingsUiUpdateResult.
  SendUpdateSettingsUiRequest(update.SerializeAsString(), base::DoNothing());
}

void AssistantManagerServiceImpl::UpdateInternalOptions() {
  DCHECK(background_thread_.task_runner()->BelongsToCurrentThread());

  if (!assistant_manager_internal_)
    return;

  auto* internal_options =
      assistant_manager_internal_->CreateDefaultInternalOptions();
  SetAssistantOptions(internal_options, BuildUserAgent(arc_version_.value()),
                      locale_, spoken_feedback_enabled_);
  assistant_manager_internal_->SetOptions(*internal_options, [](bool success) {
    DVLOG(2) << "set options: " << success;
  });
}

void AssistantManagerServiceImpl::HandleGetSettingsResponse(
    base::RepeatingCallback<void(const std::string&)> callback,
    const std::string& settings) {
  callback.Run(settings);
}

void AssistantManagerServiceImpl::HandleUpdateSettingsResponse(
    base::RepeatingCallback<void(const std::string&)> callback,
    const std::string& result) {
  callback.Run(result);
}

void AssistantManagerServiceImpl::HandleSpeakerIdEnrollmentUpdate(
    const assistant_client::SpeakerIdEnrollmentUpdate& update) {
  switch (update.state) {
    case SpeakerIdEnrollmentState::LISTEN:
      speaker_id_enrollment_client_->OnListeningHotword();
      break;
    case SpeakerIdEnrollmentState::PROCESS:
      speaker_id_enrollment_client_->OnProcessingHotword();
      break;
    case SpeakerIdEnrollmentState::DONE:
      speaker_id_enrollment_client_->OnSpeakerIdEnrollmentDone();
      break;
    case SpeakerIdEnrollmentState::FAILURE:
      speaker_id_enrollment_client_->OnSpeakerIdEnrollmentFailure();
      break;
    case SpeakerIdEnrollmentState::INIT:
    case SpeakerIdEnrollmentState::CHECK:
    case SpeakerIdEnrollmentState::UPLOAD:
    case SpeakerIdEnrollmentState::FETCH:
      break;
  }
}

void AssistantManagerServiceImpl::HandleStopSpeakerIdEnrollment(
    base::RepeatingCallback<void()> callback) {
  speaker_id_enrollment_client_.reset();
  callback.Run();
}

// assistant_client::DeviceStateListener overrides
// Run on LibAssistant threads
void AssistantManagerServiceImpl::OnStartFinished() {
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&AssistantManagerServiceImpl::RegisterFallbackMediaHandler,
                     weak_factory_.GetWeakPtr()));
}

void AssistantManagerServiceImpl::OnTimerSoundingStarted() {
  // TODO(llin): Migrate to use the AlarmManager API to better support multiple
  // timers when the API is available.
  if (!base::FeatureList::IsEnabled(kAssistantTimerNotificationFeature))
    return;

  const std::string notification_title =
      l10n_util::GetStringUTF8(IDS_ASSISTANT_TIMER_NOTIFICATION_TITLE);
  const std::string notification_content =
      l10n_util::GetStringUTF8(IDS_ASSISTANT_TIMER_NOTIFICATION_CONTENT);
  const std::string stop_timer_query =
      l10n_util::GetStringUTF8(IDS_ASSISTANT_STOP_TIMER_QUERY);

  action::Notification notification(
      /*title=*/notification_title,
      /*text=*/notification_content,
      /*action_url=*/kQueryDeeplinkPrefix + stop_timer_query,
      /*notification_id=*/{},
      /*consistency_token=*/{},
      /*opaque_token=*/{},
      /*grouping_key=*/kTimerFireNotificationGroupId,
      /*obfuscated_gaia_id=*/{});
  OnShowNotification(notification);
}

void AssistantManagerServiceImpl::OnTimerSoundingFinished() {
  if (!base::FeatureList::IsEnabled(kAssistantTimerNotificationFeature))
    return;

  OnNotificationRemoved(kTimerFireNotificationGroupId);
}

void AssistantManagerServiceImpl::OnConversationTurnStartedOnMainThread(
    bool is_mic_open) {
  interaction_subscribers_.ForAllPtrs([is_mic_open](auto* ptr) {
    ptr->OnInteractionStarted(/*is_voice_interaction=*/is_mic_open);
  });
}

void AssistantManagerServiceImpl::OnConversationTurnFinishedOnMainThread(
    Resolution resolution) {
  switch (resolution) {
    // Interaction ended normally.
    case Resolution::NORMAL:
    case Resolution::NORMAL_WITH_FOLLOW_ON:
    case Resolution::NO_RESPONSE:
      interaction_subscribers_.ForAllPtrs([](auto* ptr) {
        ptr->OnInteractionFinished(
            mojom::AssistantInteractionResolution::kNormal);
      });
      break;
    // Interaction ended due to interruption.
    case Resolution::BARGE_IN:
    case Resolution::CANCELLED:
      interaction_subscribers_.ForAllPtrs([](auto* ptr) {
        ptr->OnInteractionFinished(
            mojom::AssistantInteractionResolution::kInterruption);
      });
      break;
    // Interaction ended due to mic timeout.
    case Resolution::TIMEOUT:
      interaction_subscribers_.ForAllPtrs([](auto* ptr) {
        ptr->OnInteractionFinished(
            mojom::AssistantInteractionResolution::kMicTimeout);
      });
      break;
    // Interaction ended due to error.
    case Resolution::COMMUNICATION_ERROR:
      interaction_subscribers_.ForAllPtrs([](auto* ptr) {
        ptr->OnInteractionFinished(
            mojom::AssistantInteractionResolution::kError);
      });
      break;
    // Interaction ended because the device was not selected to produce a
    // response. This occurs due to multi-device hotword loss.
    case Resolution::DEVICE_NOT_SELECTED:
      interaction_subscribers_.ForAllPtrs([](auto* ptr) {
        ptr->OnInteractionFinished(
            mojom::AssistantInteractionResolution::kMultiDeviceHotwordLoss);
      });
      break;
  }
}

void AssistantManagerServiceImpl::OnShowHtmlOnMainThread(
    const std::string& html,
    const std::string& fallback) {
  interaction_subscribers_.ForAllPtrs(
      [&html, &fallback](auto* ptr) { ptr->OnHtmlResponse(html, fallback); });
}

void AssistantManagerServiceImpl::OnShowSuggestionsOnMainThread(
    const std::vector<mojom::AssistantSuggestionPtr>& suggestions) {
  interaction_subscribers_.ForAllPtrs([&suggestions](auto* ptr) {
    ptr->OnSuggestionsResponse(mojo::Clone(suggestions));
  });
}

void AssistantManagerServiceImpl::OnShowTextOnMainThread(
    const std::string& text) {
  interaction_subscribers_.ForAllPtrs(
      [&text](auto* ptr) { ptr->OnTextResponse(text); });
}

void AssistantManagerServiceImpl::OnOpenUrlOnMainThread(
    const std::string& url) {
  interaction_subscribers_.ForAllPtrs(
      [&url](auto* ptr) { ptr->OnOpenUrlResponse(GURL(url)); });
}

void AssistantManagerServiceImpl::OnShowNotificationOnMainThread(
    const mojom::AssistantNotificationPtr& notification) {
  notification_subscribers_.ForAllPtrs([&notification](auto* ptr) {
    ptr->OnShowNotification(notification.Clone());
  });
}

void AssistantManagerServiceImpl::OnNotificationRemovedOnMainThread(
    const std::string& grouping_key) {
  notification_subscribers_.ForAllPtrs(
      [grouping_key](auto* ptr) { ptr->OnRemoveNotification(grouping_key); });
}

void AssistantManagerServiceImpl::OnCommunicationErrorOnMainThread(
    int error_code) {
  if (IsAuthError(error_code))
    service_->RequestAccessToken();
}

void AssistantManagerServiceImpl::OnRecognitionStateChangedOnMainThread(
    assistant_client::ConversationStateListener::RecognitionState state,
    const assistant_client::ConversationStateListener::RecognitionResult&
        recognition_result) {
  switch (state) {
    case assistant_client::ConversationStateListener::RecognitionState::STARTED:
      interaction_subscribers_.ForAllPtrs(
          [](auto* ptr) { ptr->OnSpeechRecognitionStarted(); });
      break;
    case assistant_client::ConversationStateListener::RecognitionState::
        INTERMEDIATE_RESULT:
      interaction_subscribers_.ForAllPtrs([&recognition_result](auto* ptr) {
        ptr->OnSpeechRecognitionIntermediateResult(
            recognition_result.high_confidence_text,
            recognition_result.low_confidence_text);
      });
      break;
    case assistant_client::ConversationStateListener::RecognitionState::
        END_OF_UTTERANCE:
      interaction_subscribers_.ForAllPtrs(
          [](auto* ptr) { ptr->OnSpeechRecognitionEndOfUtterance(); });
      break;
    case assistant_client::ConversationStateListener::RecognitionState::
        FINAL_RESULT:
      interaction_subscribers_.ForAllPtrs([&recognition_result](auto* ptr) {
        ptr->OnSpeechRecognitionFinalResult(
            recognition_result.recognized_speech);
      });
      break;
  }
}

void AssistantManagerServiceImpl::OnRespondingStartedOnMainThread(
    bool is_error_response) {
  interaction_subscribers_.ForAllPtrs(
      [is_error_response](auto* ptr) { ptr->OnTtsStarted(is_error_response); });
}

void AssistantManagerServiceImpl::OnSpeechLevelUpdatedOnMainThread(
    const float speech_level) {
  interaction_subscribers_.ForAllPtrs(
      [&speech_level](auto* ptr) { ptr->OnSpeechLevelUpdated(speech_level); });
}

void AssistantManagerServiceImpl::CacheScreenContext(
    CacheScreenContextCallback callback) {
  if (!assistant_enabled_ || !context_enabled_) {
    std::move(callback).Run();
    return;
  }

  // Our callback should be run only after both view hierarchy and screenshot
  // data have been cached from their respective providers.
  auto on_done =
      base::BarrierClosure(2, base::BindOnce(
                                  [](CacheScreenContextCallback callback) {
                                    std::move(callback).Run();
                                  },
                                  base::Passed(std::move(callback))));

  service_->client()->RequestAssistantStructure(
      base::BindOnce(&AssistantManagerServiceImpl::CacheAssistantStructure,
                     weak_factory_.GetWeakPtr(), on_done));

  service_->assistant_controller()->RequestScreenshot(
      gfx::Rect(),
      base::BindOnce(&AssistantManagerServiceImpl::CacheAssistantScreenshot,
                     weak_factory_.GetWeakPtr(), on_done));
}

void AssistantManagerServiceImpl::OnAccessibilityStatusChanged(
    bool spoken_feedback_enabled) {
  if (spoken_feedback_enabled_ == spoken_feedback_enabled)
    return;

  spoken_feedback_enabled_ = spoken_feedback_enabled;

  // When |spoken_feedback_enabled_| changes we need to update our internal
  // options to turn on/off A11Y features in LibAssistant.
  if (assistant_manager_internal_) {
    background_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&AssistantManagerServiceImpl::UpdateInternalOptions,
                       weak_factory_.GetWeakPtr()));
  }
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
    ax::mojom::AssistantExtraPtr assistant_extra,
    std::unique_ptr<ui::AssistantTree> assistant_tree,
    const std::vector<uint8_t>& assistant_screenshot) {
  std::vector<std::string> context_protos;

  // Screen context can have the assistant_extra and assistant_tree set to
  // nullptr. This happens in the case where the screen context is coming from
  // the metalayer. For this scenario, we don't create a context proto for the
  // AssistantBundle that consists of the assistant_extra and assistant_tree.
  if (assistant_extra && assistant_tree) {
    context_protos.emplace_back(CreateContextProto(AssistantBundle{
        std::move(assistant_extra), std::move(assistant_tree)}));
  }

  context_protos.emplace_back(CreateContextProto(assistant_screenshot));
  assistant_manager_internal_->SendScreenContextRequest(context_protos);
  assistant_screenshot_.clear();
}

std::string AssistantManagerServiceImpl::GetLastSearchSource() {
  base::AutoLock lock(last_search_source_lock_);
  auto search_source = last_search_source_;
  last_search_source_ = std::string();
  return search_source;
}

void AssistantManagerServiceImpl::FillServerExperimentIds(
    std::vector<std::string>& server_experiment_ids) {
  if (base::FeatureList::IsEnabled(kChromeOSAssistantDogfood)) {
    server_experiment_ids.emplace_back(kServersideDogfoodExperimentId);
  }
}

}  // namespace assistant
}  // namespace chromeos
