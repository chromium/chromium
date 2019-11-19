// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/assistant_settings_manager_impl.h"

#include <utility>

#include "ash/public/cpp/assistant/assistant_state_base.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "chromeos/dbus/util/version_loader.h"
#include "chromeos/services/assistant/assistant_manager_service_impl.h"
#include "chromeos/services/assistant/constants.h"
#include "chromeos/services/assistant/cros_platform_api.h"
#include "chromeos/services/assistant/public/features.h"
#include "chromeos/services/assistant/public/proto/assistant_device_settings_ui.pb.h"
#include "chromeos/services/assistant/public/proto/settings_ui.pb.h"
#include "chromeos/services/assistant/service_context.h"
#include "libassistant/shared/internal_api/assistant_manager_internal.h"

using SpeakerIdEnrollmentState =
    assistant_client::SpeakerIdEnrollmentUpdate::State;

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

AssistantSettingsManagerImpl::AssistantSettingsManagerImpl(
    ServiceContext* context,
    AssistantManagerServiceImpl* assistant_manager_service)
    : context_(context),
      assistant_manager_service_(assistant_manager_service),
      weak_factory_(this) {}

AssistantSettingsManagerImpl::~AssistantSettingsManagerImpl() = default;

void AssistantSettingsManagerImpl::BindReceiver(
    mojo::PendingReceiver<mojom::AssistantSettingsManager> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void AssistantSettingsManagerImpl::GetSettings(const std::string& selector,
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
          [repeating_callback =
               base::AdaptCallbackForRepeating(std::move(callback)),
           task_runner = main_task_runner()](
              const assistant_client::VoicelessResponse& response) {
            // This callback may be called from server multiple times. We should
            // only process non-empty response.
            std::string settings = UnwrapGetSettingsUiResponse(response);
            task_runner->PostTask(
                FROM_HERE,
                base::BindOnce(
                    [](base::RepeatingCallback<void(const std::string&)>
                           callback,
                       const std::string& result) { callback.Run(result); },
                    repeating_callback, settings));
          });
}

