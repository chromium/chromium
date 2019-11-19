// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/crostini_handler.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/chromeos/guest_os/guest_os_share_path.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_thread.h"

namespace chromeos {
namespace settings {

namespace {

// These values are used for metrics and should not change.
enum class CrostiniSettingsEvent {
  kEnableAdbSideloading = 0,
  kDisableAdbSideloading = 1,
  kMaxValue = kDisableAdbSideloading,
};

void LogEvent(CrostiniSettingsEvent action) {
  base::UmaHistogramEnumeration("Crostini.SettingsEvent", action);
}

}  // namespace

CrostiniHandler::CrostiniHandler(Profile* profile) : profile_(profile) {}

CrostiniHandler::~CrostiniHandler() {
  DisallowJavascript();
}

void CrostiniHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "requestCrostiniInstallerView",
      base::BindRepeating(&CrostiniHandler::HandleRequestCrostiniInstallerView,
                          weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "requestRemoveCrostini",
      base::BindRepeating(&CrostiniHandler::HandleRequestRemoveCrostini,
                          weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "getCrostiniSharedPathsDisplayText",
      base::BindRepeating(
          &CrostiniHandler::HandleGetCrostiniSharedPathsDisplayText,
          weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "removeCrostiniSharedPath",
      base::BindRepeating(&CrostiniHandler::HandleRemoveCrostiniSharedPath,
                          weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "getCrostiniSharedUsbDevices",
      base::BindRepeating(&CrostiniHandler::HandleGetCrostiniSharedUsbDevices,
                          weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "setCrostiniUsbDeviceShared",
      base::BindRepeating(&CrostiniHandler::HandleSetCrostiniUsbDeviceShared,
                          weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "exportCrostiniContainer",
      base::BindRepeating(&CrostiniHandler::HandleExportCrostiniContainer,
                          weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "importCrostiniContainer",
      base::BindRepeating(&CrostiniHandler::HandleImportCrostiniContainer,
                          weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "requestCrostiniInstallerStatus",
      base::BindRepeating(
          &CrostiniHandler::HandleCrostiniInstallerStatusRequest,
          weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "requestCrostiniExportImportOperationStatus",
      base::BindRepeating(
          &CrostiniHandler::HandleCrostiniExportImportOperationStatusRequest,
          weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "requestArcAdbSideloadStatus",
      base::BindRepeating(&CrostiniHandler::HandleQueryArcAdbRequest,
                          weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "enableArcAdbSideload",
      base::BindRepeating(&CrostiniHandler::HandleEnableArcAdbRequest,
                          weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "disableArcAdbSideload",
      base::BindRepeating(&CrostiniHandler::HandleDisableArcAdbRequest,
                          weak_ptr_factory_.GetWeakPtr()));
}

void CrostiniHandler::OnJavascriptAllowed() {
  crostini::CrostiniManager::GetForProfile(profile_)
      ->AddInstallerViewStatusObserver(this);
  if (chromeos::CrosUsbDetector::Get()) {
    chromeos::CrosUsbDetector::Get()->AddUsbDeviceObserver(this);
  }
  crostini::CrostiniExportImport::GetForProfile(profile_)->AddObserver(this);
}

void CrostiniHandler::OnJavascriptDisallowed() {
  if (crostini::CrostiniManager::GetForProfile(profile_)
          ->HasInstallerViewStatusObserver(this)) {
    crostini::CrostiniManager::GetForProfile(profile_)
        ->RemoveInstallerViewStatusObserver(this);
  }
  if (chromeos::CrosUsbDetector::Get()) {
    chromeos::CrosUsbDetector::Get()->RemoveUsbDeviceObserver(this);
  }
  crostini::CrostiniExportImport::GetForProfile(profile_)->RemoveObserver(this);
}

void CrostiniHandler::HandleRequestCrostiniInstallerView(
    const base::ListValue* args) {
  AllowJavascript();
  ShowCrostiniInstallerView(Profile::FromWebUI(web_ui()),
                            crostini::CrostiniUISurface::kSettings);
}

void CrostiniHandler::HandleRequestRemoveCrostini(const base::ListValue* args) {
  AllowJavascript();
  ShowCrostiniUninstallerView(Profile::FromWebUI(web_ui()),
                              crostini::CrostiniUISurface::kSettings);
}

void CrostiniHandler::HandleGetCrostiniSharedPathsDisplayText(
    const base::ListValue* args) {
  AllowJavascript();
  CHECK_EQ(2U, args->GetSize());
  std::string callback_id;
  const base::ListValue* paths;
  CHECK(args->GetString(0, &callback_id));
  CHECK(args->GetList(1, &paths));

  base::ListValue texts;
  for (auto it = paths->begin(); it != paths->end(); ++it) {
    texts.AppendString(file_manager::util::GetPathDisplayTextForSettings(
        profile_, it->GetString()));
  }
  ResolveJavascriptCallback(base::Value(callback_id), texts);
}

void CrostiniHandler::HandleRemoveCrostiniSharedPath(
    const base::ListValue* args) {
  CHECK_EQ(2U, args->GetSize());
  std::string vm_name;
  CHECK(args->GetString(0, &vm_name));
  std::string path;
  CHECK(args->GetString(1, &path));

  guest_os::GuestOsSharePath::GetForProfile(profile_)->UnsharePath(
      vm_name, base::FilePath(path),
      /*unpersist=*/true,
      base::BindOnce(
          [](const std::string& path, bool result,
             const std::string& failure_reason) {
            if (!result) {
              LOG(ERROR) << "Error unsharing " << path << ": "
                         << failure_reason;
            }
          },
          path));
}

namespace {
base::ListValue UsbDevicesToListValue(
    const std::vector<CrosUsbDeviceInfo> shared_usbs) {
  base::ListValue usb_devices_list;
  for (auto device : shared_usbs) {
    base::Value device_info(base::Value::Type::DICTIONARY);
    device_info.SetKey("guid", base::Value(device.guid));
    device_info.SetKey("label", base::Value(device.label));
    const bool shared_in_crostini =
        device.vm_sharing_info[crostini::kCrostiniDefaultVmName].shared;
    device_info.SetKey("shared", base::Value(shared_in_crostini));
    usb_devices_list.Append(std::move(device_info));
  }
  return usb_devices_list;
}
}  // namespace

void CrostiniHandler::HandleGetCrostiniSharedUsbDevices(
    const base::ListValue* args) {
  AllowJavascript();
  CHECK_EQ(1U, args->GetSize());

  std::string callback_id = args->GetList()[0].GetString();

  chromeos::CrosUsbDetector* detector = chromeos::CrosUsbDetector::Get();
  if (!detector) {
    ResolveJavascriptCallback(base::Value(callback_id), base::ListValue());
    return;
  }

  ResolveJavascriptCallback(
      base::Value(callback_id),
      UsbDevicesToListValue(detector->GetDevicesSharableWithCrostini()));
}

void CrostiniHandler::HandleSetCrostiniUsbDeviceShared(
    const base::ListValue* args) {
  CHECK_EQ(2U, args->GetSize());
  const auto& args_list = args->GetList();
  std::string guid = args_list[0].GetString();
  bool shared = args_list[1].GetBool();

  chromeos::CrosUsbDetector* detector = chromeos::CrosUsbDetector::Get();
  if (!detector)
    return;

  if (shared) {
    detector->AttachUsbDeviceToVm(crostini::kCrostiniDefaultVmName, guid,
                                  base::DoNothing());
    return;
  }
  detector->DetachUsbDeviceFromVm(crostini::kCrostiniDefaultVmName, guid,
                                  base::DoNothing());
}

void CrostiniHandler::OnUsbDevicesChanged() {
  chromeos::CrosUsbDetector* detector = chromeos::CrosUsbDetector::Get();
  DCHECK(detector);  // This callback is called by the detector.
  FireWebUIListener(
      "crostini-shared-usb-devices-changed",
      UsbDevicesToListValue(detector->GetDevicesSharableWithCrostini()));
}

void CrostiniHandler::HandleExportCrostiniContainer(
    const base::ListValue* args) {
  CHECK_EQ(0U, args->GetSize());
  crostini::CrostiniExportImport::GetForProfile(profile_)->ExportContainer(
      web_ui()->GetWebContents());
}

void CrostiniHandler::HandleImportCrostiniContainer(
    const base::ListValue* args) {
  CHECK_EQ(0U, args->GetSize());
  crostini::CrostiniExportImport::GetForProfile(profile_)->ImportContainer(
      web_ui()->GetWebContents());
}

void CrostiniHandler::HandleCrostiniInstallerStatusRequest(
    const base::ListValue* args) {
  AllowJavascript();
  CHECK_EQ(0U, args->GetSize());
  bool status = crostini::CrostiniManager::GetForProfile(profile_)
                    ->GetInstallerViewStatus();
  OnCrostiniInstallerViewStatusChanged(status);
}

void CrostiniHandler::HandleCrostiniExportImportOperationStatusRequest(
    const base::ListValue* args) {
  AllowJavascript();
  CHECK_EQ(0U, args->GetSize());
  bool in_progress = crostini::CrostiniExportImport::GetForProfile(profile_)
                         ->GetExportImportOperationStatus();
  OnCrostiniExportImportOperationStatusChanged(in_progress);
}

void CrostiniHandler::OnCrostiniInstallerViewStatusChanged(bool status) {
  // It's technically possible for this to be called before Javascript is
  // enabled, in which case we must not call FireWebUIListener
  if (IsJavascriptAllowed()) {
    // Other side listens with cr.addWebUIListener
    FireWebUIListener("crostini-installer-status-changed", base::Value(status));
  }
}

void CrostiniHandler::OnCrostiniExportImportOperationStatusChanged(
    bool in_progress) {
  // Other side listens with cr.addWebUIListener
  FireWebUIListener("crostini-export-import-operation-status-changed",
                    base::Value(in_progress));
}

void CrostiniHandler::HandleQueryArcAdbRequest(const base::ListValue* args) {
  AllowJavascript();
  CHECK_EQ(0U, args->GetSize());

  chromeos::SessionManagerClient* client =
      chromeos::SessionManagerClient::Get();
  client->QueryAdbSideload(base::Bind(&CrostiniHandler::OnQueryAdbSideload,
                                      weak_ptr_factory_.GetWeakPtr()));
}

void CrostiniHandler::OnQueryAdbSideload(
    SessionManagerClient::AdbSideloadResponseCode response_code,
    bool enabled) {
  if (response_code != SessionManagerClient::AdbSideloadResponseCode::SUCCESS) {
    LOG(ERROR) << "Failed to query adb sideload status";
    enabled = false;
  }
  bool need_powerwash =
      response_code ==
      SessionManagerClient::AdbSideloadResponseCode::NEED_POWERWASH;
  // Other side listens with cr.addWebUIListener
  FireWebUIListener("crostini-arc-adb-sideload-status-changed",
                    base::Value(enabled), base::Value(need_powerwash));
}

void CrostiniHandler::HandleEnableArcAdbRequest(const base::ListValue* args) {
  CHECK_EQ(0U, args->GetSize());
  if (!CheckEligibilityToChangeArcAdbSideloading())
    return;

  LogEvent(CrostiniSettingsEvent::kEnableAdbSideloading);

  PrefService* prefs = g_browser_process->local_state();
  prefs->SetBoolean(prefs::kEnableAdbSideloadingRequested, true);
  prefs->CommitPendingWrite();

  chrome::AttemptRelaunch();
}

void CrostiniHandler::HandleDisableArcAdbRequest(const base::ListValue* args) {
  CHECK_EQ(0U, args->GetSize());
  if (!CheckEligibilityToChangeArcAdbSideloading())
    return;

  LogEvent(CrostiniSettingsEvent::kDisableAdbSideloading);

  PrefService* prefs = g_browser_process->local_state();
  prefs->SetBoolean(prefs::kFactoryResetRequested, true);
  prefs->CommitPendingWrite();

  chromeos::PowerManagerClient::Get()->RequestRestart(
      power_manager::REQUEST_RESTART_FOR_USER, "disable adb sideloading");
}

bool CrostiniHandler::CheckEligibilityToChangeArcAdbSideloading() const {
  if (!chromeos::ProfileHelper::IsOwnerProfile(profile_)) {
    DVLOG(1) << "Only the owner can change adb sideloading status";
    return false;
  }

  if (user_manager::UserManager::Get()->IsLoggedInAsChildUser()) {
    DVLOG(1) << "adb sideloading is currently unsupported for child account";
    return false;
  }

  if (profile_->GetProfilePolicyConnector()->IsManaged()) {
    DVLOG(1) << "adb sideloading is currently unsupported for managed user";
    return false;
  }

  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  if (connector->IsEnterpriseManaged()) {
    DVLOG(1) << "adb sideloading is currently unsupported on managed device";
    return false;
  }
  return true;
}

}  // namespace settings
}  // namespace chromeos
