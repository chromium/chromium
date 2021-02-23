// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/assistant_settings_impl.h"

#include <utility>

#include "ash/public/cpp/assistant/assistant_state_base.h"
#include "ash/public/cpp/assistant/controller/assistant_controller.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "chromeos/dbus/util/version_loader.h"
#include "chromeos/services/assistant/assistant_manager_service_impl.h"
#include "chromeos/services/assistant/public/cpp/features.h"
#include "chromeos/services/assistant/public/proto/settings_ui.pb.h"
#include "chromeos/services/assistant/service_context.h"
#include "chromeos/services/libassistant/public/mojom/speaker_id_enrollment_controller.mojom.h"
#include "libassistant/shared/internal_api/assistant_manager_internal.h"

using SpeakerIdEnrollmentState =
    assistant_client::SpeakerIdEnrollmentUpdate::State;
using VoicelessResponseStatus = assistant_client::VoicelessResponse::Status;

namespace chromeos {
namespace assistant {

namespace {

bool HasStarted(const AssistantManagerService* assistant_manager_service) {
  return (assistant_manager_service->GetState() ==
              AssistantManagerService::STARTED ||
          assistant_manager_service->GetState() ==
              AssistantManagerService::RUNNING);
}

}  // namespace

AssistantSettingsImpl::AssistantSettingsImpl(
    ServiceContext* context,
    AssistantManagerServiceImpl* assistant_manager_service)
    : context_(context),
      assistant_manager_service_(assistant_manager_service) {}

AssistantSettingsImpl::~AssistantSettingsImpl() = default;

void AssistantSettingsImpl::Initialize(
    mojo::PendingRemote<
        ::chromeos::libassistant::mojom::SpeakerIdEnrollmentController>
        remote) {
  speaker_id_enrollment_remote_.Bind(std::move(remote));
}

void AssistantSettingsImpl::GetSettings(const std::string& selector,
                                        GetSettingsCallback callback) {
  DCHECK(HasStarted(assistant_manager_service_));
  DCHECK(main_task_runner()->RunsTasksInCurrentSequence());

  // TODO(xiaohuic): libassistant could be restarting for various reasons. In
  // this case the remote side may not know or care and continues to send
  // requests that would need libassistant. We need a better approach to handle
  // this and ideally libassistant should not need to restart.
  if (!assistant_manager_service_->assistant_manager_internal()) {
    std::move(callback).Run(std::string());
    return;
  }

  // Wraps the callback into a repeating callback since the server side
  // interface requires the callback to be copyable.
  std::string serialized_proto = SerializeGetSettingsUiRequest(selector);
  assistant_manager_service_->assistant_manager_internal()
      ->SendGetSettingsUiRequest(
          serialized_proto, std::string(),
          [weak_ptr = weak_factory_.GetWeakPtr(),
           repeating_callback =
               base::AdaptCallbackForRepeating(std::move(callback)),
           task_runner = main_task_runner()](
              const assistant_client::VoicelessResponse& response) {
            task_runner->PostTask(
                FROM_HERE,
                base::BindOnce(
                    [](const base::WeakPtr<AssistantSettingsImpl> weak_ptr,
                       const assistant_client::VoicelessResponse response,
                       base::RepeatingCallback<void(const std::string&)>
                           callback) {
                      if (weak_ptr && !weak_ptr->ShouldIgnoreResponse(response))
                        callback.Run(UnwrapGetSettingsUiResponse(response));
                    },
                    weak_ptr, response, repeating_callback));
          });
}

void AssistantSettingsImpl::UpdateSettings(const std::string& update,
                                           GetSettingsCallback callback) {
  DCHECK(HasStarted(assistant_manager_service_));
  DCHECK(main_task_runner()->RunsTasksInCurrentSequence());

  if (!assistant_manager_service_->assistant_manager_internal()) {
    std::move(callback).Run(std::string());
    return;
  }

  // Wraps the callback into a repeating callback since the server side
  // interface requires the callback to be copyable.
  std::string serialized_proto = SerializeUpdateSettingsUiRequest(update);
  assistant_manager_service_->assistant_manager_internal()
      ->SendUpdateSettingsUiRequest(
          serialized_proto, std::string(),
          [repeating_callback =
               base::AdaptCallbackForRepeating(std::move(callback)),
           task_runner = main_task_runner()](
              const assistant_client::VoicelessResponse& response) {
            // This callback may be called from server multiple times. We should
            // only process non-empty response.
            std::string update = UnwrapUpdateSettingsUiResponse(response);
            task_runner->PostTask(
                FROM_HERE,
                base::BindOnce(
                    [](base::RepeatingCallback<void(const std::string&)>
                           callback,
                       const std::string& result) { callback.Run(result); },
                    repeating_callback, update));
          });
}

void AssistantSettingsImpl::StartSpeakerIdEnrollment(
    bool skip_cloud_enrollment,
    base::WeakPtr<SpeakerIdEnrollmentClient> client) {
  DCHECK(HasStarted(assistant_manager_service_));
  DCHECK(speaker_id_enrollment_remote_.is_bound());

  speaker_id_enrollment_remote_->StartSpeakerIdEnrollment(
      context_->primary_account_gaia_id(), skip_cloud_enrollment,
      client->BindNewPipeAndPassRemote());
}

void AssistantSettingsImpl::StopSpeakerIdEnrollment() {
  DCHECK(HasStarted(assistant_manager_service_));
  DCHECK(speaker_id_enrollment_remote_.is_bound());

  speaker_id_enrollment_remote_->StopSpeakerIdEnrollment();
}

void AssistantSettingsImpl::SyncSpeakerIdEnrollmentStatus() {
  if (assistant_state()->allowed_state() !=
          chromeos::assistant::AssistantAllowedState::ALLOWED ||
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
  DCHECK(main_task_runner()->RunsTasksInCurrentSequence());

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
  DCHECK(main_task_runner()->RunsTasksInCurrentSequence());

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

bool AssistantSettingsImpl::ShouldIgnoreResponse(
    const assistant_client::VoicelessResponse& response) const {
  // If cancellation is indicated, we'll ignore |response|. This is currently
  // only known to occur in browser testing when attempting to replay an S3
  // session that was not previously recorded.
  if (response.error_code == "CANCELLED") {
    VLOG(1) << "Ignore settings response due to cancellation.";
    return true;
  }

  // If NO_RESPONSE_ERROR is indicated, we'll check to see if LibAssistant is
  // restarting/shutting down. If so, we'll ignore |response| to avoid
  // propagating fallback values. This may occur if the user quickly toggles
  // Assistant enabled/disabled in settings.
  if (response.status == VoicelessResponseStatus::NO_RESPONSE_ERROR &&
      !assistant_manager_service_->assistant_manager_internal()) {
    VLOG(1) << "Ignore settings response due to LibAssistant restart/shutdown.";
    return true;
  }

  // Otherwise we'll allow |response| processing to proceed.
  return false;
}

ash::AssistantStateBase* AssistantSettingsImpl::assistant_state() {
  return context_->assistant_state();
}

ash::AssistantController* AssistantSettingsImpl::assistant_controller() {
  return context_->assistant_controller();
}

scoped_refptr<base::SequencedTaskRunner>
AssistantSettingsImpl::main_task_runner() {
  return context_->main_task_runner();
}

}  // namespace assistant
}  // namespace chromeos
