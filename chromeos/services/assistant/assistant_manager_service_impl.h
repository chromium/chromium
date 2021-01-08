// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_ASSISTANT_MANAGER_SERVICE_IMPL_H_
#define CHROMEOS_SERVICES_ASSISTANT_ASSISTANT_MANAGER_SERVICE_IMPL_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "ash/public/cpp/assistant/controller/assistant_screen_context_controller.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "base/scoped_observation.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
#include "chromeos/assistant/internal/action/cros_action_module.h"
#include "chromeos/assistant/internal/cros_display_connection.h"
#include "chromeos/assistant/internal/internal_util.h"
#include "chromeos/services/assistant/assistant_manager_service.h"
#include "chromeos/services/assistant/assistant_settings_impl.h"
#include "chromeos/services/assistant/chromium_api_delegate.h"
#include "chromeos/services/assistant/proxy/assistant_proxy.h"
#include "chromeos/services/assistant/proxy/libassistant_service_host.h"
#include "chromeos/services/assistant/public/cpp/assistant_notification.h"
#include "chromeos/services/assistant/public/cpp/assistant_service.h"
#include "chromeos/services/assistant/public/cpp/device_actions.h"
#include "chromeos/services/assistant/public/shared/utils.h"
#include "libassistant/shared/internal_api/assistant_manager_delegate.h"
#include "libassistant/shared/public/conversation_state_listener.h"
#include "libassistant/shared/public/device_state_listener.h"
#include "libassistant/shared/public/media_manager.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/battery_monitor.mojom.h"
#include "services/media_session/public/mojom/media_controller.mojom.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "ui/accessibility/ax_assistant_structure.h"
#include "ui/accessibility/mojom/ax_assistant_structure.mojom.h"

namespace ash {
class AssistantAlarmTimerController;
class AssistantNotificationController;
class AssistantStateBase;
}  // namespace ash

namespace assistant_client {
class AssistantManager;
class AssistantManagerInternal;
}  // namespace assistant_client

namespace network {
class PendingSharedURLLoaderFactory;
}  // namespace network

