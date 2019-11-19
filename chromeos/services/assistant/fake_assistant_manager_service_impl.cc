// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/fake_assistant_manager_service_impl.h"

#include <utility>

namespace chromeos {
namespace assistant {

FakeAssistantManagerServiceImpl::FakeAssistantManagerServiceImpl() = default;

FakeAssistantManagerServiceImpl::~FakeAssistantManagerServiceImpl() = default;

void FakeAssistantManagerServiceImpl::FinishStart() {
  SetStateAndInformObservers(State::RUNNING);
}

void FakeAssistantManagerServiceImpl::Start(
    const base::Optional<std::string>& access_token,
    bool enable_hotword) {
  SetStateAndInformObservers(State::STARTING);
}

void FakeAssistantManagerServiceImpl::Stop() {
  SetStateAndInformObservers(State::STOPPED);
}

void FakeAssistantManagerServiceImpl::SetAccessToken(
    const std::string& access_token) {}

void FakeAssistantManagerServiceImpl::EnableListening(bool enable) {}

void FakeAssistantManagerServiceImpl::EnableHotword(bool enable) {}

void FakeAssistantManagerServiceImpl::SetArcPlayStoreEnabled(bool enabled) {}

AssistantManagerService::State FakeAssistantManagerServiceImpl::GetState()
    const {
  return state_;
}

AssistantSettingsManager*
FakeAssistantManagerServiceImpl::GetAssistantSettingsManager() {
  return &assistant_settings_manager_;
}

void FakeAssistantManagerServiceImpl::AddAndFireStateObserver(
    StateObserver* observer) {
  state_observers_.AddObserver(observer);
  observer->OnStateChanged(GetState());
}

void FakeAssistantManagerServiceImpl::RemoveStateObserver(
    const StateObserver* observer) {
  state_observers_.RemoveObserver(observer);
}

void FakeAssistantManagerServiceImpl::StartCachedScreenContextInteraction() {}

void FakeAssistantManagerServiceImpl::StartEditReminderInteraction(
    const std::string& client_id) {}

void FakeAssistantManagerServiceImpl::StartMetalayerInteraction(
    const gfx::Rect& region) {}

void FakeAssistantManagerServiceImpl::StartTextInteraction(
    const std::string& query,
    mojom::AssistantQuerySource source,
    bool allow_tts) {}

void FakeAssistantManagerServiceImpl::StartVoiceInteraction() {}

void FakeAssistantManagerServiceImpl::StartWarmerWelcomeInteraction(
    int num_warmer_welcome_triggered,
    bool allow_tts) {}

void FakeAssistantManagerServiceImpl::StopActiveInteraction(
    bool cancel_conversation) {}

void FakeAssistantManagerServiceImpl::AddAssistantInteractionSubscriber(
    mojo::PendingRemote<mojom::AssistantInteractionSubscriber> subscriber) {}

void FakeAssistantManagerServiceImpl::RetrieveNotification(
    mojom::AssistantNotificationPtr notification,
    int action_index) {}

void FakeAssistantManagerServiceImpl::DismissNotification(
    mojom::AssistantNotificationPtr notification) {}

void FakeAssistantManagerServiceImpl::CacheScreenContext(
    CacheScreenContextCallback callback) {
  std::move(callback).Run();
}

void FakeAssistantManagerServiceImpl::ClearScreenContextCache() {}

void FakeAssistantManagerServiceImpl::OnAccessibilityStatusChanged(
    bool spoken_feedback_enabled) {}

void FakeAssistantManagerServiceImpl::SendAssistantFeedback(
    mojom::AssistantFeedbackPtr feedback) {}

void FakeAssistantManagerServiceImpl::StopAlarmTimerRinging() {}
void FakeAssistantManagerServiceImpl::CreateTimer(base::TimeDelta duration) {}

void FakeAssistantManagerServiceImpl::SetStateAndInformObservers(
    State new_state) {
  State old_state = state_;
  state_ = new_state;

  // In reality we will not skip states, i.e. we will always get |STARTING|
  // before ever encountering |STARTED|. As such our fake implementation will
  // send out all intermediate states between |old_state| and |new_state|.
  MaybeSendStateChange(State::STOPPED, old_state, new_state);
  MaybeSendStateChange(State::STARTING, old_state, new_state);
  MaybeSendStateChange(State::STARTED, old_state, new_state);
  MaybeSendStateChange(State::RUNNING, old_state, new_state);
}

void FakeAssistantManagerServiceImpl::MaybeSendStateChange(State state,
                                                           State old_state,
                                                           State target_state) {
  if (state > old_state && state <= target_state) {
    for (auto& observer : state_observers_)
      observer.OnStateChanged(state);
  }
}

}  // namespace assistant
}  // namespace chromeos
