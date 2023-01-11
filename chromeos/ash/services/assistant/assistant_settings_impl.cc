// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/assistant/assistant_settings_impl.h"

#include <utility>

#include "ash/public/cpp/assistant/assistant_state_base.h"
#include "ash/public/cpp/assistant/controller/assistant_controller.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "chromeos/ash/services/assistant/public/cpp/features.h"
#include "chromeos/ash/services/assistant/public/proto/settings_ui.pb.h"
#include "chromeos/ash/services/assistant/service_context.h"
#include "chromeos/ash/services/libassistant/public/mojom/settings_controller.mojom.h"
#include "chromeos/ash/services/libassistant/public/mojom/speaker_id_enrollment_controller.mojom.h"
#include "chromeos/version/version_loader.h"

namespace ash::assistant {

AssistantSettingsImpl::AssistantSettingsImpl(ServiceContext* context)
    : context_(context) {}

AssistantSettingsImpl::~AssistantSettingsImpl() = default;

void AssistantSettingsImpl::Initialize(
    mojo::PendingRemote<libassistant::mojom::SpeakerIdEnrollmentController>
        remote,
    libassistant::mojom::SettingsController* settings_controller) {
  DCHECK(!settings_controller_);

  speaker_id_enrollment_remote_.Bind(std::move(remote));
  settings_controller_ = settings_controller;
}

void AssistantSettingsImpl::Stop() {
  speaker_id_enrollment_remote_.reset();
  settings_controller_ = nullptr;
}

void AssistantSettingsImpl::GetSettings(const std::string& selector,
                                        GetSettingsCallback callback) {
  settings_controller().GetSettings(selector, /*include_header=*/false,
                                    std::move(callback));
}

void AssistantSettingsImpl::GetSettingsWithHeader(
    const std::string& selector,
    GetSettingsCallback callback) {
  settings_controller().GetSettings(selector, /*include_header=*/true,
                                    std::move(callback));
}

void AssistantSettingsImpl::UpdateSettings(const std::string& update,
                                           GetSettingsCallback callback) {
  settings_controller().UpdateSettings(update, std::move(callback));
}

void AssistantSettingsImpl::StartSpeakerIdEnrollment(
    bool skip_cloud_enrollment,
    base::WeakPtr<SpeakerIdEnrollmentClient> client) {
  DCHECK(speaker_id_enrollment_remote_.is_bound());

  speaker_id_enrollment_remote_->StartSpeakerIdEnrollment(
      context_->primary_account_gaia_id(), skip_cloud_enrollment,
      client->BindNewPipeAndPassRemote());
}

void AssistantSettingsImpl::StopSpeakerIdEnrollment() {
  DCHECK(speaker_id_enrollment_remote_.is_bound());

  speaker_id_enrollment_remote_->StopSpeakerIdEnrollment();
}

void AssistantSettingsImpl::SyncSpeakerIdEnrollmentStatus() {
  if (assistant_state()->allowed_state() != AssistantAllowedState::ALLOWED ||
      features::IsVoiceMatchDisabled()) {
    return;
  }

  DCHECK(speaker_id_enrollment_remote_.is_bound());

  speaker_id_enrollment_remote_->GetSpeakerIdEnrollmentStatus(
      context_->primary_account_gaia_id(),
      base::BindOnce(
          &AssistantSettingsImpl::HandleSpeakerIdEnrollmentStatusSync,
          weak_factory_.GetWeakPtr()));
}

void AssistantSettingsImpl::SyncDeviceAppsStatus(
    base::OnceCallback<void(bool)> callback) {
  SettingsUiSelector selector;
  ConsentFlowUiSelector* consent_flow_ui =
      selector.mutable_consent_flow_ui_selector();
  consent_flow_ui->set_flow_id(
      ActivityControlSettingsUiSelector::ASSISTANT_SUW_ONBOARDING_ON_CHROME_OS);
  selector.set_gaia_user_context_ui(true);
  GetSettings(selector.SerializeAsString(),
              base::BindOnce(&AssistantSettingsImpl::HandleDeviceAppsStatusSync,
                             weak_factory_.GetWeakPtr(), std::move(callback)));
}

void AssistantSettingsImpl::HandleSpeakerIdEnrollmentStatusSync(
    libassistant::mojom::SpeakerIdEnrollmentStatusPtr status) {
  if (!status->user_model_exists) {
    // If hotword is enabled but there is no voice model found, launch the
    // enrollment flow.
    if (assistant_state()->hotword_enabled().value())
      assistant_controller()->StartSpeakerIdEnrollmentFlow();
  }
}

void AssistantSettingsImpl::HandleDeviceAppsStatusSync(
    base::OnceCallback<void(bool)> callback,
    const std::string& settings) {
  if (settings.empty()) {
    // Note: we deliberately do not log an error here, as this happens quite
    // regularly when there is a network issue during signup. See b/151321970.
    DVLOG(1) << "Assistant: Error while syncing device apps status.";
    std::move(callback).Run(false);
    return;
  }

  SettingsUi settings_ui;
  if (!settings_ui.ParseFromString(settings)) {
    LOG(ERROR) << "Failed to parse the response proto, set the DA bit to false";
    std::move(callback).Run(false);
    return;
  }

  if (!settings_ui.has_gaia_user_context_ui()) {
    LOG(ERROR) << "Failed to get gaia user context, set the DA bit to false";
    std::move(callback).Run(false);
    return;
  }

  const auto& gaia_user_context_ui = settings_ui.gaia_user_context_ui();
  if (!gaia_user_context_ui.has_device_apps_enabled()) {
    LOG(ERROR) << "Failed to get the device apps bit, set it to false";
    std::move(callback).Run(false);
    return;
  }

  std::move(callback).Run(gaia_user_context_ui.device_apps_enabled());
}

AssistantStateBase* AssistantSettingsImpl::assistant_state() {
  return context_->assistant_state();
}

AssistantController* AssistantSettingsImpl::assistant_controller() {
  return context_->assistant_controller();
}

libassistant::mojom::SettingsController&
AssistantSettingsImpl::settings_controller() {
  DCHECK(settings_controller_);
  return *settings_controller_;
}

}  // namespace ash::assistant
