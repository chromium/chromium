// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/assistant_manager_service_impl.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/assistant/assistant_state.h"
#include "ash/public/cpp/assistant/assistant_state_base.h"
#include "ash/public/cpp/assistant/controller/assistant_notification_controller.h"
#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "chromeos/dbus/dlcservice/dlcservice_client.h"
#include "chromeos/dbus/util/version_loader.h"
#include "chromeos/services/assistant/device_settings_host.h"
#include "chromeos/services/assistant/libassistant_service_host_impl.h"
#include "chromeos/services/assistant/media_host.h"
#include "chromeos/services/assistant/platform/audio_input_host_impl.h"
#include "chromeos/services/assistant/platform/audio_output_delegate_impl.h"
#include "chromeos/services/assistant/platform/platform_delegate_impl.h"
#include "chromeos/services/assistant/public/cpp/assistant_browser_delegate.h"
#include "chromeos/services/assistant/public/cpp/assistant_enums.h"
#include "chromeos/services/assistant/public/cpp/device_actions.h"
#include "chromeos/services/assistant/public/cpp/features.h"
#include "chromeos/services/assistant/public/shared/utils.h"
#include "chromeos/services/assistant/service_context.h"
#include "chromeos/services/assistant/timer_host.h"
#include "chromeos/services/libassistant/public/mojom/android_app_info.mojom.h"
#include "chromeos/services/libassistant/public/mojom/speech_recognition_observer.mojom.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "ui/accessibility/mojom/ax_assistant_structure.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

using media_session::mojom::MediaSessionAction;

