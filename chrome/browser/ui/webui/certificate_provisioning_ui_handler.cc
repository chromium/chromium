// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/certificate_provisioning_ui_handler.h"

#include <string>

#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/i18n/time_formatting.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/ash/cert_provisioning/cert_provisioning_common.h"
#include "chrome/browser/ash/cert_provisioning/cert_provisioning_scheduler.h"
#include "chrome/browser/ash/cert_provisioning/cert_provisioning_scheduler_user_service.h"
#include "chrome/browser/ash/cert_provisioning/cert_provisioning_worker.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/common/net/x509_certificate_model.h"
#include "chrome/common/net/x509_certificate_model_nss.h"
#include "chrome/grit/generated_resources.h"
#include "components/policy/core/browser/cloud/message_util.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/user_manager/user.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"

static_assert(
    BUILDFLAG(IS_CHROMEOS),
    "CertificateProvisioningUIHandler is available only for ChromeOS");

using ash::cert_provisioning::BackendServerError;
using ash::cert_provisioning::CertProvisioningScheduler;
using ash::cert_provisioning::CertProvisioningSchedulerUserServiceFactory;
using ash::cert_provisioning::CertProvisioningWorkerState;

namespace chromeos::cert_provisioning {

namespace {

CertProvisioningScheduler* GetUserScheduler(Profile* profile) {
  if (!ash::ProfileHelper::IsPrimaryProfile(profile)) {
    return nullptr;
  }

  auto* user_service =
      CertProvisioningSchedulerUserServiceFactory::GetForProfile(profile);
  if (!user_service) {
    return nullptr;
  }

  return user_service->scheduler();
}

CertProvisioningScheduler* GetDeviceScheduler(Profile* profile) {
  if (!ash::ProfileHelper::IsPrimaryProfile(profile)) {
    return nullptr;
  }

  const user_manager::User* user =
      ash::ProfileHelper::Get()->GetUserByProfile(profile);
  if (!user || !user->IsAffiliated()) {
    return nullptr;
  }

  policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  return connector->GetDeviceCertProvisioningScheduler();
}

// Returns localized representation for the state of a certificate provisioning
// process.
std::u16string StateToText(CertProvisioningWorkerState state) {
  switch (state) {
    case CertProvisioningWorkerState ::kInitState:
      return l10n_util::GetStringUTF16(
          IDS_SETTINGS_CERTIFICATE_MANAGER_PROVISIONING_STATUS_PREPARING_CSR);
    case CertProvisioningWorkerState ::kKeypairGenerated:
      return l10n_util::GetStringUTF16(
          IDS_SETTINGS_CERTIFICATE_MANAGER_PROVISIONING_STATUS_PREPARING_CSR_WAITING);
    case CertProvisioningWorkerState::kStartCsrResponseReceived:
      // Intentional fall-through.
    case CertProvisioningWorkerState::kVaChallengeFinished:
      // Intentional fall-through.
    case CertProvisioningWorkerState::kKeyRegistered:
      // Intentional fall-through.
    case CertProvisioningWorkerState::kKeypairMarked:
      return l10n_util::GetStringUTF16(
          IDS_SETTINGS_CERTIFICATE_MANAGER_PROVISIONING_STATUS_PREPARING_CSR);
    case CertProvisioningWorkerState::kSignCsrFinished:
      return l10n_util::GetStringUTF16(
          IDS_SETTINGS_CERTIFICATE_MANAGER_PROVISIONING_STATUS_PREPARING_CSR_WAITING);
    case CertProvisioningWorkerState::kFinishCsrResponseReceived:
      return l10n_util::GetStringUTF16(
          IDS_SETTINGS_CERTIFICATE_MANAGER_PROVISIONING_STATUS_WAITING_FOR_CA);
    case CertProvisioningWorkerState::kSucceeded:
      return l10n_util::GetStringUTF16(
          IDS_SETTINGS_CERTIFICATE_MANAGER_PROVISIONING_STATUS_SUCCESS);
    case CertProvisioningWorkerState::kFailed:
      return l10n_util::GetStringUTF16(
          IDS_SETTINGS_CERTIFICATE_MANAGER_PROVISIONING_STATUS_FAILURE);
    case CertProvisioningWorkerState::kInconsistentDataError:
      return l10n_util::GetStringUTF16(
          IDS_SETTINGS_CERTIFICATE_MANAGER_PROVISIONING_STATUS_PREPARING_CSR_WAITING);
    case CertProvisioningWorkerState::kCanceled:
      return l10n_util::GetStringUTF16(
          IDS_SETTINGS_CERTIFICATE_MANAGER_PROVISIONING_STATUS_CANCELED);
    case CertProvisioningWorkerState::kReadyForNextOperation:
      return l10n_util::GetStringUTF16(
          IDS_SETTINGS_CERTIFICATE_MANAGER_PROVISIONING_STATUS_READY_FOR_NEXT_OPERATION);
    case CertProvisioningWorkerState::kAuthorizeInstructionReceived:
      return l10n_util::GetStringUTF16(
          IDS_SETTINGS_CERTIFICATE_MANAGER_PROVISIONING_STATUS_AUTHORIZE_INSTRUCTION_RECEIVED);
    case CertProvisioningWorkerState::kProofOfPossessionInstructionReceived:
      return l10n_util::GetStringUTF16(
          IDS_SETTINGS_CERTIFICATE_MANAGER_PROVISIONING_STATUS_PROOF_OF_POSSESSION_INSTRUCTION_RECEIVED);
    case CertProvisioningWorkerState::kImportCertificateInstructionReceived:
      return l10n_util::GetStringUTF16(
          IDS_SETTINGS_CERTIFICATE_MANAGER_PROVISIONING_STATUS_IMPORT_CERTIFICATE_INSTRUCTION_RECEIVED);
  }
  NOTREACHED();
}

// Returns the status message of the process.
// The status message is expanded by the failure message if the process failed
// and the error message is non-empty.
std::u16string MakeStatusMessage(
    bool did_fail,
    CertProvisioningWorkerState state,
    const std::optional<std::string>& failure_message) {
  if (!did_fail) {
    return StateToText(state);
  }
  std::u16string status_message =
      StateToText(CertProvisioningWorkerState::kFailed);
  if (failure_message.has_value()) {
    status_message += base::UTF8ToUTF16(": " + failure_message.value());
  }
  return status_message;
}

// Returns a localized representation of the last update time as a delay (e.g.
// "5 minutes ago".
std::u16string GetTimeSinceLastUpdate(base::Time last_update_time) {
  const base::Time now = base::Time::NowFromSystemTime();
  if (last_update_time.is_null() || last_update_time > now) {
    return std::u16string();
  }
  const base::TimeDelta elapsed_time = now - last_update_time;
  return ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_ELAPSED,
                                ui::TimeFormat::LENGTH_SHORT, elapsed_time);
}

std::u16string GetMessageFromBackendError(const BackendServerError& error) {
  std::u16string time_u16 = base::UTF8ToUTF16(base::TimeFormatHTTP(error.time));
  // FormatDeviceManagementStatus will return "Unknown error" if the value after
  // cast is not actually an existing enum value.
  return l10n_util::GetStringFUTF16(
      IDS_SETTINGS_CERTIFICATE_MANAGER_PROVISIONING_DMSERVER_ERROR_MESSAGE,
      policy::FormatDeviceManagementStatus(error.status), time_u16);
}

void AppendWorkerStatus(CertProvisioningScheduler* scheduler,
                        bool is_device_wide,
                        base::Value::List& all_processes) {
  if (!scheduler) {
    return;
  }

  const auto& worker_map = scheduler->GetWorkers();
  for (const auto& [profile_id, worker] : worker_map) {
    base::Value::Dict entry;
    entry.Set("processId", worker->GetProcessId());
    entry.Set("certProfileId", profile_id);
    entry.Set("certProfileName", worker->GetCertProfile().name);
    entry.Set("isDeviceWide", is_device_wide);
    entry.Set("timeSinceLastUpdate",
              GetTimeSinceLastUpdate(worker->GetLastUpdateTime()));
    const auto& backend_error = worker->GetLastBackendServerError();
    entry.Set("lastUnsuccessfulMessage",
              backend_error.has_value()
                  ? GetMessageFromBackendError(*backend_error)
                  : std::u16string());
    entry.Set("stateId", static_cast<int>(worker->GetState()));
    entry.Set("status", MakeStatusMessage(/*did_fail=*/false,
                                          worker->GetState(), std::nullopt));
    entry.Set("publicKey",
              x509_certificate_model::ProcessRawSubjectPublicKeyInfo(
                  worker->GetPublicKey()));

    all_processes.Append(std::move(entry));
  }

  const auto& failed_workers_map = scheduler->GetFailedCertProfileIds();
  for (const auto& [profile_id, worker] : failed_workers_map) {
    base::Value::Dict entry;
    entry.Set("processId", worker.process_id);
    entry.Set("certProfileId", profile_id);
    entry.Set("certProfileName", worker.cert_profile_name);
    entry.Set("isDeviceWide", is_device_wide);
    entry.Set("timeSinceLastUpdate",
              GetTimeSinceLastUpdate(worker.last_update_time));
    entry.Set("lastUnsuccessfulMessage", std::u16string());
    entry.Set("stateId", static_cast<int>(worker.state_before_failure));
    entry.Set("status",
              MakeStatusMessage(/*did_fail=*/true, worker.state_before_failure,
                                worker.failure_message));
    entry.Set("publicKey",
              x509_certificate_model::ProcessRawSubjectPublicKeyInfo(
                  worker.public_key));

    all_processes.Append(std::move(entry));
  }
}

}  // namespace

