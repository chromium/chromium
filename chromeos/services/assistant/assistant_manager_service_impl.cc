// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/assistant_manager_service_impl.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/assistant/assistant_state_base.h"
#include "ash/public/cpp/assistant/controller/assistant_notification_controller.h"
#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/callback_forward.h"
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
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "chromeos/assistant/internal/internal_constants.h"
#include "chromeos/assistant/internal/proto/google3/assistant/api/client_op/device_args.pb.h"
#include "chromeos/dbus/util/version_loader.h"
#include "chromeos/services/assistant/assistant_device_settings_delegate.h"
#include "chromeos/services/assistant/libassistant_service_host_impl.h"
#include "chromeos/services/assistant/media_host.h"
#include "chromeos/services/assistant/platform/audio_output_delegate_impl.h"
#include "chromeos/services/assistant/platform/platform_delegate_impl.h"
#include "chromeos/services/assistant/proxy/conversation_controller_proxy.h"
#include "chromeos/services/assistant/proxy/service_controller_proxy.h"
#include "chromeos/services/assistant/public/cpp/assistant_client.h"
#include "chromeos/services/assistant/public/cpp/device_actions.h"
#include "chromeos/services/assistant/public/cpp/features.h"
#include "chromeos/services/assistant/public/cpp/migration/assistant_manager_service_delegate.h"
#include "chromeos/services/assistant/public/cpp/migration/audio_input_host.h"
#include "chromeos/services/assistant/public/cpp/migration/libassistant_v1_api.h"
#include "chromeos/services/assistant/public/shared/utils.h"
#include "chromeos/services/assistant/service_context.h"
#include "chromeos/services/assistant/timer_host.h"
#include "chromeos/services/libassistant/public/mojom/android_app_info.mojom.h"
#include "chromeos/services/libassistant/public/mojom/speech_recognition_observer.mojom.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "libassistant/shared/internal_api/assistant_manager_delegate.h"
#include "libassistant/shared/internal_api/assistant_manager_internal.h"
#include "libassistant/shared/public/assistant_manager.h"
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

using media_session::mojom::MediaSessionAction;
using Resolution = assistant_client::ConversationStateListener::Resolution;
using CommunicationErrorType =
    chromeos::assistant::AssistantManagerService::CommunicationErrorType;

namespace api = ::assistant::api;

namespace chromeos {
namespace assistant {
namespace {

static bool is_first_init = true;

constexpr char kAndroidSettingsAppPackage[] = "com.android.settings";

CommunicationErrorType CommunicationErrorTypeFromLibassistantErrorCode(
    int error_code) {
  if (IsAuthError(error_code))
    return CommunicationErrorType::AuthenticationError;
  return CommunicationErrorType::Other;
}

ServiceControllerProxy::AuthTokens ToAuthTokensOrEmpty(
    const base::Optional<AssistantManagerService::UserInfo>& user) {
  if (!user.has_value())
    return {};

  DCHECK(!user.value().gaia_id.empty());
  DCHECK(!user.value().access_token.empty());
  return {std::make_pair(user.value().gaia_id, user.value().access_token)};
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
  }
}

bool ShouldPutLogsInHomeDirectory() {
  // If this command line flag is specified, the logs should *not* be put in
  // the home directory.
  const bool redirect_logging =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kRedirectLibassistantLogging);
  return !redirect_logging;
}

ServiceControllerProxy::BootupConfigPtr CreateBootupConfig(
    const base::Optional<std::string>& s3_server_uri_override,
    const base::Optional<std::string>& device_id_override) {
  auto result = ServiceControllerProxy::BootupConfig::New();
  result->s3_server_uri_override = s3_server_uri_override;
  result->device_id_override = device_id_override;
  result->log_in_home_dir = ShouldPutLogsInHomeDirectory();
  return result;
}

}  // namespace

