// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_ASSISTANT_ASSISTANT_MANAGER_SERVICE_IMPL_H_
#define CHROMEOS_ASH_SERVICES_ASSISTANT_ASSISTANT_MANAGER_SERVICE_IMPL_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/cancelable_callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "chromeos/ash/services/assistant/assistant_host.h"
#include "chromeos/ash/services/assistant/assistant_manager_service.h"
#include "chromeos/ash/services/assistant/assistant_settings_impl.h"
#include "chromeos/ash/services/assistant/libassistant_service_host.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_service.h"
#include "chromeos/ash/services/assistant/public/cpp/conversation_observer.h"
#include "chromeos/ash/services/assistant/public/cpp/device_actions.h"
#include "chromeos/ash/services/libassistant/public/cpp/assistant_notification.h"
#include "chromeos/ash/services/libassistant/public/mojom/notification_delegate.mojom.h"
#include "chromeos/ash/services/libassistant/public/mojom/service_controller.mojom-forward.h"
#include "chromeos/services/assistant/public/shared/utils.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/battery_monitor.mojom.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "ui/accessibility/ax_assistant_structure.h"
#include "ui/accessibility/mojom/ax_assistant_structure.mojom.h"

namespace ash {

class AssistantNotificationController;
class AssistantStateBase;

namespace assistant {

class AssistantHost;
class AssistantMediaSession;
class AudioInputHost;
class AudioOutputDelegateImpl;
class DeviceSettingsHost;
class MediaHost;
class PlatformDelegateImpl;
class ServiceContext;
class SpeechRecognitionObserverWrapper;
class TimerHost;

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
  // or Facebook, some deeplink actions such as opening chrome settings page),
  // indicating that Assistant knows what to do.
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
      public AppListEventSubscriber,
      private libassistant::mojom::StateObserver,
      public ConversationObserver {
 public:
  // |callback| is called when AssistantManagerServiceImpl got initialized
  // internally. This waits DeviceApps config value sync.
  static void SetInitializedInternalCallbackForTesting(
      base::OnceCallback<void()> callback);
  static void ResetIsFirstInitFlagForTesting();

  // |service| owns this class and must outlive this class.
  AssistantManagerServiceImpl(
      ServiceContext* context,
      std::unique_ptr<network::PendingSharedURLLoaderFactory>
          pending_url_loader_factory,
      std::optional<std::string> s3_server_uri_override,
      std::optional<std::string> device_id_override,
      // Allows to inect a custom |LibassistantServiceHost| during unittests.
      std::unique_ptr<LibassistantServiceHost> libassistant_service_host =
          nullptr);

  AssistantManagerServiceImpl(const AssistantManagerServiceImpl&) = delete;
  AssistantManagerServiceImpl& operator=(const AssistantManagerServiceImpl&) =
      delete;

  ~AssistantManagerServiceImpl() override;

  // assistant::AssistantManagerService overrides:
  void Start(const std::optional<UserInfo>& user, bool enable_hotword) override;
  void Stop() override;
  State GetState() const override;
  void SetUser(const std::optional<UserInfo>& user) override;
  void EnableListening(bool enable) override;
  void EnableHotword(bool enable) override;
  void SetArcPlayStoreEnabled(bool enable) override;
  void SetAssistantContextEnabled(bool enable) override;
  AssistantSettings* GetAssistantSettings() override;
  void AddAuthenticationStateObserver(
      AuthenticationStateObserver* observer) override;
  void AddAndFireStateObserver(
      AssistantManagerService::StateObserver* observer) override;
  void RemoveStateObserver(
      const AssistantManagerService::StateObserver* observer) override;
  void SyncDeviceAppsStatus() override;
  void UpdateInternalMediaPlayerStatus(
      media_session::mojom::MediaSessionAction action) override;

  // Assistant overrides:
  void StartEditReminderInteraction(const std::string& client_id) override;
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
  void OnColorModeChanged(bool dark_mode_enabled) override;
  void SendAssistantFeedback(const AssistantFeedback& feedback) override;
  void AddTimeToTimer(const std::string& id, base::TimeDelta duration) override;
  void PauseTimer(const std::string& id) override;
  void RemoveAlarmOrTimer(const std::string& id) override;
  void ResumeTimer(const std::string& id) override;
  void AddRemoteConversationObserver(ConversationObserver* observer) override;
  mojo::PendingReceiver<libassistant::mojom::NotificationDelegate>
  GetPendingNotificationDelegate() override;

  // ConversationObserver overrides:
  void OnInteractionStarted(
      const AssistantInteractionMetadata& metadata) override;
  void OnInteractionFinished(
      AssistantInteractionResolution resolution) override;
  void OnHtmlResponse(const std::string& response,
                      const std::string& fallback) override;
  void OnTextResponse(const std::string& reponse) override;
  void OnOpenUrlResponse(const GURL& url, bool in_background) override;

  // AppListEventSubscriber overrides:
  void OnAndroidAppListRefreshed(
      const std::vector<AndroidAppInfo>& apps_info) override;

  // libassistant::mojom::StateObserver implementation:
  void OnStateChanged(libassistant::mojom::ServiceState new_state) override;

  void SetMicState(bool mic_open);

  base::Thread& GetBackgroundThreadForTesting();

 private:
  void Initialize();
  void InitAssistant(const std::optional<UserInfo>& user);
  void OnServiceStarted();
  void OnServiceRunning();
  void OnServiceStopped();
  void OnServiceDisconnected();
  bool IsServiceStarted() const;

  mojo::PendingRemote<network::mojom::URLLoaderFactory> BindURLLoaderFactory();

  void OnModifySettingsAction(const std::string& modify_setting_args_proto);

  void OnDeviceAppsEnabled(bool enabled);

  void FillServerExperimentIds(std::vector<std::string>* server_experiment_ids);

  // Record the response type for each query. Note that query on device
  // actions (e.g. turn on Bluetooth, turn on WiFi) will cause duplicate
  // record because it interacts with server twice on on the same query.
  // The first round interaction checks if a setting is supported with no
  // responses sent back and ends normally (will be recorded as kUnspecified),
  // and settings modification proto along with any text/voice responses would
  // be sent back in the second round (recorded as kDeviceAction).
  void RecordQueryResponseTypeUMA();
  bool HasReceivedQueryResponse() const;
  AssistantQueryResponseType GetQueryResponseType() const;

  std::string NewPendingInteraction(AssistantInteractionType interaction_type,
                                    AssistantQuerySource source,
                                    const std::string& query);

  void SendVoicelessInteraction(const std::string& interaction,
                                const std::string& description,
                                bool is_user_initiated);

  AssistantNotificationController* assistant_notification_controller();
  AssistantStateBase* assistant_state();
  DeviceActions* device_actions();
  scoped_refptr<base::SequencedTaskRunner> main_task_runner();

  libassistant::mojom::ConversationController& conversation_controller();
  libassistant::mojom::DisplayController& display_controller();
  libassistant::mojom::ServiceController& service_controller();
  libassistant::mojom::SettingsController& settings_controller();
  base::Thread& background_thread();

  void SetStateAndInformObservers(State new_state);

  void ClearAfterStop();

  State state_ = State::STOPPED;

  std::unique_ptr<AssistantSettingsImpl> assistant_settings_;

  std::unique_ptr<AssistantHost> assistant_host_;
  std::unique_ptr<PlatformDelegateImpl> platform_delegate_;
  std::unique_ptr<AudioInputHost> audio_input_host_;

  base::ObserverList<AssistantInteractionSubscriber> interaction_subscribers_;

  // Owned by the parent |Service| which will destroy |this| before |context_|.
  const raw_ptr<ServiceContext> context_;

  std::unique_ptr<LibassistantServiceHost> libassistant_service_host_;
  std::unique_ptr<DeviceSettingsHost> device_settings_host_;
  std::unique_ptr<MediaHost> media_host_;
  std::unique_ptr<TimerHost> timer_host_;
  std::unique_ptr<AudioOutputDelegateImpl> audio_output_delegate_;
  std::unique_ptr<SpeechRecognitionObserverWrapper>
      speech_recognition_observer_;
  mojo::Receiver<libassistant::mojom::StateObserver> state_observer_receiver_{
      this};

  bool spoken_feedback_enabled_ = false;
  bool dark_mode_enabled_ = false;

  base::TimeTicks started_time_;

  bool receive_inline_response_ = false;
  std::string receive_url_response_;

  // Configuration passed to libassistant.
  libassistant::mojom::BootupConfigPtr bootup_config_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  base::ScopedObservation<DeviceActions, AppListEventSubscriber>
      scoped_app_list_event_subscriber_{this};
  base::ObserverList<AssistantManagerService::StateObserver> state_observers_;

  base::WeakPtrFactory<AssistantManagerServiceImpl> weak_factory_;
};

}  // namespace assistant
}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_ASSISTANT_ASSISTANT_MANAGER_SERVICE_IMPL_H_
