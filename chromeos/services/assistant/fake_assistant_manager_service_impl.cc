// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/fake_assistant_manager_service_impl.h"

namespace chromeos {
namespace assistant {

FakeAssistantManagerServiceImpl::FakeAssistantManagerServiceImpl() = default;

FakeAssistantManagerServiceImpl::~FakeAssistantManagerServiceImpl() = default;

void FakeAssistantManagerServiceImpl::Start(const std::string& access_token,
                                            base::OnceClosure callback) {
  state_ = State::RUNNING;

  if (callback)
    std::move(callback).Run();
}

void FakeAssistantManagerServiceImpl::Stop() {
  state_ = State::STOPPED;
}

void FakeAssistantManagerServiceImpl::SetAccessToken(
    const std::string& access_token) {}

void FakeAssistantManagerServiceImpl::EnableListening(bool enable) {}

AssistantManagerService::State FakeAssistantManagerServiceImpl::GetState()
    const {
  return state_;
}

AssistantSettingsManager*
FakeAssistantManagerServiceImpl::GetAssistantSettingsManager() {
  return nullptr;
}

void FakeAssistantManagerServiceImpl::SendGetSettingsUiRequest(
    const std::string& selector,
    GetSettingsUiResponseCallback callback) {}

void FakeAssistantManagerServiceImpl::SendUpdateSettingsUiRequest(
    const std::string& update,
    UpdateSettingsUiResponseCallback callback) {}

void FakeAssistantManagerServiceImpl::StartSpeakerIdEnrollment(
    bool skip_cloud_enrollment,
    mojom::SpeakerIdEnrollmentClientPtr client) {}

void FakeAssistantManagerServiceImpl::StopSpeakerIdEnrollment(
    AssistantSettingsManager::StopSpeakerIdEnrollmentCallback on_stopped) {}

void FakeAssistantManagerServiceImpl::StartCachedScreenContextInteraction() {}

void FakeAssistantManagerServiceImpl::StartMetalayerInteraction(
    const gfx::Rect& region) {}

void FakeAssistantManagerServiceImpl::StartVoiceInteraction() {}

void FakeAssistantManagerServiceImpl::StopActiveInteraction(
    bool cancel_conversation) {}

void FakeAssistantManagerServiceImpl::SendTextQuery(const std::string& query) {}

void FakeAssistantManagerServiceImpl::AddAssistantInteractionSubscriber(
    mojom::AssistantInteractionSubscriberPtr subscriber) {}

void FakeAssistantManagerServiceImpl::AddAssistantNotificationSubscriber(
    mojom::AssistantNotificationSubscriberPtr subscriber) {}

void FakeAssistantManagerServiceImpl::RetrieveNotification(
    mojom::AssistantNotificationPtr notification,
    int action_index) {}

void FakeAssistantManagerServiceImpl::DismissNotification(
    mojom::AssistantNotificationPtr notification) {}

void FakeAssistantManagerServiceImpl::CacheScreenContext(
    CacheScreenContextCallback callback) {}

void FakeAssistantManagerServiceImpl::OnAccessibilityStatusChanged(
    bool spoken_feedback_enabled) {}

}  // namespace assistant
}  // namespace chromeos
