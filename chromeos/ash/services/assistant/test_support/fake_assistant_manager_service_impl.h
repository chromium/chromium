// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_ASSISTANT_TEST_SUPPORT_FAKE_ASSISTANT_MANAGER_SERVICE_IMPL_H_
#define CHROMEOS_ASH_SERVICES_ASSISTANT_TEST_SUPPORT_FAKE_ASSISTANT_MANAGER_SERVICE_IMPL_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/observer_list.h"
#include "chromeos/ash/services/assistant/assistant_manager_service.h"
#include "chromeos/ash/services/assistant/test_support/fake_assistant_settings_impl.h"
#include "chromeos/ash/services/libassistant/public/mojom/notification_delegate.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace ash::assistant {

using media_session::mojom::MediaSessionAction;

// Stub implementation of AssistantManagerService.
// Will return deterministic result for testing.
class COMPONENT_EXPORT(ASSISTANT_SERVICE) FakeAssistantManagerServiceImpl
    : public AssistantManagerService {
 public:
  FakeAssistantManagerServiceImpl();

  FakeAssistantManagerServiceImpl(const FakeAssistantManagerServiceImpl&) =
      delete;
  FakeAssistantManagerServiceImpl& operator=(
      const FakeAssistantManagerServiceImpl&) = delete;

  ~FakeAssistantManagerServiceImpl() override;

  void FinishStart();

  // assistant::AssistantManagerService overrides
  void Start(const std::optional<UserInfo>& user, bool enable_hotword) override;
  void Stop() override;
  void SetUser(const std::optional<UserInfo>& user) override;
  void EnableListening(bool enable) override;
  void EnableHotword(bool enable) override;
  void SetArcPlayStoreEnabled(bool enabled) override;
  void SetAssistantContextEnabled(bool enable) override;
  State GetState() const override;
  AssistantSettings* GetAssistantSettings() override;
  void AddAuthenticationStateObserver(
      AuthenticationStateObserver* observer) override {}
  void AddAndFireStateObserver(StateObserver* observer) override;
  void RemoveStateObserver(const StateObserver* observer) override;
  void SyncDeviceAppsStatus() override {}
  void UpdateInternalMediaPlayerStatus(MediaSessionAction action) override;

  // Assistant overrides:
  void StartEditReminderInteraction(const std::string& client_id) override;
  void StartTextInteraction(const std::string& query,
                            AssistantQuerySource source,
                            bool allow_tts) override;
  void StartVoiceInteraction() override;
  void StopActiveInteraction(bool cancel_conversation) override;
  void AddAssistantInteractionSubscriber(
      AssistantInteractionSubscriber* subscriber) override;
  void AddRemoteConversationObserver(ConversationObserver* observer) override {}
  void RemoveAssistantInteractionSubscriber(
      AssistantInteractionSubscriber* subscriber) override;
  mojo::PendingReceiver<libassistant::mojom::NotificationDelegate>
  GetPendingNotificationDelegate() override;
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

  // Update the state to the corresponding value, and inform the
  // |AssistantStateObserver| of the change.
  void SetStateAndInformObservers(State new_state);

  // Return the access token that was passed to |SetUser|.
  std::optional<std::string> access_token() { return access_token_; }
  // Return the Gaia ID that was passed to |SetUser|.
  std::optional<std::string> gaia_id() { return gaia_id_; }

  void Disconnected();

 private:
  // Send out a |AssistantStateObserver::OnStateChange(state)| event if we are
  // transitioning from a prior state to a later state.
  void MaybeSendStateChange(State state, State old_state, State target_state);

  State state_ = State::STOPPED;
  std::optional<std::string> gaia_id_;
  std::optional<std::string> access_token_;
  FakeAssistantSettingsImpl assistant_settings_;
  base::ObserverList<StateObserver> state_observers_;
};

}  // namespace ash::assistant

#endif  // CHROMEOS_ASH_SERVICES_ASSISTANT_TEST_SUPPORT_FAKE_ASSISTANT_MANAGER_SERVICE_IMPL_H_