void AssistantSettingsManagerImpl::UpdateSettings(
    const std::string& update,
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

void AssistantSettingsManagerImpl::StartSpeakerIdEnrollment(
    bool skip_cloud_enrollment,
    mojo::PendingRemote<mojom::SpeakerIdEnrollmentClient> client) {
  DCHECK(HasStarted(assistant_manager_service_));
  DCHECK(main_task_runner()->RunsTasksInCurrentSequence());

  assistant_manager_service_->platform_api()->SetMicState(true);

  if (!assistant_manager_service_->assistant_manager_internal())
    return;

  speaker_id_enrollment_client_.Bind(std::move(client));

  assistant_client::SpeakerIdEnrollmentConfig client_config;
  client_config.user_id = kUserID;
  client_config.skip_cloud_enrollment = skip_cloud_enrollment;

  assistant_manager_service_->assistant_manager_internal()
      ->StartSpeakerIdEnrollment(
          client_config,
          [weak_ptr = weak_factory_.GetWeakPtr(),
           task_runner = main_task_runner()](
              const assistant_client::SpeakerIdEnrollmentUpdate& update) {
            task_runner->PostTask(
                FROM_HERE, base::BindOnce(&AssistantSettingsManagerImpl::
                                              HandleSpeakerIdEnrollmentUpdate,
                                          weak_ptr, update));
          });
}

void AssistantSettingsManagerImpl::StopSpeakerIdEnrollment(
    StopSpeakerIdEnrollmentCallback callback) {
  DCHECK(HasStarted(assistant_manager_service_));
  DCHECK(main_task_runner()->RunsTasksInCurrentSequence());

  assistant_manager_service_->platform_api()->SetMicState(false);

  if (!assistant_manager_service_->assistant_manager_internal()) {
    std::move(callback).Run();
    return;
  }

  assistant_manager_service_->assistant_manager_internal()
      ->StopSpeakerIdEnrollment([repeating_callback =
                                     base::AdaptCallbackForRepeating(
                                         std::move(callback)),
                                 task_runner = main_task_runner(),
                                 weak_ptr = weak_factory_.GetWeakPtr()]() {
        task_runner->PostTask(
            FROM_HERE,
            base::BindOnce(
                &AssistantSettingsManagerImpl::HandleStopSpeakerIdEnrollment,
                std::move(weak_ptr), repeating_callback));
      });
}

void AssistantSettingsManagerImpl::SyncSpeakerIdEnrollmentStatus() {
  DCHECK(main_task_runner()->RunsTasksInCurrentSequence());

  if (assistant_state()->allowed_state() !=
          ash::mojom::AssistantAllowedState::ALLOWED ||
      features::IsVoiceMatchDisabled()) {
    return;
  }

  assistant_manager_service_->assistant_manager_internal()
      ->GetSpeakerIdEnrollmentStatus(
          kUserID,
          [weak_ptr = weak_factory_.GetWeakPtr(),
           task_runner = main_task_runner()](
              const assistant_client::SpeakerIdEnrollmentStatus& status) {
            task_runner->PostTask(
                FROM_HERE,
                base::BindOnce(&AssistantSettingsManagerImpl::
                                   HandleSpeakerIdEnrollmentStatusSync,
                               weak_ptr, status));
          });
}

void AssistantSettingsManagerImpl::SyncDeviceAppsStatus(
    base::OnceCallback<void(bool)> callback) {
  DCHECK(main_task_runner()->RunsTasksInCurrentSequence());

  SettingsUiSelector selector;
  ConsentFlowUiSelector* consent_flow_ui =
      selector.mutable_consent_flow_ui_selector();
  consent_flow_ui->set_flow_id(
      ActivityControlSettingsUiSelector::ASSISTANT_SUW_ONBOARDING_ON_CHROME_OS);
  selector.set_gaia_user_context_ui(true);
  GetSettings(
      selector.SerializeAsString(),
      base::BindOnce(&AssistantSettingsManagerImpl::HandleDeviceAppsStatusSync,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void AssistantSettingsManagerImpl::HandleSpeakerIdEnrollmentUpdate(
    const assistant_client::SpeakerIdEnrollmentUpdate& update) {
  DCHECK(main_task_runner()->RunsTasksInCurrentSequence());
  switch (update.state) {
    case SpeakerIdEnrollmentState::LISTEN:
      speaker_id_enrollment_client_->OnListeningHotword();
      break;
    case SpeakerIdEnrollmentState::PROCESS:
      speaker_id_enrollment_client_->OnProcessingHotword();
      break;
    case SpeakerIdEnrollmentState::DONE:
      speaker_id_enrollment_client_->OnSpeakerIdEnrollmentDone();
      if (!speaker_id_enrollment_done_) {
        speaker_id_enrollment_done_ = true;
        assistant_manager_service_->UpdateInternalOptions(
            assistant_manager_service_->assistant_manager_internal());
      }
      break;
    case SpeakerIdEnrollmentState::FAILURE:
      speaker_id_enrollment_client_->OnSpeakerIdEnrollmentFailure();
      break;
    case SpeakerIdEnrollmentState::INIT:
    case SpeakerIdEnrollmentState::CHECK:
    case SpeakerIdEnrollmentState::UPLOAD:
    case SpeakerIdEnrollmentState::FETCH:
      break;
  }
}

void AssistantSettingsManagerImpl::HandleSpeakerIdEnrollmentStatusSync(
    const assistant_client::SpeakerIdEnrollmentStatus& status) {
  DCHECK(main_task_runner()->RunsTasksInCurrentSequence());

  speaker_id_enrollment_done_ = status.user_model_exists;

  if (speaker_id_enrollment_done_) {
    assistant_manager_service_->UpdateInternalOptions(
        assistant_manager_service_->assistant_manager_internal());

  } else {
    // If hotword is enabled but there is no voice model found, launch the
    // enrollment flow.
    if (assistant_state()->hotword_enabled().value())
      assistant_controller()->StartSpeakerIdEnrollmentFlow();
  }
}

ash::AssistantStateBase* AssistantSettingsManagerImpl::assistant_state() {
  return context_->assistant_state();
}

mojom::AssistantController*
AssistantSettingsManagerImpl::assistant_controller() {
  return context_->assistant_controller();
}

scoped_refptr<base::SequencedTaskRunner>
AssistantSettingsManagerImpl::main_task_runner() {
  return context_->main_task_runner();
}

void AssistantSettingsManagerImpl::HandleStopSpeakerIdEnrollment(
    base::RepeatingCallback<void()> callback) {
  DCHECK(main_task_runner()->RunsTasksInCurrentSequence());
  speaker_id_enrollment_client_.reset();
  callback.Run();
}

void AssistantSettingsManagerImpl::HandleDeviceAppsStatusSync(
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

void AssistantSettingsManagerImpl::UpdateServerDeviceSettings() {
  DCHECK(main_task_runner()->RunsTasksInCurrentSequence());

  const std::string device_id =
      assistant_manager_service_->assistant_manager()->GetDeviceId();
  if (device_id.empty())
    return;

  // Update device id and device type.
  assistant::SettingsUiUpdate update;
  assistant::AssistantDeviceSettingsUpdate* device_settings_update =
      update.mutable_assistant_device_settings_update()
          ->add_assistant_device_settings_update();
  device_settings_update->set_device_id(device_id);
  device_settings_update->set_assistant_device_type(
      assistant::AssistantDevice::CROS);

  if (assistant_state()->hotword_enabled().value()) {
    device_settings_update->mutable_device_settings()->set_speaker_id_enabled(
        true);
  }

  VLOG(1) << "Update assistant device locale: "
          << assistant_state()->locale().value();
  device_settings_update->mutable_device_settings()->set_locale(
      assistant_state()->locale().value());

  // Enable personal readout to grant permission for personal features.
  device_settings_update->mutable_device_settings()->set_personal_readout(
      assistant::AssistantDeviceSettings::PERSONAL_READOUT_ENABLED);

  // Device settings update result is not handled because it is not included in
  // the SettingsUiUpdateResult.
  UpdateSettings(update.SerializeAsString(), base::DoNothing());
}

}  // namespace assistant
}  // namespace chromeos
