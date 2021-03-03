// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_FAKE_ASSISTANT_MANAGER_SERVICE_IMPL_H_
#define CHROMEOS_SERVICES_ASSISTANT_FAKE_ASSISTANT_MANAGER_SERVICE_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "chromeos/services/assistant/assistant_manager_service.h"
#include "chromeos/services/assistant/fake_assistant_settings_impl.h"

namespace chromeos {
namespace assistant {

using media_session::mojom::MediaSessionAction;

// Stub implementation of AssistantManagerService.  Should return deterministic
// result for testing.
class COMPONENT_EXPORT(ASSISTANT_SERVICE) FakeAssistantManagerServiceImpl
    : public AssistantManagerService {
 public:
  FakeAssistantManagerServiceImpl();
  ~FakeAssistantManagerServiceImpl() override;

  void FinishStart();

  // assistant::AssistantManagerService overrides
  void Start(const base::Optional<UserInfo>& user,
             bool enable_hotword) override;
  void Stop() override;
  void SetUser(const base::Optional<UserInfo>& user) override;
  void EnableListening(bool enable) override;
  void EnableHotword(bool enable) override;
  void SetArcPlayStoreEnabled(bool enabled) override;
  void SetAssistantContextEnabled(bool enable) override;
  State GetState() const override;
  AssistantSettings* GetAssistantSettings() override;
  void AddCommunicationErrorObserver(
      CommunicationErrorObserver* observer) override {}
  void RemoveCommunicationErrorObserver(
      const CommunicationErrorObserver* observer) override {}
  void AddAndFireStateObserver(StateObserver* observer) override;
  void RemoveStateObserver(const StateObserver* observer) override;
  void SyncDeviceAppsStatus() override {}
  void UpdateInternalMediaPlayerStatus(MediaSessionAction action) override;

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

  // Update the state to the corresponding value, and inform the
  // |AssistantStateObserver| of the change.
  void SetStateAndInformObservers(State new_state);

  // Return the access token that was passed to |SetUser|.
  base::Optional<std::string> access_token() { return access_token_; }
  // Return the Gaia ID that was passed to |SetUser|.
  base::Optional<std::string> gaia_id() { return gaia_id_; }

 private:
  // Send out a |AssistantStateObserver::OnStateChange(state)| event if we are
  // transitioning from a prior state to a later state.
  void MaybeSendStateChange(State state, State old_state, State target_state);

  State state_ = State::STOPPED;
  base::Optional<std::string> gaia_id_;
  base::Optional<std::string> access_token_;
  FakeAssistantSettingsImpl assistant_settings_;
  base::ObserverList<StateObserver> state_observers_;

  DISALLOW_COPY_AND_ASSIGN(FakeAssistantManagerServiceImpl);
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_FAKE_ASSISTANT_MANAGER_SERVICE_IMPL_H_
