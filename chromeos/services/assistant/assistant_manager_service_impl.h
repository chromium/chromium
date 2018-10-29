// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_ASSISTANT_MANAGER_SERVICE_IMPL_H_
#define CHROMEOS_SERVICES_ASSISTANT_ASSISTANT_MANAGER_SERVICE_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/public/cpp/assistant/default_voice_interaction_observer.h"
#include "ash/public/interfaces/ash_message_center_controller.mojom.h"
#include "ash/public/interfaces/assistant_controller.mojom.h"
#include "ash/public/interfaces/voice_interaction_controller.mojom.h"
#include "base/threading/thread.h"
#include "chromeos/assistant/internal/action/cros_action_module.h"
#include "chromeos/assistant/internal/cros_display_connection.h"
#include "chromeos/assistant/internal/internal_util.h"
#include "chromeos/services/assistant/assistant_manager_service.h"
#include "chromeos/services/assistant/assistant_settings_manager_impl.h"
#include "chromeos/services/assistant/platform_api_impl.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"
#include "libassistant/shared/internal_api/assistant_manager_delegate.h"
#include "libassistant/shared/public/conversation_state_listener.h"
#include "libassistant/shared/public/device_state_listener.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"
#include "services/device/public/mojom/battery_monitor.mojom.h"
#include "ui/accessibility/ax_assistant_structure.h"
#include "ui/accessibility/mojom/ax_assistant_structure.mojom.h"

namespace assistant_client {
class AssistantManager;
class AssistantManagerInternal;
struct SpeakerIdEnrollmentUpdate;
}  // namespace assistant_client

namespace service_manager {
class Connector;
}  // namespace service_manager

