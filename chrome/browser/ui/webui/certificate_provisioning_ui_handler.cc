// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/browser/ui/webui/certificate_provisioning_ui_handler.h"

#include "base/bind.h"
#include "base/containers/span.h"
#include "base/strings/string16.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/cert_provisioning/cert_provisioning_common.h"
#include "chrome/browser/chromeos/cert_provisioning/cert_provisioning_scheduler_user_service.h"
#include "chrome/browser/chromeos/cert_provisioning/cert_provisioning_worker.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/common/net/x509_certificate_model_nss.h"
#include "chrome/grit/generated_resources.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"

namespace chromeos {
namespace cert_provisioning {

namespace {

// Returns the per-user CertProvisioningScheduler for |user_profile|, if it has
// any.
CertProvisioningScheduler* GetCertProvisioningSchedulerForUser(
    Profile* user_profile) {
  CertProvisioningSchedulerUserService* user_service =
      CertProvisioningSchedulerUserServiceFactory::GetForProfile(user_profile);
  if (!user_service)
    return nullptr;
  return user_service->scheduler();
}

// Returns the per-device CertProvisioningScheduler, if it exists. No
// affiliation check is done here.
CertProvisioningScheduler* GetCertProvisioningSchedulerForDevice() {
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  return connector->GetDeviceCertProvisioningScheduler();
}

// Returns localized representation for the state of a certificate provisioning
// process.
base::string16 GetProvisioningProcessStatus(CertProvisioningWorkerState state) {
  using CertProvisioningWorkerState = CertProvisioningWorkerState;
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
  }
  NOTREACHED();
}

// Returns a localized representation of the last update time as a delay (e.g.
// "5 minutes ago".
base::string16 GetTimeSinceLastUpdate(base::Time last_update_time) {
  const base::Time now = base::Time::NowFromSystemTime();
  if (last_update_time.is_null() || last_update_time > now)
    return base::string16();
  const base::TimeDelta elapsed_time = now - last_update_time;
  return ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_ELAPSED,
                                ui::TimeFormat::LENGTH_SHORT, elapsed_time);
}

base::Value CreateProvisioningProcessEntry(
    const std::string& cert_profile_id,
    bool is_device_wide,
    CertProvisioningWorkerState state,
    base::Time time_since_last_update,
    const std::string& public_key_spki_der) {
  base::Value entry(base::Value::Type::DICTIONARY);
  entry.SetStringKey("certProfileId", cert_profile_id);
  entry.SetBoolKey("isDeviceWide", is_device_wide);
  entry.SetStringKey("status", GetProvisioningProcessStatus(state));
  entry.SetIntKey("stateId", static_cast<int>(state));
  entry.SetStringKey("timeSinceLastUpdate",
                     GetTimeSinceLastUpdate(time_since_last_update));

  auto spki_der_bytes = base::as_bytes(base::make_span(public_key_spki_der));
  entry.SetStringKey(
      "publicKey",
      x509_certificate_model::ProcessRawSubjectPublicKeyInfo(spki_der_bytes));

  return entry;
}

// Collects information about certificate provisioning processes from
// |cert_provisioning_scheduler| and appends them to |list_to_append_to|.
void CollectProvisioningProcesses(
    base::Value* list_to_append_to,
    CertProvisioningScheduler* cert_provisioning_scheduler,
    bool is_device_wide) {
  for (const auto& worker_entry : cert_provisioning_scheduler->GetWorkers()) {
    CertProvisioningWorker* worker = worker_entry.second.get();
    list_to_append_to->Append(CreateProvisioningProcessEntry(
        worker_entry.first, is_device_wide, worker->GetState(),
        worker->GetLastUpdateTime(), worker->GetPublicKey()));
  }
  for (const auto& failed_worker_entry :
       cert_provisioning_scheduler->GetFailedCertProfileIds()) {
    const FailedWorkerInfo& worker = failed_worker_entry.second;
    list_to_append_to->Append(CreateProvisioningProcessEntry(
        failed_worker_entry.first, is_device_wide,
        CertProvisioningWorkerState::kFailed, worker.last_update_time,
        worker.public_key));
  }
}

}  // namespace

// static
std::unique_ptr<CertificateProvisioningUiHandler>
CertificateProvisioningUiHandler::CreateForProfile(Profile* user_profile) {
  return std::make_unique<CertificateProvisioningUiHandler>(
      user_profile, GetCertProvisioningSchedulerForUser(user_profile),
      GetCertProvisioningSchedulerForDevice());
}