// Observer that will receive all speech recognition related events,
// and forwards them to all |AssistantInteractionSubscriber|.
class SpeechRecognitionObserverWrapper
    : public libassistant::mojom::SpeechRecognitionObserver {
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

  // libassistant::mojom::SpeechRecognitionObserver implementation:
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
void AssistantManagerServiceImpl::ResetIsFirstInitFlagForTesting() {
  is_first_init = true;
}

AssistantManagerServiceImpl::AssistantManagerServiceImpl(
    ServiceContext* context,
    std::unique_ptr<AssistantManagerServiceDelegate> delegate,
    std::unique_ptr<network::PendingSharedURLLoaderFactory>
        pending_url_loader_factory,
    base::Optional<std::string> s3_server_uri_override,
    base::Optional<std::string> device_id_override,
    std::unique_ptr<LibassistantServiceHost> libassistant_service_host)
    : assistant_settings_(
          std::make_unique<AssistantSettingsImpl>(context, this)),
      assistant_proxy_(std::make_unique<AssistantProxy>()),
      platform_delegate_(std::make_unique<PlatformDelegateImpl>()),
      context_(context),
      delegate_(std::move(delegate)),
      media_host_(std::make_unique<MediaHost>(AssistantClient::Get(),
                                              &interaction_subscribers_)),
      timer_host_(std::make_unique<TimerHost>(context)),
      audio_output_delegate_(std::make_unique<AudioOutputDelegateImpl>(
          &media_host_->media_session())),
      speech_recognition_observer_(
          std::make_unique<SpeechRecognitionObserverWrapper>(
              &interaction_subscribers_)),
      bootup_config_(
          CreateBootupConfig(s3_server_uri_override, device_id_override)),
      weak_factory_(this) {
  if (libassistant_service_host) {
    // During unittests a custom host is passed in, so we'll use that one.
    libassistant_service_host_ = std::move(libassistant_service_host);
  } else {
    // Use the default service host if none was provided.
    libassistant_service_host_ =
        std::make_unique<LibassistantServiceHostImpl>(delegate_.get());
  }

  assistant_proxy_->Initialize(libassistant_service_host_.get(),
                               std::move(pending_url_loader_factory));

  assistant_proxy_->service_controller().AddAndFireStateObserver(
      state_observer_receiver_.BindNewPipeAndPassRemote());
  assistant_proxy_->AddSpeechRecognitionObserver(
      speech_recognition_observer_->BindNewPipeAndPassRemote());

  audio_output_delegate_->Bind(assistant_proxy_->ExtractAudioOutputDelegate());
  platform_delegate_->Bind(assistant_proxy_->ExtractPlatformDelegate());
  audio_input_host_ = delegate_->CreateAudioInputHost(
      assistant_proxy_->ExtractAudioInputController());

  assistant_settings_->Initialize(
      assistant_proxy_->ExtractSpeakerIdEnrollmentController());

  media_host_->Initialize(&assistant_proxy_->media_controller(),
                          assistant_proxy_->ExtractMediaDelegate());

  settings_delegate_ =
      std::make_unique<AssistantDeviceSettingsDelegate>(context);
}

AssistantManagerServiceImpl::~AssistantManagerServiceImpl() {
  // Reset the observer for |CrosActionModule| before we flush the background
  // thread, where |CrosActionModule| gets created and exposed from. This can
  // prevent potential race condition between the main thread and background
  // thread when accessing/changing the shared object.
  scoped_action_observer_.Reset();

  // Destroy the Assistant Proxy first so the background thread is flushed
  // before any of the other objects are destroyed.
  assistant_proxy_ = nullptr;
}

void AssistantManagerServiceImpl::Start(const base::Optional<UserInfo>& user,
                                        bool enable_hotword) {
  DCHECK(!IsServiceStarted());
  DCHECK_EQ(GetState(), State::STOPPED);

  // Set the flag to avoid starting the service multiple times.
  SetStateAndInformObservers(State::STARTING);

  started_time_ = base::TimeTicks::Now();

  EnableHotword(enable_hotword);

  InitAssistant(user);
}

void AssistantManagerServiceImpl::Stop() {
  // We cannot cleanly stop the service if it is in the process of starting up.
  DCHECK_NE(GetState(), State::STARTING);

  SetStateAndInformObservers(State::STOPPED);

  media_host_->Stop();
  timer_host_->Stop();
  scoped_app_list_event_subscriber_.Reset();
  scoped_action_observer_.Reset();

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
    const base::Optional<UserInfo>& user) {
  if (!IsServiceStarted())
    return;

  VLOG(1) << "Set user information (Gaia ID and access token).";
  service_controller().SetAuthTokens(ToAuthTokensOrEmpty(user));
}

void AssistantManagerServiceImpl::EnableListening(bool enable) {
  if (!assistant_manager())
    return;
  // TODO(jeroendh): add EnableListening() to ServiceController.
  assistant_manager()->EnableListening(enable);
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

void AssistantManagerServiceImpl::AddCommunicationErrorObserver(
    CommunicationErrorObserver* observer) {
  error_observers_.AddObserver(observer);
}

void AssistantManagerServiceImpl::RemoveCommunicationErrorObserver(
    const CommunicationErrorObserver* observer) {
  error_observers_.RemoveObserver(observer);
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
  switch (action) {
    case MediaSessionAction::kPause:
      media_host_->PauseInternalMediaPlayer();
      break;
    case MediaSessionAction::kPlay:
      media_host_->ResumeInternalMediaPlayer();
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
  DCHECK(assistant_manager());
  DVLOG(1) << __func__;
  MaybeStopPreviousInteraction();

  audio_input_host_->SetMicState(true);
  assistant_manager()->StartAssistantInteraction();
}

void AssistantManagerServiceImpl::StopActiveInteraction(
    bool cancel_conversation) {
  DVLOG(1) << __func__;
  audio_input_host_->SetMicState(false);

  if (!assistant_manager_internal()) {
    VLOG(1) << "Stopping interaction without assistant manager.";
    return;
  }

  // We do not stop the interaction immediately, but instead we give
  // Libassistant a bit of time to stop on its own accord. This improves
  // stability as Libassistant might misbehave when it's forcefully stopped.
  auto stop_callback = [](base::WeakPtr<AssistantManagerServiceImpl> weak_this,
                          bool cancel_conversation) {
    if (!weak_this || !weak_this->assistant_manager_internal()) {
      return;
    }
    VLOG(1) << "Stopping interaction.";
    weak_this->assistant_manager_internal()->StopAssistantInteractionInternal(
        cancel_conversation);
  };

  stop_interaction_closure_ =
      std::make_unique<base::CancelableOnceClosure>(base::BindOnce(
          stop_callback, weak_factory_.GetWeakPtr(), cancel_conversation));

  main_task_runner()->PostDelayedTask(FROM_HERE,
                                      stop_interaction_closure_->callback(),
                                      stop_interactioin_delay_);
}

void AssistantManagerServiceImpl::StartEditReminderInteraction(
    const std::string& client_id) {
  conversation_controller_proxy().StartEditReminderInteraction(client_id);
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
  assistant_manager_internal()->SendScreenContextRequest(context_protos);
}

void AssistantManagerServiceImpl::StartTextInteraction(
    const std::string& query,
    AssistantQuerySource source,
    bool allow_tts) {
  DVLOG(1) << __func__;

  MaybeStopPreviousInteraction();

  conversation_controller_proxy().SendTextQuery(
      query, allow_tts,
      NewPendingInteraction(AssistantInteractionType::kText, source, query));
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
  conversation_controller_proxy().RetrieveNotification(notification,
                                                       action_index);
}

void AssistantManagerServiceImpl::DismissNotification(
    const AssistantNotification& notification) {
  conversation_controller_proxy().DismissNotification(notification);
}

void AssistantManagerServiceImpl::OnConversationTurnStartedInternal(
    const assistant_client::ConversationTurnMetadata& metadata) {
  ENSURE_MAIN_THREAD(
      &AssistantManagerServiceImpl::OnConversationTurnStartedInternal,
      metadata);

  stop_interaction_closure_.reset();

  audio_input_host_->OnConversationTurnStarted();

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

  stop_interaction_closure_.reset();

  // TODO(updowndota): Find a better way to handle the edge cases.
  if (resolution != Resolution::NORMAL_WITH_FOLLOW_ON &&
      resolution != Resolution::CANCELLED &&
      resolution != Resolution::BARGE_IN) {
    audio_input_host_->SetMicState(false);
  }

  audio_input_host_->OnConversationTurnFinished();

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
            if (weak_ptr && weak_ptr->action_module()) {
              weak_ptr->action_module()->OnScheduledWaitDone(
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

  assistant_manager_internal()->SendVoicelessInteraction(
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

  assistant_manager_internal()->SendVoicelessInteraction(
      interaction_proto, /*description=*/"verify_provider_response", options,
      [](auto) {});
}

void AssistantManagerServiceImpl::OnRespondingStarted(bool is_error_response) {
  ENSURE_MAIN_THREAD(&AssistantManagerServiceImpl::OnRespondingStarted,
                     is_error_response);

  for (auto& it : interaction_subscribers_)
    it.OnTtsStarted(is_error_response);
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

void AssistantManagerServiceImpl::OnStateChanged(
    libassistant::mojom::ServiceState new_state) {
  using libassistant::mojom::ServiceState;

  DVLOG(1) << "Libassistant service state changed to " << new_state;

  // We shall only move forward here if |state_| is not STOPPED, meaning
  // that we are still in the normal process of bringing up Libassistant
  // without barge in. In edge cases when assistant service could be shut
  // down right after being started, |state_| could be overridden to STOPPED
  // and thus we shall not proceed to avoid potential crashes caused by
  // accessing memory which has already been freed during Libassistant shut
  // down. This early return is also safe during normal shutdown process
  // since it will be no-op for ServiceState::kStopped case.
  if (GetState() == State::STOPPED)
    return;

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

void AssistantManagerServiceImpl::InitAssistant(
    const base::Optional<UserInfo>& user) {
  DCHECK(!IsServiceStarted());

  auto bootup_config = bootup_config_.Clone();
  bootup_config->locale = assistant_state()->locale().value();
  bootup_config->spoken_feedback_enabled = spoken_feedback_enabled_;
  bootup_config->hotword_enabled = assistant_state()->hotword_enabled().value();

  service_controller().Start(
      /*assistant_manager_delegate=*/this,
      /*conversation_state_listener=*/this, std::move(bootup_config),
      ToAuthTokensOrEmpty(user));
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

  if (base::FeatureList::IsEnabled(assistant::features::kAssistantAppSupport))
    scoped_app_list_event_subscriber_.Observe(device_actions());

  scoped_action_observer_.Observe(action_module());
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

void AssistantManagerServiceImpl::OnServiceRunning() {
  // It is possible the |assistant_manager()| was destructed before the
  // rescheduled main thread task got a chance to run. We check this and also
  // try to avoid double run by checking |GetState()|.
  if (!assistant_manager() || (GetState() == State::RUNNING))
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

  timer_host_->Start();

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
    service_controller().SetSpokenFeedbackEnabled(spoken_feedback_enabled_);
}

void AssistantManagerServiceImpl::OnDeviceAppsEnabled(bool enabled) {
  // The device apps state sync should only be sent after service is running.
  // Check state here to prevent timing issue when the service is restarting.
  if (GetState() != State::RUNNING)
    return;

  display_controller().SetDeviceAppsEnabled(enabled);
  action_module()->SetAppSupportEnabled(
      assistant::features::IsAppSupportEnabled() && enabled);
}

void AssistantManagerServiceImpl::AddTimeToTimer(const std::string& id,
                                                 base::TimeDelta duration) {
  timer_host_->AddTimeToTimer(id, duration);
}

void AssistantManagerServiceImpl::PauseTimer(const std::string& id) {
  timer_host_->PauseTimer(id);
}

void AssistantManagerServiceImpl::RemoveAlarmOrTimer(const std::string& id) {
  timer_host_->RemoveAlarmOrTimer(id);
}

void AssistantManagerServiceImpl::ResumeTimer(const std::string& id) {
  timer_host_->ResumeTimer(id);
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

  assistant_manager_internal()->SendVoicelessInteraction(
      interaction, description, voiceless_options, [](auto) {});
}

void AssistantManagerServiceImpl::MaybeStopPreviousInteraction() {
  if (!stop_interaction_closure_ || stop_interaction_closure_->IsCancelled()) {
    return;
  }

  stop_interaction_closure_->callback().Run();
}

std::string AssistantManagerServiceImpl::GetLastSearchSource() {
  return ConsumeLastTriggerSource();
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
  conversation_controller_proxy().SendAssistantFeedback(assistant_feedback);
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

AssistantProxy::DisplayController&
AssistantManagerServiceImpl::display_controller() {
  return assistant_proxy_->display_controller();
}

assistant_client::AssistantManager*
AssistantManagerServiceImpl::assistant_manager() {
  auto* api = LibassistantV1Api::Get();
  return api ? api->assistant_manager() : nullptr;
}

assistant_client::AssistantManagerInternal*
AssistantManagerServiceImpl::assistant_manager_internal() {
  auto* api = LibassistantV1Api::Get();
  return api ? api->assistant_manager_internal() : nullptr;
}

assistant::action::CrosActionModule*
AssistantManagerServiceImpl::action_module() {
  auto* api = LibassistantV1Api::Get();
  return api ? api->action_module() : nullptr;
}

void AssistantManagerServiceImpl::SetMicState(bool mic_open) {
  DCHECK(audio_input_host_);
  audio_input_host_->SetMicState(mic_open);
}

ConversationControllerProxy&
AssistantManagerServiceImpl::conversation_controller_proxy() {
  return assistant_proxy_->conversation_controller_proxy();
}

ServiceControllerProxy& AssistantManagerServiceImpl::service_controller() {
  return assistant_proxy_->service_controller();
}

const ServiceControllerProxy& AssistantManagerServiceImpl::service_controller()
    const {
  return assistant_proxy_->service_controller();
}

base::Thread& AssistantManagerServiceImpl::background_thread() {
  return assistant_proxy_->background_thread();
}

void AssistantManagerServiceImpl::SetStateAndInformObservers(State new_state) {
  state_ = new_state;

  for (auto& observer : state_observers_)
    observer.OnStateChanged(state_);
}

}  // namespace assistant
}  // namespace chromeos