namespace chromeos {
namespace assistant {

class Service;

// Implementation of AssistantManagerService based on LibAssistant.
// This is the main class that ineracts with LibAssistant.
// Since LibAssistant is a standalone library, all callbacks come from it
// running on threads not owned by Chrome. Thus we need to post the callbacks
// onto the main thread.
class AssistantManagerServiceImpl
    : public AssistantManagerService,
      public ::chromeos::assistant::action::AssistantActionObserver,
      public AssistantEventObserver,
      public assistant_client::ConversationStateListener,
      public assistant_client::AssistantManagerDelegate,
      public ash::DefaultVoiceInteractionObserver,
      public assistant_client::DeviceStateListener {
 public:
  // |service| owns this class and must outlive this class.
  AssistantManagerServiceImpl(
      service_manager::Connector* connector,
      device::mojom::BatteryMonitorPtr battery_monitor,
      Service* service,
      bool enable_hotword,
      network::NetworkConnectionTracker* network_connection_tracker);

  ~AssistantManagerServiceImpl() override;

  // assistant::AssistantManagerService overrides
  void Start(const std::string& access_token,
             base::OnceClosure callback) override;
  void Stop() override;
  State GetState() const override;
  void SetAccessToken(const std::string& access_token) override;
  void EnableListening(bool enable) override;
  AssistantSettingsManager* GetAssistantSettingsManager() override;
  void SendGetSettingsUiRequest(
      const std::string& selector,
      GetSettingsUiResponseCallback callback) override;
  void SendUpdateSettingsUiRequest(
      const std::string& update,
      UpdateSettingsUiResponseCallback callback) override;
  void StartSpeakerIdEnrollment(
      bool skip_cloud_enrollment,
      mojom::SpeakerIdEnrollmentClientPtr client) override;
  void StopSpeakerIdEnrollment(
      AssistantSettingsManager::StopSpeakerIdEnrollmentCallback callback)
      override;

  // mojom::Assistant overrides:
  void StartCachedScreenContextInteraction() override;
  void StartMetalayerInteraction(const gfx::Rect& region) override;
  void StartVoiceInteraction() override;
  void StopActiveInteraction(bool cancel_conversation) override;
  void SendTextQuery(const std::string& query) override;
  void AddAssistantInteractionSubscriber(
      mojom::AssistantInteractionSubscriberPtr subscriber) override;
  void AddAssistantNotificationSubscriber(
      mojom::AssistantNotificationSubscriberPtr subscriber) override;
  void RetrieveNotification(mojom::AssistantNotificationPtr notification,
                            int action_index) override;
  void DismissNotification(
      mojom::AssistantNotificationPtr notification) override;
  void CacheScreenContext(CacheScreenContextCallback callback) override;
  void OnAccessibilityStatusChanged(bool spoken_feedback_enabled) override;

  // AssistantActionObserver overrides:
  void OnShowContextualQueryFallback() override;
  void OnShowHtml(const std::string& html,
                  const std::string& fallback) override;
  void OnShowSuggestions(
      const std::vector<action::Suggestion>& suggestions) override;
  void OnShowText(const std::string& text) override;
  void OnOpenUrl(const std::string& url) override;
  void OnShowNotification(const action::Notification& notification) override;

  // AssistantEventObserver overrides:
  void OnSpeechLevelUpdated(float speech_level) override;

  // assistant_client::ConversationStateListener overrides:
  void OnConversationTurnStarted(bool is_mic_open) override;
  void OnConversationTurnFinished(
      assistant_client::ConversationStateListener::Resolution resolution)
      override;
  void OnRecognitionStateChanged(
      assistant_client::ConversationStateListener::RecognitionState state,
      const assistant_client::ConversationStateListener::RecognitionResult&
          recognition_result) override;
  void OnRespondingStarted(bool is_error_response) override;

  // AssistantManagerDelegate overrides
  assistant_client::ActionModule::Result HandleModifySettingClientOp(
      const std::string& modify_setting_args_proto) override;
  bool IsSettingSupported(const std::string& setting_id) override;
  bool SupportsModifySettings() override;
  void OnNotificationRemoved(const std::string& grouping_key) override;
  void OnCommunicationError(int error_code) override;
  // Last search source will be cleared after it is retrieved.
  std::string GetLastSearchSource() override;

  // ash::mojom::VoiceInteractionObserver:
  void OnVoiceInteractionSettingsEnabled(bool enabled) override;
  void OnVoiceInteractionContextEnabled(bool enabled) override;
  void OnVoiceInteractionHotwordEnabled(bool enabled) override;
  void OnLocaleChanged(const std::string& locale) override;

  // assistant_client::DeviceStateListener overrides:
  void OnStartFinished() override;
  void OnTimerSoundingStarted() override;
  void OnTimerSoundingFinished() override;

 private:
  void StartAssistantInternal(const std::string& access_token,
                              const std::string& arc_version);
  void PostInitAssistant(base::OnceClosure post_init_callback);

  std::string BuildUserAgent(const std::string& arc_version) const;

  // Update device id, type and locale
  void UpdateDeviceSettings();
  void UpdateInternalOptions();

  void HandleGetSettingsResponse(
      base::RepeatingCallback<void(const std::string&)> callback,
      const std::string& settings);
  void HandleUpdateSettingsResponse(
      base::RepeatingCallback<void(const std::string&)> callback,
      const std::string& result);
  void HandleSpeakerIdEnrollmentUpdate(
      const assistant_client::SpeakerIdEnrollmentUpdate& update);
  void HandleStopSpeakerIdEnrollment(base::RepeatingCallback<void()> callback);

  void OnConversationTurnStartedOnMainThread(bool is_mic_open);
  void OnConversationTurnFinishedOnMainThread(
      assistant_client::ConversationStateListener::Resolution resolution);
  void OnShowHtmlOnMainThread(const std::string& html,
                              const std::string& fallback);
  void OnShowSuggestionsOnMainThread(
      const std::vector<mojom::AssistantSuggestionPtr>& suggestions);
  void OnShowTextOnMainThread(const std::string& text);
  void OnOpenUrlOnMainThread(const std::string& url);
  void OnShowNotificationOnMainThread(
      const mojom::AssistantNotificationPtr& notification);
  void OnNotificationRemovedOnMainThread(const std::string& grouping_id);
  void OnCommunicationErrorOnMainThread(int error_code);
  void OnRecognitionStateChangedOnMainThread(
      assistant_client::ConversationStateListener::RecognitionState state,
      const assistant_client::ConversationStateListener::RecognitionResult&
          recognition_result);
  void OnRespondingStartedOnMainThread(bool is_error_response);
  void OnSpeechLevelUpdatedOnMainThread(const float speech_level);
  void OnModifySettingsAction(const std::string& modify_setting_args_proto);

  void RegisterFallbackMediaHandler();

  void CacheAssistantStructure(
      base::OnceClosure on_done,
      ax::mojom::AssistantExtraPtr assistant_extra,
      std::unique_ptr<ui::AssistantTree> assistant_tree);

  void CacheAssistantScreenshot(
      base::OnceClosure on_done,
      const std::vector<uint8_t>& assistant_screenshot);

  void SendScreenContextRequest(
      ax::mojom::AssistantExtraPtr assistant_extra,
      std::unique_ptr<ui::AssistantTree> assistant_tree,
      const std::vector<uint8_t>& assistant_screenshot);

  void FillServerExperimentIds(std::vector<std::string>& server_experiment_ids);

  State state_ = State::STOPPED;
  std::unique_ptr<PlatformApiImpl> platform_api_;
  bool enable_hotword_;
  std::unique_ptr<action::CrosActionModule> action_module_;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;
  std::unique_ptr<assistant_client::AssistantManager> assistant_manager_;
  std::unique_ptr<AssistantSettingsManagerImpl> assistant_settings_manager_;
  // same ownership as assistant_manager_.
  assistant_client::AssistantManagerInternal* assistant_manager_internal_ =
      nullptr;
  std::unique_ptr<CrosDisplayConnection> display_connection_;
  mojo::InterfacePtrSet<mojom::AssistantInteractionSubscriber>
      interaction_subscribers_;
  mojo::InterfacePtrSet<mojom::AssistantNotificationSubscriber>
      notification_subscribers_;
  ash::mojom::VoiceInteractionControllerPtr voice_interaction_controller_;
  mojo::Binding<ash::mojom::VoiceInteractionObserver>
      voice_interaction_observer_binding_;
  ash::mojom::AshMessageCenterControllerPtr ash_message_center_controller_;
  mojom::SpeakerIdEnrollmentClientPtr speaker_id_enrollment_client_;

  Service* service_;  // unowned.

  base::Optional<std::string> arc_version_;

  bool assistant_enabled_ = false;
  bool context_enabled_ = false;
  bool spoken_feedback_enabled_ = false;
  std::string locale_;

  ax::mojom::AssistantExtraPtr assistant_extra_;
  std::unique_ptr<ui::AssistantTree> assistant_tree_;
  std::vector<uint8_t> assistant_screenshot_;
  std::string last_search_source_;
  base::Lock last_search_source_lock_;
  base::TimeTicks started_time_;

  base::Thread background_thread_;

  base::WeakPtrFactory<AssistantManagerServiceImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AssistantManagerServiceImpl);
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_ASSISTANT_MANAGER_SERVICE_IMPL_H_