namespace chromeos {
namespace assistant {
namespace {

static base::OnceCallback<void()> initialized_internal_callback_for_testing;
static bool is_first_init = true;

constexpr char kAndroidSettingsAppPackage[] = "com.android.settings";

// The DLC ID of Libassistant.so, used to download and mount the library.
constexpr char kLibassistantDlcId[] = "assistant-dlc";

std::vector<chromeos::libassistant::mojom::AuthenticationTokenPtr>
ToAuthenticationTokens(
    const absl::optional<AssistantManagerService::UserInfo>& user) {
  std::vector<chromeos::libassistant::mojom::AuthenticationTokenPtr> result;

  if (user.has_value()) {
    DCHECK(!user.value().gaia_id.empty());
    DCHECK(!user.value().access_token.empty());
    result.emplace_back(libassistant::mojom::AuthenticationToken::New(
        /*gaia_id=*/user.value().gaia_id,
        /*access_token=*/user.value().access_token));
  }

  return result;
}

chromeos::libassistant::mojom::BootupConfigPtr CreateBootupConfig(
    const absl::optional<std::string>& s3_server_uri_override,
    const absl::optional<std::string>& device_id_override) {
  auto result = chromeos::libassistant::mojom::BootupConfig::New();
  result->s3_server_uri_override = s3_server_uri_override;
  result->device_id_override = device_id_override;
  return result;
}

}  // namespace

// Observer that will receive all speech recognition related events,
// and forwards them to all |AssistantInteractionSubscriber|.
class SpeechRecognitionObserverWrapper
    : public chromeos::libassistant::mojom::SpeechRecognitionObserver {
 public:
  explicit SpeechRecognitionObserverWrapper(
      const base::ObserverList<AssistantInteractionSubscriber>* observers)
      : interaction_subscribers_(*observers) {
    DCHECK(observers);
  }
  SpeechRecognitionObserverWrapper(const SpeechRecognitionObserverWrapper&) =
      delete;
  SpeechRecognitionObserverWrapper& operator=(
      const SpeechRecognitionObserverWrapper&) = delete;
  ~SpeechRecognitionObserverWrapper() override = default;

  mojo::PendingRemote<chromeos::libassistant::mojom::SpeechRecognitionObserver>
  BindNewPipeAndPassRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  // chromeos::libassistant::mojom::SpeechRecognitionObserver implementation:
  void OnSpeechLevelUpdated(float speech_level_in_decibels) override {
    for (auto& it : interaction_subscribers_)
      it.OnSpeechLevelUpdated(speech_level_in_decibels);
  }

  void OnSpeechRecognitionStart() override {
    for (auto& it : interaction_subscribers_)
      it.OnSpeechRecognitionStarted();
  }

  void OnIntermediateResult(const std::string& high_confidence_text,
                            const std::string& low_confidence_text) override {
    for (auto& it : interaction_subscribers_) {
      it.OnSpeechRecognitionIntermediateResult(high_confidence_text,
                                               low_confidence_text);
    }
  }

  void OnSpeechRecognitionEnd() override {
    for (auto& it : interaction_subscribers_)
      it.OnSpeechRecognitionEndOfUtterance();
  }

  void OnFinalResult(const std::string& recognized_text) override {
    for (auto& it : interaction_subscribers_)
      it.OnSpeechRecognitionFinalResult(recognized_text);
  }

 private:
  // Owned by our parent, |AssistantManagerServiceImpl|.
  const base::ObserverList<AssistantInteractionSubscriber>&
      interaction_subscribers_;

  mojo::Receiver<chromeos::libassistant::mojom::SpeechRecognitionObserver>
      receiver_{this};
};

// static
void AssistantManagerServiceImpl::SetInitializedInternalCallbackForTesting(
    base::OnceCallback<void()> callback) {
  CHECK(initialized_internal_callback_for_testing.is_null());
  // We expect that the callback is set when AssistantStatus is NOT_READY to
  // confirm that AssistantStatus has changed from NOT_READY to READY. See more
  // details at a comment in AssistantManagerServiceImpl::OnDeviceAppsEnabled.
  CHECK(ash::AssistantState::Get()->assistant_status() ==
        chromeos::assistant::AssistantStatus::NOT_READY);
  initialized_internal_callback_for_testing = std::move(callback);
}

// static
void AssistantManagerServiceImpl::ResetIsFirstInitFlagForTesting() {
  is_first_init = true;
}

AssistantManagerServiceImpl::AssistantManagerServiceImpl(
    ServiceContext* context,
    std::unique_ptr<network::PendingSharedURLLoaderFactory>
        pending_url_loader_factory,
    absl::optional<std::string> s3_server_uri_override,
    absl::optional<std::string> device_id_override,
    std::unique_ptr<LibassistantServiceHost> libassistant_service_host)
    : assistant_settings_(std::make_unique<AssistantSettingsImpl>(context)),
      assistant_host_(std::make_unique<AssistantHost>()),
      platform_delegate_(std::make_unique<PlatformDelegateImpl>()),
      context_(context),
      device_settings_host_(std::make_unique<DeviceSettingsHost>(context)),
      media_host_(std::make_unique<MediaHost>(AssistantBrowserDelegate::Get(),
                                              &interaction_subscribers_)),
      timer_host_(std::make_unique<TimerHost>(context)),
      audio_output_delegate_(std::make_unique<AudioOutputDelegateImpl>(
          &media_host_->media_session())),
      speech_recognition_observer_(
          std::make_unique<SpeechRecognitionObserverWrapper>(
              &interaction_subscribers_)),
      bootup_config_(
          CreateBootupConfig(s3_server_uri_override, device_id_override)),
      url_loader_factory_(network::SharedURLLoaderFactory::Create(
          std::move(pending_url_loader_factory))),
      weak_factory_(this) {
  if (libassistant_service_host) {
    // During unittests a custom host is passed in, so we'll use that one.
    libassistant_service_host_ = std::move(libassistant_service_host);
  } else {
    // Use the default service host if none was provided.
    libassistant_service_host_ =
        std::make_unique<LibassistantServiceHostImpl>();
  }

  assistant_host_->Initialize(libassistant_service_host_.get());

  service_controller().AddAndFireStateObserver(
      state_observer_receiver_.BindNewPipeAndPassRemote());
  assistant_host_->AddSpeechRecognitionObserver(
      speech_recognition_observer_->BindNewPipeAndPassRemote());
  AddRemoteConversationObserver(this);

  audio_output_delegate_->Bind(assistant_host_->ExtractAudioOutputDelegate());
  platform_delegate_->Bind(assistant_host_->ExtractPlatformDelegate());
  audio_input_host_ = std::make_unique<AudioInputHostImpl>(
      assistant_host_->ExtractAudioInputController(),
      context_->cras_audio_handler(), context_->power_manager_client(),
      context_->assistant_state()->locale().value());

  assistant_settings_->Initialize(
      assistant_host_->ExtractSpeakerIdEnrollmentController(),
      &assistant_host_->settings_controller());

  media_host_->Initialize(&assistant_host_->media_controller(),
                          assistant_host_->ExtractMediaDelegate());
  timer_host_->Initialize(&assistant_host_->timer_controller(),
                          assistant_host_->ExtractTimerDelegate());

  device_settings_host_->Bind(assistant_host_->ExtractDeviceSettingsDelegate());
}

AssistantManagerServiceImpl::~AssistantManagerServiceImpl() {
  // Destroy the Assistant Proxy first so the background thread is flushed
  // before any of the other objects are destroyed.
  assistant_host_ = nullptr;
}

void AssistantManagerServiceImpl::Start(const absl::optional<UserInfo>& user,
                                        bool enable_hotword) {
  DCHECK(!IsServiceStarted());
  DCHECK_EQ(GetState(), State::STOPPED);

  // Set the flag to avoid starting the service multiple times.
  SetStateAndInformObservers(State::STARTING);

  started_time_ = base::TimeTicks::Now();

  EnableHotword(enable_hotword);

  // Install libassistant.so from DLC.
  // TODO(b/225063204): For phase 1, fallback to load libassistant.so from
  // rootfs if installabtion failed. No error handling needed.
  auto* client = chromeos::DlcserviceClient::Get();
  if (!client) {
    InitAssistant(user, /*dlc_path=*/std::string());
    return;
  }

  DVLOG(1) << "Install libassistant.so from DLC";
  dlcservice::InstallRequest install_request;
  install_request.set_id(kLibassistantDlcId);
  client->Install(
      install_request,
      base::BindOnce(&AssistantManagerServiceImpl::OnInstallDlcComplete,
                     weak_factory_.GetWeakPtr(), user),
      /*ProgressCallback=*/base::DoNothing());
}

void AssistantManagerServiceImpl::Stop() {
  // We cannot cleanly stop the service if it is in the process of starting up.
  DCHECK_NE(GetState(), State::STARTING);

  SetStateAndInformObservers(State::STOPPED);

  media_host_->Stop();
  scoped_app_list_event_subscriber_.Reset();

  // When user disables the feature, we also delete all data.
  if (!assistant_state()->settings_enabled().value())
    service_controller().ResetAllDataAndStop();
  else
    service_controller().Stop();
}

AssistantManagerService::State AssistantManagerServiceImpl::GetState() const {
  return state_;
}

void AssistantManagerServiceImpl::SetUser(
    const absl::optional<UserInfo>& user) {
  if (!IsServiceStarted())
    return;

  VLOG(1) << "Set user information (Gaia ID and access token).";
  settings_controller().SetAuthenticationTokens(ToAuthenticationTokens(user));
}

void AssistantManagerServiceImpl::EnableListening(bool enable) {
  settings_controller().SetListeningEnabled(enable);
}

void AssistantManagerServiceImpl::EnableHotword(bool enable) {
  audio_input_host_->OnHotwordEnabled(enable);
}

void AssistantManagerServiceImpl::SetArcPlayStoreEnabled(bool enable) {
  DCHECK(GetState() == State::RUNNING);
  if (assistant::features::IsAppSupportEnabled())
    display_controller().SetArcPlayStoreEnabled(enable);
}

void AssistantManagerServiceImpl::SetAssistantContextEnabled(bool enable) {
  DCHECK(GetState() == State::RUNNING);

  media_host_->SetRelatedInfoEnabled(enable);
  display_controller().SetRelatedInfoEnabled(enable);
}

AssistantSettings* AssistantManagerServiceImpl::GetAssistantSettings() {
  return assistant_settings_.get();
}

void AssistantManagerServiceImpl::AddAuthenticationStateObserver(
    AuthenticationStateObserver* observer) {
  DCHECK(observer);
  assistant_host_->AddAuthenticationStateObserver(
      observer->BindNewPipeAndPassRemote());
}

void AssistantManagerServiceImpl::AddAndFireStateObserver(
    AssistantManagerService::StateObserver* observer) {
  state_observers_.AddObserver(observer);
  observer->OnStateChanged(GetState());
}

void AssistantManagerServiceImpl::RemoveStateObserver(
    const AssistantManagerService::StateObserver* observer) {
  state_observers_.RemoveObserver(observer);
}

void AssistantManagerServiceImpl::SyncDeviceAppsStatus() {
  assistant_settings_->SyncDeviceAppsStatus(
      base::BindOnce(&AssistantManagerServiceImpl::OnDeviceAppsEnabled,
                     weak_factory_.GetWeakPtr()));
}

void AssistantManagerServiceImpl::UpdateInternalMediaPlayerStatus(
    MediaSessionAction action) {
  if (action == MediaSessionAction::kPause) {
    media_host_->PauseInternalMediaPlayer();
  } else if (action == MediaSessionAction::kPlay) {
    media_host_->ResumeInternalMediaPlayer();
  }
}

void AssistantManagerServiceImpl::StartVoiceInteraction() {
  DVLOG(1) << __func__;

  audio_input_host_->SetMicState(true);
  conversation_controller().StartVoiceInteraction();
}

void AssistantManagerServiceImpl::StopActiveInteraction(
    bool cancel_conversation) {
  DVLOG(1) << __func__;

  audio_input_host_->SetMicState(false);
  conversation_controller().StopActiveInteraction(cancel_conversation);
}

void AssistantManagerServiceImpl::StartEditReminderInteraction(
    const std::string& client_id) {
  conversation_controller().StartEditReminderInteraction(client_id);
}

void AssistantManagerServiceImpl::StartScreenContextInteraction(
    const std::vector<uint8_t>& assistant_screenshot) {
  conversation_controller().StartScreenContextInteraction(nullptr,
                                                          assistant_screenshot);
}

void AssistantManagerServiceImpl::StartTextInteraction(
    const std::string& query,
    AssistantQuerySource source,
    bool allow_tts) {
  DVLOG(1) << __func__;

  conversation_controller().SendTextQuery(query, source, allow_tts);
}

void AssistantManagerServiceImpl::AddAssistantInteractionSubscriber(
    AssistantInteractionSubscriber* subscriber) {
  // For now, this function is handling events registration for both Mojom
  // side and Assistant service side. All native AssistantInteractionSubscriber
  // should be deprecated and replaced with |ConversationObserver| once the
  // migration is done.
  interaction_subscribers_.AddObserver(subscriber);
  AddRemoteConversationObserver(subscriber);
}

void AssistantManagerServiceImpl::RemoveAssistantInteractionSubscriber(
    AssistantInteractionSubscriber* subscriber) {
  interaction_subscribers_.RemoveObserver(subscriber);
}

void AssistantManagerServiceImpl::RetrieveNotification(
    const AssistantNotification& notification,
    int action_index) {
  conversation_controller().RetrieveNotification(notification, action_index);
}

void AssistantManagerServiceImpl::DismissNotification(
    const AssistantNotification& notification) {
  conversation_controller().DismissNotification(notification);
}

void AssistantManagerServiceImpl::OnInteractionStarted(
    const AssistantInteractionMetadata& metadata) {
  audio_input_host_->OnConversationTurnStarted();
}

void AssistantManagerServiceImpl::OnInteractionFinished(
    AssistantInteractionResolution resolution) {
  switch (resolution) {
    case AssistantInteractionResolution::kNormal:
      RecordQueryResponseTypeUMA();
      return;
    case AssistantInteractionResolution::kInterruption:
      if (HasReceivedQueryResponse())
        RecordQueryResponseTypeUMA();
      return;
    case AssistantInteractionResolution::kMicTimeout:
    case AssistantInteractionResolution::kError:
    case AssistantInteractionResolution::kMultiDeviceHotwordLoss:
      // No action needed.
      return;
  }
}

void AssistantManagerServiceImpl::OnHtmlResponse(const std::string& html,
                                                 const std::string& fallback) {
  receive_inline_response_ = true;
}

void AssistantManagerServiceImpl::OnTextResponse(const std::string& reponse) {
  receive_inline_response_ = true;
}

void AssistantManagerServiceImpl::OnOpenUrlResponse(const GURL& url,
                                                    bool in_background) {
  receive_url_response_ = url.spec();
}

void AssistantManagerServiceImpl::OnStateChanged(
    chromeos::libassistant::mojom::ServiceState new_state) {
  using chromeos::libassistant::mojom::ServiceState;

  DVLOG(1) << "Libassistant service state changed to " << new_state;
  switch (new_state) {
    case ServiceState::kStarted:
      OnServiceStarted();
      break;
    case ServiceState::kRunning:
      OnServiceRunning();
      break;
    case ServiceState::kStopped:
      break;
  }
}

void AssistantManagerServiceImpl::OnInstallDlcComplete(
    const absl::optional<UserInfo>& user,
    const chromeos::DlcserviceClient::InstallResult& result) {
  DVLOG(1) << "Installed libassistant.so from DLC";
  std::string dlc_path;
  if (result.error == dlcservice::kErrorNone) {
    dlc_path = result.root_path;
  }

  InitAssistant(user, dlc_path);
}

void AssistantManagerServiceImpl::InitAssistant(
    const absl::optional<UserInfo>& user,
    const std::string& dlc_path) {
  DCHECK(!IsServiceStarted());

  auto bootup_config = bootup_config_.Clone();
  bootup_config->authentication_tokens = ToAuthenticationTokens(user);
  bootup_config->hotword_enabled = assistant_state()->hotword_enabled().value();
  bootup_config->locale = assistant_state()->locale().value();
  bootup_config->spoken_feedback_enabled = spoken_feedback_enabled_;
  bootup_config->dark_mode_enabled = dark_mode_enabled_;
  bootup_config->dlc_path = dlc_path;

  service_controller().Initialize(std::move(bootup_config),
                                  BindURLLoaderFactory());
  service_controller().Start();
}

base::Thread& AssistantManagerServiceImpl::GetBackgroundThreadForTesting() {
  return background_thread();
}

void AssistantManagerServiceImpl::OnServiceStarted() {
  DCHECK_EQ(GetState(), State::STARTING);

  const base::TimeDelta time_since_started =
      base::TimeTicks::Now() - started_time_;
  UMA_HISTOGRAM_TIMES("Assistant.ServiceStartTime", time_since_started);

  SetStateAndInformObservers(State::STARTED);
}

bool AssistantManagerServiceImpl::IsServiceStarted() const {
  switch (state_) {
    case State::STOPPED:
    case State::STARTING:
      return false;
    case State::STARTED:
    case State::RUNNING:
      return true;
  }
}

mojo::PendingRemote<network::mojom::URLLoaderFactory>
AssistantManagerServiceImpl::BindURLLoaderFactory() {
  mojo::PendingRemote<network::mojom::URLLoaderFactory> pending_remote;
  url_loader_factory_->Clone(pending_remote.InitWithNewPipeAndPassReceiver());
  return pending_remote;
}

void AssistantManagerServiceImpl::OnServiceRunning() {
  // Try to avoid double run by checking |GetState()|.
  if (GetState() == State::RUNNING)
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

  SetAssistantContextEnabled(assistant_state()->IsScreenContextAllowed());

  if (assistant_state()->arc_play_store_enabled().has_value())
    SetArcPlayStoreEnabled(assistant_state()->arc_play_store_enabled().value());

  if (base::FeatureList::IsEnabled(assistant::features::kAssistantAppSupport))
    scoped_app_list_event_subscriber_.Observe(device_actions());
}

void AssistantManagerServiceImpl::OnAndroidAppListRefreshed(
    const std::vector<AndroidAppInfo>& apps_info) {
  DCHECK(GetState() == State::RUNNING);

  std::vector<AndroidAppInfo> filtered_apps_info;
  for (const auto& app_info : apps_info) {
    // TODO(b/146355799): Remove the special handling for Android settings app.
    if (app_info.package_name == kAndroidSettingsAppPackage)
      continue;

    filtered_apps_info.push_back(app_info);
  }
  display_controller().SetAndroidAppList(std::move(filtered_apps_info));
}

void AssistantManagerServiceImpl::OnAccessibilityStatusChanged(
    bool spoken_feedback_enabled) {
  if (spoken_feedback_enabled_ == spoken_feedback_enabled)
    return;

  spoken_feedback_enabled_ = spoken_feedback_enabled;

  // When |spoken_feedback_enabled_| changes we need to update our internal
  // options to turn on/off A11Y features in LibAssistant.
  if (IsServiceStarted())
    settings_controller().SetSpokenFeedbackEnabled(spoken_feedback_enabled_);
}

void AssistantManagerServiceImpl::OnColorModeChanged(bool dark_mode_enabled) {
  if (dark_mode_enabled_ == dark_mode_enabled)
    return;

  dark_mode_enabled_ = dark_mode_enabled;

  if (IsServiceStarted())
    settings_controller().SetDarkModeEnabled(dark_mode_enabled_);
}

void AssistantManagerServiceImpl::OnDeviceAppsEnabled(bool enabled) {
  // The device apps state sync should only be sent after service is running.
  // Check state here to prevent timing issue when the service is restarting.
  if (GetState() != State::RUNNING)
    return;

  display_controller().SetDeviceAppsEnabled(enabled);

  // You can set initialized_internal callback only when AssistantStatus is
  // NOT_READY. Also this line reaches only after GetState() becomes RUNNING
  // (i.e. READY). From that reason, test code can assume that status has
  // changed from NOT_READY to READY between those two points.
  //
  // Test code expects those things when Assistant gets initialized:
  //
  // - Status becomes READY.
  // - All necessary settings are passed to LibAssistant.
  //
  // We update necessary settings after status becomes READY. For now,
  // DeviceAppsEnabled is the only settings update which involves async call.
  // As other settings are sync, if this async call gets completed, we can also
  // assume that all necessary settings are passed to LibAssistant, i.e.
  // initialized.
  if (!initialized_internal_callback_for_testing.is_null()) {
    std::move(initialized_internal_callback_for_testing).Run();
  }
}

void AssistantManagerServiceImpl::AddTimeToTimer(const std::string& id,
                                                 base::TimeDelta duration) {
  timer_host_->AddTimeToTimer(id, duration);
}

void AssistantManagerServiceImpl::PauseTimer(const std::string& id) {
  timer_host_->PauseTimer(id);
}

void AssistantManagerServiceImpl::RemoveAlarmOrTimer(const std::string& id) {
  timer_host_->RemoveTimer(id);
}

void AssistantManagerServiceImpl::ResumeTimer(const std::string& id) {
  timer_host_->ResumeTimer(id);
}

void AssistantManagerServiceImpl::AddRemoteConversationObserver(
    ConversationObserver* observer) {
  conversation_controller().AddRemoteObserver(
      observer->BindNewPipeAndPassRemote());
}

mojo::PendingReceiver<chromeos::libassistant::mojom::NotificationDelegate>
AssistantManagerServiceImpl::GetPendingNotificationDelegate() {
  return assistant_host_->ExtractNotificationDelegate();
}

void AssistantManagerServiceImpl::RecordQueryResponseTypeUMA() {
  AssistantQueryResponseType response_type = GetQueryResponseType();

  UMA_HISTOGRAM_ENUMERATION("Assistant.QueryResponseType", response_type);

  // Reset the flags.
  receive_url_response_.clear();
  receive_inline_response_ = false;
  device_settings_host_->reset_has_setting_changed();
}

bool AssistantManagerServiceImpl::HasReceivedQueryResponse() const {
  return GetQueryResponseType() != AssistantQueryResponseType::kUnspecified;
}

AssistantQueryResponseType AssistantManagerServiceImpl::GetQueryResponseType()
    const {
  if (device_settings_host_->has_setting_changed()) {
    return AssistantQueryResponseType::kDeviceAction;
  } else if (!receive_url_response_.empty()) {
    if (receive_url_response_.find("www.google.com/search?") !=
        std::string::npos) {
      return AssistantQueryResponseType::kSearchFallback;
    } else {
      return AssistantQueryResponseType::kTargetedAction;
    }
  } else if (receive_inline_response_) {
    return AssistantQueryResponseType::kInlineElement;
  }

  return AssistantQueryResponseType::kUnspecified;
}

void AssistantManagerServiceImpl::SendAssistantFeedback(
    const AssistantFeedback& assistant_feedback) {
  conversation_controller().SendAssistantFeedback(assistant_feedback);
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

chromeos::libassistant::mojom::DisplayController&
AssistantManagerServiceImpl::display_controller() {
  return assistant_host_->display_controller();
}

chromeos::libassistant::mojom::ServiceController&
AssistantManagerServiceImpl::service_controller() {
  return assistant_host_->service_controller();
}

void AssistantManagerServiceImpl::SetMicState(bool mic_open) {
  DCHECK(audio_input_host_);
  audio_input_host_->SetMicState(mic_open);
}

chromeos::libassistant::mojom::ConversationController&
AssistantManagerServiceImpl::conversation_controller() {
  return assistant_host_->conversation_controller();
}

chromeos::libassistant::mojom::SettingsController&
AssistantManagerServiceImpl::settings_controller() {
  return assistant_host_->settings_controller();
}

base::Thread& AssistantManagerServiceImpl::background_thread() {
  return assistant_host_->background_thread();
}

void AssistantManagerServiceImpl::SetStateAndInformObservers(State new_state) {
  state_ = new_state;

  for (auto& observer : state_observers_)
    observer.OnStateChanged(state_);
}

}  // namespace assistant
}  // namespace chromeos