namespace chromeos {
namespace assistant {

class AssistantMediaSession;
class AssistantDeviceSettingsDelegate;
class AssistantManagerServiceDelegate;
class AssistantProxy;
class CrosPlatformApi;
class ServiceContext;
class ServiceControllerProxy;

// Enumeration of Assistant query response type, also recorded in histograms.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Only append to this enum is allowed
// if the possible type grows.
enum class AssistantQueryResponseType {
  // Query without response.
  kUnspecified = 0,
  // Query results in device actions (e.g. turn on bluetooth/WiFi).
  kDeviceAction = 1,
  // Query results in answer cards with contents rendered inside the
  // Assistant UI.
  kInlineElement = 2,
  // Query results in searching on Google, indicating that Assistant
  // doesn't know what to do.
  kSearchFallback = 3,
  // Query results in specific actions (e.g. opening a web app such as YouTube
  // or Facebook, some deeplink actions such as taking a screenshot or opening
  // chrome settings page), indicating that Assistant knows what to do.
  kTargetedAction = 4,
  // Special enumerator value used by histogram macros.
  kMaxValue = kTargetedAction
};

// Implementation of AssistantManagerService based on LibAssistant.
// This is the main class that interacts with LibAssistant.
// Since LibAssistant is a standalone library, all callbacks come from it
// running on threads not owned by Chrome. Thus we need to post the callbacks
// onto the main thread.
// NOTE: this class may start/stop LibAssistant multiple times throughout its
// lifetime. This may occur, for example, if the user manually toggles Assistant
// enabled/disabled in settings or switches to a non-primary profile.
class COMPONENT_EXPORT(ASSISTANT_SERVICE) AssistantManagerServiceImpl
    : public AssistantManagerService,
      public ::chromeos::assistant::action::AssistantActionObserver,
      public AssistantEventObserver,
      public assistant_client::ConversationStateListener,
      public assistant_client::AssistantManagerDelegate,
      public assistant_client::DeviceStateListener,
      public assistant_client::MediaManager::Listener,
      public media_session::mojom::MediaControllerObserver,
      public AppListEventSubscriber {
 public:
  // |service| owns this class and must outlive this class.
  AssistantManagerServiceImpl(
      ServiceContext* context,
      std::unique_ptr<AssistantManagerServiceDelegate> delegate,
      std::unique_ptr<network::PendingSharedURLLoaderFactory>
          pending_url_loader_factory,
      base::Optional<std::string> s3_server_uri_override,
      base::Optional<std::string> device_id_override,
      // Allows to inect a custom |LibassistantServiceHost| during unittests.
      std::unique_ptr<LibassistantServiceHost> libassistant_service_host =
          nullptr);

  ~AssistantManagerServiceImpl() override;

  // assistant::AssistantManagerService overrides:
  void Start(const base::Optional<UserInfo>& user,
             bool enable_hotword) override;
  void Stop() override;
  State GetState() const override;
  void SetUser(const base::Optional<UserInfo>& user) override;
  void EnableAmbientMode(bool enabled) override;
  void EnableListening(bool enable) override;
  void EnableHotword(bool enable) override;
  void SetArcPlayStoreEnabled(bool enable) override;
  void SetAssistantContextEnabled(bool enable) override;
  AssistantSettings* GetAssistantSettings() override;
  void AddCommunicationErrorObserver(
      CommunicationErrorObserver* observer) override;
  void RemoveCommunicationErrorObserver(
      const CommunicationErrorObserver* observer) override;
  void AddAndFireStateObserver(StateObserver* observer) override;
  void RemoveStateObserver(const StateObserver* observer) override;
  void SyncDeviceAppsStatus() override;
  void UpdateInternalMediaPlayerStatus(
      media_session::mojom::MediaSessionAction action) override;

  // Assistant overrides:
  void StartEditReminderInteraction(const std::string& client_id) override;
  void StartScreenContextInteraction(
      ax::mojom::AssistantStructurePtr assistant_structure,
      const std::vector<uint8_t>& assistant_screenshot) override;
  void StartTextInteraction(const std::string& query,
                            AssistantQuerySource source,
                            bool allow_tts) override;
  void StartVoiceInteraction() override;
  void StopActiveInteraction(bool cancel_conversation) override;
  void AddAssistantInteractionSubscriber(
      AssistantInteractionSubscriber* subscriber) override;
  void RemoveAssistantInteractionSubscriber(
      AssistantInteractionSubscriber* subscriber) override;
  void RetrieveNotification(const AssistantNotification& notification,
                            int action_index) override;
  void DismissNotification(const AssistantNotification& notification) override;
  void OnAccessibilityStatusChanged(bool spoken_feedback_enabled) override;
  void SendAssistantFeedback(const AssistantFeedback& feedback) override;
  void NotifyEntryIntoAssistantUi(AssistantEntryPoint entry_point) override;
  void AddTimeToTimer(const std::string& id, base::TimeDelta duration) override;
  void PauseTimer(const std::string& id) override;
  void RemoveAlarmOrTimer(const std::string& id) override;
  void ResumeTimer(const std::string& id) override;

  // AssistantActionObserver overrides:
  void OnScheduleWait(int id, int time_ms) override;
  void OnShowContextualQueryFallback() override;
  void OnShowHtml(const std::string& html,
                  const std::string& fallback) override;
  void OnShowSuggestions(
      const std::vector<action::Suggestion>& suggestions) override;
  void OnShowText(const std::string& text) override;
  void OnOpenUrl(const std::string& url, bool in_background) override;
  void OnShowNotification(const action::Notification& notification) override;
  void OnOpenAndroidApp(const AndroidAppInfo& app_info,
                        const InteractionInfo& interaction) override;
  void OnVerifyAndroidApp(const std::vector<AndroidAppInfo>& apps_info,
                          const InteractionInfo& interaction) override;
  void OnModifyDeviceSetting(
      const ::assistant::api::client_op::ModifySettingArgs& args) override;
  void OnGetDeviceSettings(
      int interaction_id,
      const ::assistant::api::client_op::GetDeviceSettingsArgs& args) override;

  // AssistantEventObserver overrides:
  void OnSpeechLevelUpdated(float speech_level) override;

  // assistant_client::ConversationStateListener overrides:
  void OnConversationTurnFinished(
      assistant_client::ConversationStateListener::Resolution resolution)
      override;
  void OnRecognitionStateChanged(
      assistant_client::ConversationStateListener::RecognitionState state,
      const assistant_client::ConversationStateListener::RecognitionResult&
          recognition_result) override;
  void OnRespondingStarted(bool is_error_response) override;

  // AssistantManagerDelegate overrides:
  void OnConversationTurnStartedInternal(
      const assistant_client::ConversationTurnMetadata& metadata) override;
  void OnNotificationRemoved(const std::string& grouping_key) override;
  void OnCommunicationError(int error_code) override;
  // Last search source will be cleared after it is retrieved.
  std::string GetLastSearchSource() override;

  // assistant_client::DeviceStateListener overrides:
  void OnStartFinished() override;

  // AppListEventSubscriber overrides:
  void OnAndroidAppListRefreshed(
      const std::vector<AndroidAppInfo>& apps_info) override;

  assistant_client::AssistantManager* assistant_manager();
  assistant_client::AssistantManagerInternal* assistant_manager_internal();
  CrosPlatformApi* platform_api() { return platform_api_.get(); }

  // assistant_client::MediaManager::Listener overrides:
  void OnPlaybackStateChange(
      const assistant_client::MediaStatus& status) override;

  // media_session::mojom::MediaControllerObserver overrides:
  void MediaSessionInfoChanged(
      media_session::mojom::MediaSessionInfoPtr info) override;
  void MediaSessionMetadataChanged(
      const base::Optional<media_session::MediaMetadata>& metadata) override;
  void MediaSessionActionsChanged(
      const std::vector<media_session::mojom::MediaSessionAction>& action)
      override {}
  void MediaSessionChanged(
      const base::Optional<base::UnguessableToken>& request_id) override;
  void MediaSessionPositionChanged(
      const base::Optional<media_session::MediaPosition>& position) override {}

  // Get the action module for testing.
  action::CrosActionModule* action_module_for_testing() {
    return action_module_.get();
  }

 private:
  void InitAssistant(const base::Optional<UserInfo>& user,
                     const std::string& locale);
  void PostInitAssistant();
  bool IsServiceStarted() const;

  void HandleLaunchMediaIntentResponse(bool app_opened);

  void OnAlarmTimerStateChanged();
  void OnModifySettingsAction(const std::string& modify_setting_args_proto);
  void OnOpenMediaAndroidIntent(const std::string& play_media_args_proto,
                                AndroidAppInfo* app_info);
  void OnPlayMedia(const std::string& play_media_args_proto);
  void OnMediaControlAction(const std::string& action_name,
                            const std::string& media_action_args_proto);

  void OnDeviceAppsEnabled(bool enabled);

  void RegisterFallbackMediaHandler();
  void AddMediaControllerObserver();
  void RemoveMediaControllerObserver();
  void RegisterAlarmsTimersListener();

  void FillServerExperimentIds(std::vector<std::string>* server_experiment_ids);

  // Record the response type for each query. Note that query on device
  // actions (e.g. turn on Bluetooth, turn on WiFi) will cause duplicate
  // record because it interacts with server twice on on the same query.
  // The first round interaction checks if a setting is supported with no
  // responses sent back and ends normally (will be recorded as kUnspecified),
  // and settings modification proto along with any text/voice responses would
  // be sent back in the second round (recorded as kDeviceAction).
  void RecordQueryResponseTypeUMA();

  void UpdateMediaState();
  void ResetMediaState();

  std::string NewPendingInteraction(AssistantInteractionType interaction_type,
                                    AssistantQuerySource source,
                                    const std::string& query);

  std::string ConsumeLastTriggerSource();

  void SendVoicelessInteraction(const std::string& interaction,
                                const std::string& description,
                                bool is_user_initiated);

  ash::AssistantAlarmTimerController* assistant_alarm_timer_controller();
  ash::AssistantNotificationController* assistant_notification_controller();
  ash::AssistantScreenContextController* assistant_screen_context_controller();
  ash::AssistantStateBase* assistant_state();
  DeviceActions* device_actions();
  scoped_refptr<base::SequencedTaskRunner> main_task_runner();

  CrosDisplayConnection* display_connection();
  ServiceControllerProxy& service_controller();
  const ServiceControllerProxy& service_controller() const;
  base::Thread& background_thread();

  void SetStateAndInformObservers(State new_state);

  State state_ = State::STOPPED;
  std::unique_ptr<AssistantMediaSession> media_session_;
  std::unique_ptr<CrosPlatformApi> platform_api_;
  std::unique_ptr<action::CrosActionModule> action_module_;
  ChromiumApiDelegate chromium_api_delegate_;
  std::unique_ptr<AssistantSettingsImpl> assistant_settings_;

  std::unique_ptr<AssistantProxy> assistant_proxy_;

  base::ObserverList<AssistantInteractionSubscriber> interaction_subscribers_;
  mojo::Remote<media_session::mojom::MediaController> media_controller_;

  // Owned by the parent |Service| which will destroy |this| before |context_|.
  ServiceContext* const context_;

  std::unique_ptr<AssistantManagerServiceDelegate> delegate_;
  std::unique_ptr<LibassistantServiceHost> libassistant_service_host_;
  std::unique_ptr<AssistantDeviceSettingsDelegate> settings_delegate_;

  bool spoken_feedback_enabled_ = false;

  std::string last_trigger_source_;
  base::Lock last_trigger_source_lock_;
  base::TimeTicks started_time_;

  int next_interaction_id_ = 1;
  std::map<std::string, std::unique_ptr<AssistantInteractionMetadata>>
      pending_interactions_;

  bool receive_modify_settings_proto_response_ = false;
  bool receive_inline_response_ = false;
  std::string receive_url_response_;

  mojo::Receiver<media_session::mojom::MediaControllerObserver>
      media_controller_observer_receiver_{this};

  // Info associated to the active media session.
  media_session::mojom::MediaSessionInfoPtr media_session_info_ptr_;
  // The metadata for the active media session. It can be null to be reset, e.g.
  // the media that was being played has been stopped.
  base::Optional<media_session::MediaMetadata> media_metadata_ = base::nullopt;

  base::UnguessableToken media_session_audio_focus_id_ =
      base::UnguessableToken::Null();

  // Configuration passed to libassistant.
  std::string libassistant_config_;

  base::ScopedObservation<DeviceActions,
                          AppListEventSubscriber,
                          &DeviceActions::AddAndFireAppListEventSubscriber,
                          &DeviceActions::RemoveAppListEventSubscriber>
      scoped_app_list_event_subscriber_{this};
  base::ObserverList<CommunicationErrorObserver> error_observers_;
  base::ObserverList<StateObserver> state_observers_;

  base::WeakPtrFactory<AssistantManagerServiceImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AssistantManagerServiceImpl);
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_ASSISTANT_MANAGER_SERVICE_IMPL_H_
