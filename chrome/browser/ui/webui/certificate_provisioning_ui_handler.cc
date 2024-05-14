// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/webui/certificate_provisioning_ui_handler.h"

#include "base/check_is_test.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/i18n/time_formatting.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/common/net/x509_certificate_model.h"
#include "chrome/common/net/x509_certificate_model_nss.h"
#include "chrome/grit/generated_resources.h"
#include "components/policy/core/browser/cloud/message_util.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/profiles/profile.h"
#include "chromeos/lacros/lacros_service.h"
#include "chromeos/startup/browser_params_proxy.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/crosapi/cert_provisioning_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#endif  // #if BUILDFLAG(IS_CHROMEOS_ASH)

using crosapi::mojom::CertProvisioningProcessState;

namespace chromeos::cert_provisioning {

namespace {

crosapi::mojom::CertProvisioning* GetCertProvisioningInterface(
    Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  chromeos::LacrosService* service = chromeos::LacrosService::Get();
  if (!profile->IsMainProfile() || !service ||
      !service->IsAvailable<crosapi::mojom::CertProvisioning>()) {
    return nullptr;
  }
  return service->GetRemote<crosapi::mojom::CertProvisioning>().get();
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!ash::ProfileHelper::IsPrimaryProfile(profile)) {
    return nullptr;
  }
  return crosapi::CrosapiManager::Get()->crosapi_ash()->cert_provisioning_ash();
#endif  // #if BUILDFLAG(IS_CHROMEOS_ASH)
}

// Performs common crosapi validation. Returns void in case of success.
// Returns a string error message in case of a mismatch.
// |min_version| is the minimum version of the ash implementation
// of CertificateProvisioning necessary to support this
// operation.
base::expected<void, std::string> ValidateCrosapi(int min_version) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (BrowserParamsProxy::Get()->IsCrosapiDisabledForTesting()) {
    CHECK_IS_TEST();
    // Use the crosapi even though it's disabled - the test installs a fake.
    return {};
  }
  chromeos::LacrosService* service = chromeos::LacrosService::Get();
  int current_version =
      service->GetInterfaceVersion<crosapi::mojom::CertProvisioning>();
  if (current_version < min_version) {
    return base::unexpected(base::StringPrintf(
        "validate crosapi error: min_version:%i current_version:%i",
        min_version, current_version));
  }
#endif  // #if BUILDFLAG(IS_CHROME_LACROS)

  return {};
}

// Returns localized representation for the state of a certificate provisioning
// process.
std::u16string StateToText(CertProvisioningProcessState state) {
  switch (state) {
    case CertProvisioningProcessState ::kInitState:
      return l10n_util::GetStringUTF16(
          IDS_SETTINGS_CERTIFICATE_MANAGER_PROVISIONING_STATUS_PREPARING_CSR);
    case CertProvisioningProcessState ::kKeypairGenerated:
      return l10n_util::GetStringUTF16(
          IDS_SETTINGS_CERTIFICATE_MANAGER_PROVISIONING_STATUS_PREPARING_CSR_WAITING);
    case CertProvisioningProcessState::kStartCsrResponseReceived:
      // Intentional fall-through.
    case CertProvisioningProcessState::kVaChallengeFinished:
      // Intentional fall-through.
    case CertProvisioningProcessState::kKeyRegistered:
      // Intentional fall-through.
    case CertProvisioningProcessState::kKeypairMarked:
      return l10n_util::GetStringUTF16(
          IDS_SETTINGS_CERTIFICATE_MANAGER_PROVISIONING_STATUS_PREPARING_CSR);
    case CertProvisioningProcessState::kSignCsrFinished:
      return l10n_util::GetStringUTF16(
          IDS_SETTINGS_CERTIFICATE_MANAGER_PROVISIONING_STATUS_PREPARING_CSR_WAITING);
    case CertProvisioningProcessState::kFinishCsrResponseReceived:
      return l10n_util::GetStringUTF16(
          IDS_SETTINGS_CERTIFICATE_MANAGER_PROVISIONING_STATUS_WAITING_FOR_CA);
    case CertProvisioningProcessState::kSucceeded:
      return l10n_util::GetStringUTF16(
          IDS_SETTINGS_CERTIFICATE_MANAGER_PROVISIONING_STATUS_SUCCESS);
    case CertProvisioningProcessState::kFailed:
      return l10n_util::GetStringUTF16(
          IDS_SETTINGS_CERTIFICATE_MANAGER_PROVISIONING_STATUS_FAILURE);
    case CertProvisioningProcessState::kInconsistentDataError:
      return l10n_util::GetStringUTF16(
          IDS_SETTINGS_CERTIFICATE_MANAGER_PROVISIONING_STATUS_PREPARING_CSR_WAITING);
    case CertProvisioningProcessState::kCanceled:
      return l10n_util::GetStringUTF16(
          IDS_SETTINGS_CERTIFICATE_MANAGER_PROVISIONING_STATUS_CANCELED);
    case CertProvisioningProcessState::kReadyForNextOperation:
      return l10n_util::GetStringUTF16(
          IDS_SETTINGS_CERTIFICATE_MANAGER_PROVISIONING_STATUS_READY_FOR_NEXT_OPERATION);
    case CertProvisioningProcessState::kAuthorizeInstructionReceived:
      return l10n_util::GetStringUTF16(
          IDS_SETTINGS_CERTIFICATE_MANAGER_PROVISIONING_STATUS_AUTHORIZE_INSTRUCTION_RECEIVED);
    case CertProvisioningProcessState::kProofOfPossessionInstructionReceived:
      return l10n_util::GetStringUTF16(
          IDS_SETTINGS_CERTIFICATE_MANAGER_PROVISIONING_STATUS_PROOF_OF_POSSESSION_INSTRUCTION_RECEIVED);
    case CertProvisioningProcessState::kImportCertificateInstructionReceived:
      return l10n_util::GetStringUTF16(
          IDS_SETTINGS_CERTIFICATE_MANAGER_PROVISIONING_STATUS_IMPORT_CERTIFICATE_INSTRUCTION_RECEIVED);
  }
  NOTREACHED_IN_MIGRATION();
}

