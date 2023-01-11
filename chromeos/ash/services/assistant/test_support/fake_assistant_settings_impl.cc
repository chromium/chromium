// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/assistant/test_support/fake_assistant_settings_impl.h"

#include <utility>

#include "base/functional/callback.h"
#include "chromeos/ash/services/assistant/public/proto/get_settings_ui.pb.h"
#include "chromeos/ash/services/assistant/public/proto/settings_ui.pb.h"

namespace ash::assistant {

FakeAssistantSettingsImpl::FakeAssistantSettingsImpl() = default;

FakeAssistantSettingsImpl::~FakeAssistantSettingsImpl() = default;

void FakeAssistantSettingsImpl::GetSettings(const std::string& selector,
                                            GetSettingsCallback callback) {
  // Create a fake response
  SettingsUi settings_ui;
  settings_ui.mutable_consent_flow_ui()->set_consent_status(
      ConsentFlowUi_ConsentStatus_ALREADY_CONSENTED);
  std::move(callback).Run(settings_ui.SerializeAsString());
}

void FakeAssistantSettingsImpl::GetSettingsWithHeader(
    const std::string& selector,
    GetSettingsCallback callback) {
  // Create a fake response
  assistant::GetSettingsUiResponse response;
  response.mutable_settings()->mutable_consent_flow_ui()->set_consent_status(
      ConsentFlowUi_ConsentStatus_ALREADY_CONSENTED);
  std::move(callback).Run(response.SerializeAsString());
}

void FakeAssistantSettingsImpl::UpdateSettings(
    const std::string& update,
    UpdateSettingsCallback callback) {
  std::move(callback).Run(std::string());
}

void FakeAssistantSettingsImpl::StartSpeakerIdEnrollment(
    bool skip_cloud_enrollment,
    base::WeakPtr<SpeakerIdEnrollmentClient> client) {
  client->OnSpeakerIdEnrollmentDone();
}

void FakeAssistantSettingsImpl::StopSpeakerIdEnrollment() {}

}  // namespace ash::assistant