// static
std::unique_ptr<CertificateProvisioningUiHandler>
CertificateProvisioningUiHandler::CreateForProfile(Profile* user_profile) {
  return std::make_unique<CertificateProvisioningUiHandler>(
      GetUserScheduler(user_profile), GetDeviceScheduler(user_profile));
}

CertificateProvisioningUiHandler::CertificateProvisioningUiHandler(
    CertProvisioningScheduler* user_scheduler,
    CertProvisioningScheduler* device_scheduler)
    : user_scheduler_(user_scheduler), device_scheduler_(device_scheduler) {
  if (user_scheduler_) {
    user_subscription_ = user_scheduler_->AddObserver(
        base::BindRepeating(&CertificateProvisioningUiHandler::OnStateChanged,
                            weak_ptr_factory_.GetWeakPtr()));
  }
  if (device_scheduler_) {
    device_subscription_ = device_scheduler_->AddObserver(
        base::BindRepeating(&CertificateProvisioningUiHandler::OnStateChanged,
                            weak_ptr_factory_.GetWeakPtr()));
  }
}

CertificateProvisioningUiHandler::~CertificateProvisioningUiHandler() = default;

void CertificateProvisioningUiHandler::RegisterMessages() {
  // Passing base::Unretained(this) to
  // web_ui()->RegisterMessageCallback is fine because in chrome Web
  // UI, web_ui() has acquired ownership of |this| and maintains the life time
  // of |this| accordingly.
  web_ui()->RegisterMessageCallback(
      "refreshCertificateProvisioningProcessses",
      base::BindRepeating(&CertificateProvisioningUiHandler::
                              HandleRefreshCertificateProvisioningProcesses,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "triggerCertificateProvisioningProcessUpdate",
      base::BindRepeating(&CertificateProvisioningUiHandler::
                              HandleTriggerCertificateProvisioningProcessUpdate,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "triggerCertificateProvisioningProcessReset",
      base::BindRepeating(&CertificateProvisioningUiHandler::
                              HandleTriggerCertificateProvisioningProcessReset,
                          base::Unretained(this)));
}

void CertificateProvisioningUiHandler::OnStateChanged() {
  // If Javascript is not allowed yet, the UI will request a refresh during its
  // first message to the handler.
  if (!IsJavascriptAllowed()) {
    return;
  }

  RefreshCertificateProvisioningProcesses();
}

unsigned int
CertificateProvisioningUiHandler::ReadAndResetUiRefreshCountForTesting() {
  unsigned int value = ui_refresh_count_for_testing_;
  ui_refresh_count_for_testing_ = 0;
  return value;
}

void CertificateProvisioningUiHandler::
    HandleRefreshCertificateProvisioningProcesses(
        const base::Value::List& args) {
  CHECK_EQ(0U, args.size());
  AllowJavascript();
  RefreshCertificateProvisioningProcesses();
}

void CertificateProvisioningUiHandler::
    HandleTriggerCertificateProvisioningProcessUpdate(
        const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  const base::Value& cert_profile_id = args[0];
  if (!cert_profile_id.is_string()) {
    return;
  }

  if (user_scheduler_ &&
      user_scheduler_->UpdateOneWorker(cert_profile_id.GetString())) {
    return;
  }

  if (device_scheduler_ &&
      device_scheduler_->UpdateOneWorker(cert_profile_id.GetString())) {
    return;
  }

  if (user_scheduler_ || device_scheduler_) {
    LOG(ERROR) << "Updating cert_profile_id was not found. id:"
               << cert_profile_id.GetString()
               << " user_scheduler:" << bool(user_scheduler_)
               << " device_scheduler:" << bool(device_scheduler_);
  }
}

void CertificateProvisioningUiHandler::
    HandleTriggerCertificateProvisioningProcessReset(
        const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  const base::Value& cert_profile_id = args[0];
  if (!cert_profile_id.is_string()) {
    return;
  }

  if (user_scheduler_ &&
      user_scheduler_->ResetOneWorker(cert_profile_id.GetString())) {
    return;
  }
  if (device_scheduler_ &&
      device_scheduler_->ResetOneWorker(cert_profile_id.GetString())) {
    return;
  }

  if (user_scheduler_ || device_scheduler_) {
    LOG(ERROR) << "Resetting cert_profile_id was not found. id:"
               << cert_profile_id.GetString()
               << " user_scheduler:" << bool(user_scheduler_)
               << " device_scheduler:" << bool(device_scheduler_);
  }
}

void CertificateProvisioningUiHandler::
    RefreshCertificateProvisioningProcesses() {
  base::Value::List all_processes;
  AppendWorkerStatus(user_scheduler_, /*is_device_wide=*/false, all_processes);
  AppendWorkerStatus(device_scheduler_, /*is_device_wide=*/true, all_processes);

  ++ui_refresh_count_for_testing_;
  FireWebUIListener("certificate-provisioning-processes-changed",
                    all_processes);
}

}  // namespace chromeos::cert_provisioning