// Returns the status message of the process.
// The status message is expanded by the failure message if the process failed
// and the error message is non-empty.
std::u16string MakeStatusMessage(
    bool did_fail,
    CertProvisioningProcessState state,
    const std::optional<std::string>& failure_message) {
  if (!did_fail) {
    return StateToText(state);
  }
  std::u16string status_message =
      StateToText(CertProvisioningProcessState::kFailed);
  if (failure_message.has_value()) {
    status_message += base::UTF8ToUTF16(": " + failure_message.value());
  }
  return status_message;
}

// Returns a localized representation of the last update time as a delay (e.g.
// "5 minutes ago".
std::u16string GetTimeSinceLastUpdate(base::Time last_update_time) {
  const base::Time now = base::Time::NowFromSystemTime();
  if (last_update_time.is_null() || last_update_time > now)
    return std::u16string();
  const base::TimeDelta elapsed_time = now - last_update_time;
  return ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_ELAPSED,
                                ui::TimeFormat::LENGTH_SHORT, elapsed_time);
}

std::u16string GetMessageFromBackendError(
    const crosapi::mojom::CertProvisioningBackendServerErrorPtr& call_info) {
  if (!call_info)
    return std::u16string();

  std::u16string time_u16 =
      base::UTF8ToUTF16(base::TimeFormatHTTP(call_info->time));
  // FormatDeviceManagementStatus will return "Unknown error" if the value after
  // cast is not actually an existing enum value.
  auto status =
      static_cast<policy::DeviceManagementStatus>(call_info->status_code);
  return l10n_util::GetStringFUTF16(
      IDS_SETTINGS_CERTIFICATE_MANAGER_PROVISIONING_DMSERVER_ERROR_MESSAGE,
      policy::FormatDeviceManagementStatus(status), time_u16);
}

}  // namespace

// static
std::unique_ptr<CertificateProvisioningUiHandler>
CertificateProvisioningUiHandler::CreateForProfile(Profile* user_profile) {
  return std::make_unique<CertificateProvisioningUiHandler>(
      GetCertProvisioningInterface(user_profile));
}

CertificateProvisioningUiHandler::CertificateProvisioningUiHandler(
    crosapi::mojom::CertProvisioning* cert_provisioning_interface)
    : cert_provisioning_interface_(cert_provisioning_interface) {
  if (cert_provisioning_interface_) {
    cert_provisioning_interface->AddObserver(
        receiver_.BindNewPipeAndPassRemote());
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
  if (!IsJavascriptAllowed())
    return;

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

  if (cert_provisioning_interface_) {
    cert_provisioning_interface_->UpdateOneProcess(cert_profile_id.GetString());
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

  if (cert_provisioning_interface_) {
    base::expected<void, std::string> success = ValidateCrosapi(
        crosapi::mojom::CertProvisioning::kResetOneProcessMinVersion);
    if (success.has_value()) {
      cert_provisioning_interface_->ResetOneProcess(
          cert_profile_id.GetString());
    } else {
      LOG(ERROR) << "cert-prov cros_api validation error: " << success.error();
    }
  }
}

void CertificateProvisioningUiHandler::
    RefreshCertificateProvisioningProcesses() {
  if (cert_provisioning_interface_) {
    cert_provisioning_interface_->GetStatus(
        base::BindOnce(&CertificateProvisioningUiHandler::GotStatus,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void CertificateProvisioningUiHandler::GotStatus(
    std::vector<crosapi::mojom::CertProvisioningProcessStatusPtr> status) {
  base::Value::List all_processes;

  for (auto& process : status) {
    base::Value::Dict entry;
    entry.Set("certProfileId", std::move(process->cert_profile_id));
    entry.Set("certProfileName", std::move(process->cert_profile_name));
    entry.Set("isDeviceWide", process->is_device_wide);
    entry.Set("timeSinceLastUpdate",
              GetTimeSinceLastUpdate(process->last_update_time));
    entry.Set("lastUnsuccessfulMessage",
              GetMessageFromBackendError(process->last_backend_server_error));
    entry.Set("stateId", static_cast<int>(process->state));
    entry.Set("status", MakeStatusMessage(process->did_fail, process->state,
                                          process->failure_message));
    entry.Set("publicKey",
              x509_certificate_model::ProcessRawSubjectPublicKeyInfo(
                  process->public_key));

    all_processes.Append(std::move(entry));
  }

  ++ui_refresh_count_for_testing_;
  FireWebUIListener("certificate-provisioning-processes-changed",
                    all_processes);
}

}  // namespace chromeos::cert_provisioning