CertificateProvisioningUiHandler::CertificateProvisioningUiHandler(
    Profile* user_profile,
    CertProvisioningScheduler* scheduler_for_user,
    CertProvisioningScheduler* scheduler_for_device)
    : scheduler_for_user_(scheduler_for_user),
      scheduler_for_device_(ShouldUseDeviceWideProcesses(user_profile)
                                ? scheduler_for_device
                                : nullptr) {
  if (scheduler_for_user_)
    observed_schedulers_.Add(scheduler_for_user_);
  if (scheduler_for_device_)
    observed_schedulers_.Add(scheduler_for_device_);
}

CertificateProvisioningUiHandler::~CertificateProvisioningUiHandler() = default;

void CertificateProvisioningUiHandler::RegisterMessages() {
  // Passing base::Unretained(this) to web_ui()->RegisterMessageCallback is fine
  // because in chrome Web UI, web_ui() has acquired ownership of |this| and
  // maintains the life time of |this| accordingly.
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
}

void CertificateProvisioningUiHandler::OnVisibleStateChanged() {
  // If Javascript is not allowed yet, we don't need to cache the update,
  // because the UI will request a refresh during its first message to the
  // handler.
  if (!IsJavascriptAllowed())
    return;
  if (hold_back_updates_timer_.IsRunning()) {
    update_after_hold_back_ = true;
    return;
  }
  constexpr base::TimeDelta kTimeToHoldBackUpdates =
      base::TimeDelta::FromMilliseconds(300);
  hold_back_updates_timer_.Start(
      FROM_HERE, kTimeToHoldBackUpdates,
      base::BindOnce(
          &CertificateProvisioningUiHandler::OnHoldBackUpdatesTimerExpired,
          weak_ptr_factory_.GetWeakPtr()));

  RefreshCertificateProvisioningProcesses();
}

unsigned int
CertificateProvisioningUiHandler::ReadAndResetUiRefreshCountForTesting() {
  unsigned int value = ui_refresh_count_for_testing_;
  ui_refresh_count_for_testing_ = 0;
  return value;
}

void CertificateProvisioningUiHandler::
    HandleRefreshCertificateProvisioningProcesses(const base::ListValue* args) {
  CHECK_EQ(0U, args->GetSize());
  AllowJavascript();
  RefreshCertificateProvisioningProcesses();
}

void CertificateProvisioningUiHandler::
    HandleTriggerCertificateProvisioningProcessUpdate(
        const base::ListValue* args) {
  CHECK_EQ(2U, args->GetSize());
  if (!args->is_list())
    return;
  const base::Value& cert_profile_id = args->GetList()[0];
  if (!cert_profile_id.is_string())
    return;
  const base::Value& device_wide = args->GetList()[1];
  if (!device_wide.is_bool())
    return;

  if (device_wide.GetBool() && !scheduler_for_device_)
    return;

  CertProvisioningScheduler* scheduler =
      device_wide.GetBool() ? scheduler_for_device_ : scheduler_for_user_;
  if (!scheduler)
    return;

  scheduler->UpdateOneCert(cert_profile_id.GetString());
}

void CertificateProvisioningUiHandler::
    RefreshCertificateProvisioningProcesses() {
  base::ListValue all_processes;
  if (scheduler_for_user_) {
    CollectProvisioningProcesses(&all_processes, scheduler_for_user_,
                                 /*is_device_wide=*/false);
  }

  if (scheduler_for_device_) {
    CollectProvisioningProcesses(&all_processes, scheduler_for_device_,
                                 /*is_device_wide=*/true);
  }

  ++ui_refresh_count_for_testing_;
  FireWebUIListener("certificate-provisioning-processes-changed",
                    std::move(all_processes));
}

void CertificateProvisioningUiHandler::OnHoldBackUpdatesTimerExpired() {
  if (update_after_hold_back_) {
    update_after_hold_back_ = false;
    RefreshCertificateProvisioningProcesses();
  }
}

// static
bool CertificateProvisioningUiHandler::ShouldUseDeviceWideProcesses(
    Profile* user_profile) {
  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(user_profile);
  return user && user->IsAffiliated();
}

}  // namespace cert_provisioning
}  // namespace chromeos
