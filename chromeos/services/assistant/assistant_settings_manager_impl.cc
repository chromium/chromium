// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/assistant_settings_manager_impl.h"

namespace chromeos {
namespace assistant {

AssistantSettingsManagerImpl::AssistantSettingsManagerImpl(
    AssistantManagerService* assistant_manager_service)
    : assistant_manager_service_(assistant_manager_service) {}

AssistantSettingsManagerImpl::~AssistantSettingsManagerImpl() = default;

void AssistantSettingsManagerImpl::BindRequest(
    mojom::AssistantSettingsManagerRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void AssistantSettingsManagerImpl::GetSettings(const std::string& selector,
                                               GetSettingsCallback callback) {
  DCHECK(assistant_manager_service_->GetState() ==
         AssistantManagerService::State::RUNNING);
  // Wraps the callback into a repeating callback since the server side
  // interface requires the callback to be copyable.
  assistant_manager_service_->SendGetSettingsUiRequest(selector,
                                                       std::move(callback));
}

void AssistantSettingsManagerImpl::UpdateSettings(
    const std::string& update,
    GetSettingsCallback callback) {
  DCHECK(assistant_manager_service_->GetState() ==
         AssistantManagerService::State::RUNNING);
  // Wraps the callback into a repeating callback since the server side
  // interface requires the callback to be copyable.
  assistant_manager_service_->SendUpdateSettingsUiRequest(update,
                                                          std::move(callback));
}

void AssistantSettingsManagerImpl::StartSpeakerIdEnrollment(
    bool skip_cloud_enrollment,
    mojom::SpeakerIdEnrollmentClientPtr client) {
  DCHECK(assistant_manager_service_->GetState() ==
         AssistantManagerService::State::RUNNING);
  assistant_manager_service_->StartSpeakerIdEnrollment(skip_cloud_enrollment,
                                                       std::move(client));
}

void AssistantSettingsManagerImpl::StopSpeakerIdEnrollment(
    StopSpeakerIdEnrollmentCallback callback) {
  DCHECK(assistant_manager_service_->GetState() ==
         AssistantManagerService::State::RUNNING);
  assistant_manager_service_->StopSpeakerIdEnrollment(std::move(callback));
}

}  // namespace assistant
}  // namespace chromeos
