// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/crostini_handler.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_forward.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/crostini/crostini_disk.h"
#include "chrome/browser/chromeos/crostini/crostini_features.h"
#include "chrome/browser/chromeos/crostini/crostini_installer.h"
#include "chrome/browser/chromeos/crostini/crostini_port_forwarder.h"
#include "chrome/browser/chromeos/crostini/crostini_pref_names.h"
#include "chrome/browser/chromeos/crostini/crostini_types.mojom.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/chromeos/guest_os/guest_os_share_path.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chromeos/crostini_upgrader/crostini_upgrader_dialog.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_thread.h"
#include "ui/display/screen.h"

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
      "notifyCrostiniSharedUsbDevicesPageReady",
      base::BindRepeating(
          &CrostiniHandler::HandleNotifyCrostiniSharedUsbDevicesPageReady,
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
      "getCanChangeArcAdbSideloading",
      base::BindRepeating(
          &CrostiniHandler::HandleCanChangeArcAdbSideloadingRequest,
          weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "enableArcAdbSideload",
      base::BindRepeating(&CrostiniHandler::HandleEnableArcAdbRequest,
                          weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "disableArcAdbSideload",
      base::BindRepeating(&CrostiniHandler::HandleDisableArcAdbRequest,
                          weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "requestCrostiniContainerUpgradeView",
      base::BindRepeating(&CrostiniHandler::HandleRequestContainerUpgradeView,
                          weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "requestCrostiniUpgraderDialogStatus",
      base::BindRepeating(
          &CrostiniHandler::HandleCrostiniUpgraderDialogStatusRequest,
          weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "requestCrostiniContainerUpgradeAvailable",
      base::BindRepeating(
          &CrostiniHandler::HandleCrostiniContainerUpgradeAvailableRequest,
          weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "addCrostiniPortForward",
      base::BindRepeating(&CrostiniHandler::HandleAddCrostiniPortForward,
                          weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "getCrostiniDiskInfo",
      base::BindRepeating(&CrostiniHandler::HandleGetCrostiniDiskInfo,
                          weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "resizeCrostiniDisk",
      base::BindRepeating(&CrostiniHandler::HandleResizeCrostiniDisk,
                          weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "checkCrostiniMicSharingStatus",
      base::BindRepeating(&CrostiniHandler::HandleCheckCrostiniMicSharingStatus,
                          weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "removeCrostiniPortForward",
      base::BindRepeating(&CrostiniHandler::HandleRemoveCrostiniPortForward,
                          weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "removeAllCrostiniPortForwards",
      base::BindRepeating(&CrostiniHandler::HandleRemoveAllCrostiniPortForwards,
                          weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "activateCrostiniPortForward",
      base::BindRepeating(&CrostiniHandler::HandleActivateCrostiniPortForward,
                          weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "deactivateCrostiniPortForward",
      base::BindRepeating(&CrostiniHandler::HandleDeactivateCrostiniPortForward,
                          weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "getCrostiniActivePorts",
      base::BindRepeating(&CrostiniHandler::HandleGetCrostiniActivePorts,
                          weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "checkCrostiniIsRunning",
      base::BindRepeating(&CrostiniHandler::HandleCheckCrostiniIsRunning,
                          weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "shutdownCrostini",
      base::BindRepeating(&CrostiniHandler::HandleShutdownCrostini,
                          weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "setCrostiniMicSharingEnabled",
      base::BindRepeating(&CrostiniHandler::HandleSetCrostiniMicSharingEnabled,
                          weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "getCrostiniMicSharingEnabled",
      base::BindRepeating(&CrostiniHandler::HandleGetCrostiniMicSharingEnabled,
                          weak_ptr_factory_.GetWeakPtr()));
}

void CrostiniHandler::OnJavascriptAllowed() {
  auto* crostini_manager = crostini::CrostiniManager::GetForProfile(profile_);
  crostini_manager->AddCrostiniDialogStatusObserver(this);
  crostini_manager->AddCrostiniContainerPropertiesObserver(this);
  crostini_manager->AddContainerStartedObserver(this);
  crostini_manager->AddContainerShutdownObserver(this);
  crostini_manager->AddCrostiniMicSharingEnabledObserver(this);
  if (chromeos::CrosUsbDetector::Get()) {
    chromeos::CrosUsbDetector::Get()->AddUsbDeviceObserver(this);
  }
  crostini::CrostiniExportImport::GetForProfile(profile_)->AddObserver(this);
  crostini::CrostiniPortForwarder::GetForProfile(profile_)->AddObserver(this);

  // Observe ADB sideloading device policy and react to its changes
  adb_sideloading_device_policy_subscription_ =
      chromeos::CrosSettings::Get()->AddSettingsObserver(
          chromeos::kDeviceCrostiniArcAdbSideloadingAllowed,
          base::BindRepeating(&CrostiniHandler::FetchCanChangeAdbSideloading,
                              weak_ptr_factory_.GetWeakPtr()));

  // Observe ADB sideloading user policy and react to its changes
  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      crostini::prefs::kCrostiniArcAdbSideloadingUserPref,
      base::BindRepeating(&CrostiniHandler::FetchCanChangeAdbSideloading,
                          weak_ptr_factory_.GetWeakPtr()));
}

void CrostiniHandler::OnJavascriptDisallowed() {
  auto* crostini_manager = crostini::CrostiniManager::GetForProfile(profile_);
  crostini_manager->RemoveCrostiniDialogStatusObserver(this);
  crostini_manager->RemoveCrostiniContainerPropertiesObserver(this);
  crostini_manager->RemoveContainerStartedObserver(this);
  crostini_manager->RemoveContainerShutdownObserver(this);
  crostini_manager->RemoveCrostiniMicSharingEnabledObserver(this);
  if (chromeos::CrosUsbDetector::Get()) {
    chromeos::CrosUsbDetector::Get()->RemoveUsbDeviceObserver(this);
  }
  crostini::CrostiniExportImport::GetForProfile(profile_)->RemoveObserver(this);
  crostini::CrostiniPortForwarder::GetForProfile(profile_)->RemoveObserver(
      this);

  adb_sideloading_device_policy_subscription_.reset();
  pref_change_registrar_.RemoveAll();
}

void CrostiniHandler::HandleRequestCrostiniInstallerView(
    const base::ListValue* args) {
  AllowJavascript();
  crostini::CrostiniInstaller::GetForProfile(Profile::FromWebUI(web_ui()))
      ->ShowDialog(crostini::CrostiniUISurface::kSettings);
}

void CrostiniHandler::HandleRequestRemoveCrostini(const base::ListValue* args) {
  AllowJavascript();
  ShowCrostiniUninstallerView(Profile::FromWebUI(web_ui()),
                              crostini::CrostiniUISurface::kSettings);
}

void CrostiniHandler::HandleGetCrostiniSharedPathsDisplayText(
    const base::ListValue* args) {
  AllowJavascript();
  CHECK_EQ(2U, args->GetList().size());
  std::string callback_id = args->GetList()[0].GetString();
  base::Value::ConstListView paths = args->GetList()[1].GetList();

  base::ListValue texts;
  for (size_t i = 0; i < paths.size(); ++i) {
    texts.AppendString(file_manager::util::GetPathDisplayTextForSettings(
        profile_, paths[i].GetString()));
  }
  ResolveJavascriptCallback(base::Value(callback_id), texts);
}

void CrostiniHandler::HandleRemoveCrostiniSharedPath(
    const base::ListValue* args) {
  AllowJavascript();
  CHECK_EQ(3U, args->GetList().size());
  std::string callback_id = args->GetList()[0].GetString();
  std::string vm_name = args->GetList()[1].GetString();
  std::string path = args->GetList()[2].GetString();

  guest_os::GuestOsSharePath::GetForProfile(profile_)->UnsharePath(
      vm_name, base::FilePath(path),
      /*unpersist=*/true,
      base::BindOnce(&CrostiniHandler::OnCrostiniSharedPathRemoved,
                     weak_ptr_factory_.GetWeakPtr(), callback_id, path));
}

void CrostiniHandler::OnCrostiniSharedPathRemoved(
    const std::string& callback_id,
    const std::string& path,
    bool result,
    const std::string& failure_reason) {
  if (!result) {
    LOG(ERROR) << "Error unsharing " << path << ": " << failure_reason;
  }
  ResolveJavascriptCallback(base::Value(callback_id), base::Value(result));
}

namespace {
base::ListValue UsbDevicesToListValue(
    const std::vector<CrosUsbDeviceInfo> shared_usbs) {
  base::ListValue usb_devices_list;
  for (auto& device : shared_usbs) {
    base::Value device_info(base::Value::Type::DICTIONARY);
    device_info.SetStringKey("guid", device.guid);
    device_info.SetStringKey("label", device.label);
    bool shared = device.shared_vm_name == crostini::kCrostiniDefaultVmName;
    device_info.SetBoolKey("shared", shared);
    device_info.SetBoolKey("shareWillReassign",
                           device.shared_vm_name && !shared);
    usb_devices_list.Append(std::move(device_info));
  }
  return usb_devices_list;
}

base::Value CrostiniDiskInfoToValue(
    std::unique_ptr<crostini::CrostiniDiskInfo> disk_info) {
  base::Value disk_value(base::Value::Type::DICTIONARY);
  if (!disk_info) {
    disk_value.SetBoolKey("succeeded", false);
    return disk_value;
  }
  disk_value.SetBoolKey("succeeded", true);
  disk_value.SetBoolKey("canResize", disk_info->can_resize);
  disk_value.SetBoolKey("isUserChosenSize", disk_info->is_user_chosen_size);
  disk_value.SetBoolKey("isLowSpaceAvailable",
                        disk_info->is_low_space_available);
  disk_value.SetIntKey("defaultIndex", disk_info->default_index);
  base::Value ticks(base::Value::Type::LIST);
  for (const auto& tick : disk_info->ticks) {
    base::Value t(base::Value::Type::DICTIONARY);
    t.SetDoubleKey("value", static_cast<double>(tick->value));
    t.SetStringKey("ariaValue", tick->aria_value);
    t.SetStringKey("label", tick->label);
    ticks.Append(std::move(t));
  }
  disk_value.SetKey("ticks", std::move(ticks));
  return disk_value;
}
}  // namespace

void CrostiniHandler::HandleNotifyCrostiniSharedUsbDevicesPageReady(
    const base::ListValue* args) {
  AllowJavascript();
  OnUsbDevicesChanged();
}

void CrostiniHandler::HandleSetCrostiniUsbDeviceShared(
    const base::ListValue* args) {
  CHECK_EQ(2U, args->GetList().size());
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
  CHECK_EQ(0U, args->GetList().size());
  crostini::CrostiniExportImport::GetForProfile(profile_)->ExportContainer(
      web_ui()->GetWebContents());
}

void CrostiniHandler::HandleImportCrostiniContainer(
    const base::ListValue* args) {
  CHECK_EQ(0U, args->GetList().size());
  crostini::CrostiniExportImport::GetForProfile(profile_)->ImportContainer(
      web_ui()->GetWebContents());
}

void CrostiniHandler::HandleCrostiniInstallerStatusRequest(
    const base::ListValue* args) {
  AllowJavascript();
  CHECK_EQ(0U, args->GetList().size());
  bool status = crostini::CrostiniManager::GetForProfile(profile_)
                    ->GetCrostiniDialogStatus(crostini::DialogType::INSTALLER);
  OnCrostiniDialogStatusChanged(crostini::DialogType::INSTALLER, status);
}

void CrostiniHandler::HandleCrostiniExportImportOperationStatusRequest(
    const base::ListValue* args) {
  AllowJavascript();
  CHECK_EQ(0U, args->GetList().size());
  bool in_progress = crostini::CrostiniExportImport::GetForProfile(profile_)
                         ->GetExportImportOperationStatus();
  OnCrostiniExportImportOperationStatusChanged(in_progress);
}

void CrostiniHandler::OnCrostiniDialogStatusChanged(
    crostini::DialogType dialog_type,
    bool status) {
  // It's technically possible for this to be called before Javascript is
  // enabled, in which case we must not call FireWebUIListener
  if (IsJavascriptAllowed()) {
    // Other side listens with cr.addWebUIListener
    switch (dialog_type) {
      case crostini::DialogType::INSTALLER:
        FireWebUIListener("crostini-installer-status-changed",
                          base::Value(status));
        break;
      case crostini::DialogType::UPGRADER:
        FireWebUIListener("crostini-upgrader-status-changed",
                          base::Value(status));
        break;
      case crostini::DialogType::REMOVER:
        FireWebUIListener("crostini-remover-status-changed",
                          base::Value(status));
        break;
      default:
        NOTREACHED();
        break;
    }
  }
}

void CrostiniHandler::OnContainerOsReleaseChanged(
    const crostini::ContainerId& container_id,
    bool can_upgrade) {
  if (crostini::CrostiniFeatures::Get()->IsContainerUpgradeUIAllowed(
          profile_) &&
      container_id == crostini::DefaultContainerId()) {
    FireWebUIListener("crostini-container-upgrade-available-changed",
                      base::Value(can_upgrade));
  }
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
  CHECK_EQ(0U, args->GetList().size());

  crostini::CrostiniFeatures::Get()->CanChangeAdbSideloading(
      profile_, base::BindOnce(&CrostiniHandler::OnCanEnableArcAdbSideloading,
                               weak_ptr_factory_.GetWeakPtr()));
}

void CrostiniHandler::OnCanEnableArcAdbSideloading(
    bool can_change_adb_sideloading) {
  if (!can_change_adb_sideloading)
    return;

  LogEvent(CrostiniSettingsEvent::kEnableAdbSideloading);

  PrefService* prefs = g_browser_process->local_state();
  prefs->SetBoolean(prefs::kEnableAdbSideloadingRequested, true);
  prefs->CommitPendingWrite();

  chrome::AttemptRelaunch();
}

void CrostiniHandler::HandleDisableArcAdbRequest(const base::ListValue* args) {
  CHECK_EQ(0U, args->GetList().size());

  crostini::CrostiniFeatures::Get()->CanChangeAdbSideloading(
      profile_, base::BindOnce(&CrostiniHandler::OnCanDisableArcAdbSideloading,
                               weak_ptr_factory_.GetWeakPtr()));
}

void CrostiniHandler::OnCanDisableArcAdbSideloading(
    bool can_change_adb_sideloading) {
  if (!can_change_adb_sideloading)
    return;

  LogEvent(CrostiniSettingsEvent::kDisableAdbSideloading);

  PrefService* prefs = g_browser_process->local_state();
  prefs->SetBoolean(prefs::kFactoryResetRequested, true);
  prefs->CommitPendingWrite();

  chromeos::PowerManagerClient::Get()->RequestRestart(
      power_manager::REQUEST_RESTART_FOR_USER, "disable adb sideloading");
}

void CrostiniHandler::LaunchTerminal() {
  crostini::LaunchCrostiniApp(
      profile_, crostini::kCrostiniTerminalSystemAppId,
      display::Screen::GetScreen()->GetPrimaryDisplay().id());
}

void CrostiniHandler::HandleRequestContainerUpgradeView(
    const base::ListValue* args) {
  CHECK_EQ(0U, args->GetList().size());
  chromeos::CrostiniUpgraderDialog::Show(
      profile_,
      base::BindOnce(&CrostiniHandler::LaunchTerminal,
                     weak_ptr_factory_.GetWeakPtr()),
      // If the user cancels the upgrade, we won't need to restart Crostini and
      // we don't want to run the launch closure which would launch Terminal.
      /*only_run_launch_closure_on_restart=*/true);
}

void CrostiniHandler::OnCrostiniExportImportOperationStatusChanged(
    bool in_progress) {
  // Other side listens with cr.addWebUIListener
  FireWebUIListener("crostini-export-import-operation-status-changed",
                    base::Value(in_progress));
}

void CrostiniHandler::HandleQueryArcAdbRequest(const base::ListValue* args) {
  AllowJavascript();
  CHECK_EQ(0U, args->GetList().size());

  chromeos::SessionManagerClient* client =
      chromeos::SessionManagerClient::Get();
  client->QueryAdbSideload(base::BindOnce(&CrostiniHandler::OnQueryAdbSideload,
                                          weak_ptr_factory_.GetWeakPtr()));
}

void CrostiniHandler::HandleCanChangeArcAdbSideloadingRequest(
    const base::ListValue* args) {
  AllowJavascript();
  CHECK_EQ(0U, args->GetList().size());

  FetchCanChangeAdbSideloading();
}

void CrostiniHandler::FetchCanChangeAdbSideloading() {
  crostini::CrostiniFeatures::Get()->CanChangeAdbSideloading(
      profile_, base::BindOnce(&CrostiniHandler::OnCanChangeArcAdbSideloading,
                               weak_ptr_factory_.GetWeakPtr()));
}

void CrostiniHandler::OnCanChangeArcAdbSideloading(
    bool can_change_arc_adb_sideloading) {
  FireWebUIListener("crostini-can-change-arc-adb-sideload-changed",
                    base::Value(can_change_arc_adb_sideloading));
}

void CrostiniHandler::HandleCrostiniUpgraderDialogStatusRequest(
    const base::ListValue* args) {
  AllowJavascript();
  CHECK_EQ(0U, args->GetList().size());
  bool is_open = crostini::CrostiniManager::GetForProfile(profile_)
                     ->GetCrostiniDialogStatus(crostini::DialogType::UPGRADER);
  OnCrostiniDialogStatusChanged(crostini::DialogType::UPGRADER, is_open);
}

void CrostiniHandler::HandleCrostiniContainerUpgradeAvailableRequest(
    const base::ListValue* args) {
  AllowJavascript();

  bool can_upgrade = crostini::ShouldAllowContainerUpgrade(profile_);
  OnContainerOsReleaseChanged(crostini::DefaultContainerId(), can_upgrade);
}

void CrostiniHandler::OnActivePortsChanged(const base::ListValue& activePorts) {
  // Other side listens with cr.addWebUIListener
  FireWebUIListener("crostini-port-forwarder-active-ports-changed",
                    activePorts);
}

void CrostiniHandler::HandleAddCrostiniPortForward(
    const base::ListValue* args) {
  CHECK_EQ(6U, args->GetList().size());

  std::string callback_id = args->GetList()[0].GetString();
  std::string vm_name = args->GetList()[1].GetString();
  std::string container_name = args->GetList()[2].GetString();
  int port_number = args->GetList()[3].GetInt();
  int protocol_type = args->GetList()[4].GetInt();
  std::string label = args->GetList()[5].GetString();

  if (!crostini::CrostiniFeatures::Get()->IsPortForwardingAllowed(profile_)) {
    OnPortForwardComplete(callback_id, false);
    return;
  }

  crostini::CrostiniPortForwarder::GetForProfile(profile_)->AddPort(
      crostini::ContainerId(std::move(vm_name), std::move(container_name)),
      port_number,
      static_cast<crostini::CrostiniPortForwarder::Protocol>(protocol_type),
      std::move(label),
      base::BindOnce(&CrostiniHandler::OnPortForwardComplete,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback_id)));
}

void CrostiniHandler::HandleRemoveCrostiniPortForward(
    const base::ListValue* args) {
  CHECK_EQ(5U, args->GetSize());
  std::string callback_id;
  CHECK(args->GetString(0, &callback_id));
  std::string vm_name;
  CHECK(args->GetString(1, &vm_name));
  std::string container_name;
  CHECK(args->GetString(2, &container_name));
  int port_number;
  CHECK(args->GetInteger(3, &port_number));
  int protocol_type;
  CHECK(args->GetInteger(4, &protocol_type));

  if (!crostini::CrostiniFeatures::Get()->IsPortForwardingAllowed(profile_)) {
    OnPortForwardComplete(callback_id, false);
    return;
  }

  crostini::CrostiniPortForwarder::GetForProfile(profile_)->RemovePort(
      crostini::ContainerId(std::move(vm_name), std::move(container_name)),
      port_number,
      static_cast<crostini::CrostiniPortForwarder::Protocol>(protocol_type),
      base::Bind(&CrostiniHandler::OnPortForwardComplete,
                 weak_ptr_factory_.GetWeakPtr(), std::move(callback_id)));
}

void CrostiniHandler::HandleRemoveAllCrostiniPortForwards(
    const base::ListValue* args) {
  CHECK_EQ(2U, args->GetSize());
  const auto& args_list = args->GetList();
  std::string vm_name = args_list[0].GetString();
  std::string container_name = args_list[1].GetString();

  if (!crostini::CrostiniFeatures::Get()->IsPortForwardingAllowed(profile_)) {
    return;
  }

  crostini::CrostiniPortForwarder::GetForProfile(profile_)->RemoveAllPorts(
      crostini::ContainerId(std::move(vm_name), std::move(container_name)));
}

void CrostiniHandler::HandleActivateCrostiniPortForward(
    const base::ListValue* args) {
  CHECK_EQ(5U, args->GetSize());

  std::string callback_id;
  CHECK(args->GetString(0, &callback_id));
  std::string vm_name;
  CHECK(args->GetString(1, &vm_name));
  std::string container_name;
  CHECK(args->GetString(2, &container_name));
  int port_number;
  CHECK(args->GetInteger(3, &port_number));
  int protocol_type;
  CHECK(args->GetInteger(4, &protocol_type));

  if (!crostini::CrostiniFeatures::Get()->IsPortForwardingAllowed(profile_)) {
    OnPortForwardComplete(callback_id, false);
    return;
  }

  crostini::CrostiniPortForwarder::GetForProfile(profile_)->ActivatePort(
      crostini::ContainerId(std::move(vm_name), std::move(container_name)),
      port_number,
      static_cast<crostini::CrostiniPortForwarder::Protocol>(protocol_type),
      base::Bind(&CrostiniHandler::OnPortForwardComplete,
                 weak_ptr_factory_.GetWeakPtr(), std::move(callback_id)));
}

void CrostiniHandler::HandleDeactivateCrostiniPortForward(
    const base::ListValue* args) {
  CHECK_EQ(5U, args->GetSize());

  std::string callback_id;
  CHECK(args->GetString(0, &callback_id));
  std::string vm_name;
  CHECK(args->GetString(1, &vm_name));
  std::string container_name;
  CHECK(args->GetString(2, &container_name));
  int port_number;
  CHECK(args->GetInteger(3, &port_number));
  int protocol_type;
  CHECK(args->GetInteger(4, &protocol_type));

  if (!crostini::CrostiniFeatures::Get()->IsPortForwardingAllowed(profile_)) {
    OnPortForwardComplete(callback_id, false);
    return;
  }

  crostini::CrostiniPortForwarder::GetForProfile(profile_)->DeactivatePort(
      crostini::ContainerId(std::move(vm_name), std::move(container_name)),
      port_number,
      static_cast<crostini::CrostiniPortForwarder::Protocol>(protocol_type),
      base::Bind(&CrostiniHandler::OnPortForwardComplete,
                 weak_ptr_factory_.GetWeakPtr(), std::move(callback_id)));
}

void CrostiniHandler::OnPortForwardComplete(std::string callback_id,
                                            bool success) {
  ResolveJavascriptCallback(base::Value(callback_id), base::Value(success));
}

void CrostiniHandler::ResolveGetCrostiniDiskInfoCallback(
    const std::string& callback_id,
    std::unique_ptr<crostini::CrostiniDiskInfo> disk_info) {
  ResolveJavascriptCallback(base::Value(std::move(callback_id)),
                            CrostiniDiskInfoToValue(std::move(disk_info)));
}

void CrostiniHandler::HandleGetCrostiniDiskInfo(const base::ListValue* args) {
  AllowJavascript();
  CHECK_EQ(3U, args->GetList().size());
  std::string callback_id = args->GetList()[0].GetString();
  std::string vm_name = args->GetList()[1].GetString();
  bool full_info = args->GetList()[2].GetBool();
  crostini::disk::GetDiskInfo(
      base::BindOnce(&CrostiniHandler::ResolveGetCrostiniDiskInfoCallback,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback_id)),
      profile_, std::move(vm_name), full_info);
}

void CrostiniHandler::HandleResizeCrostiniDisk(const base::ListValue* args) {
  CHECK_EQ(3U, args->GetList().size());
  std::string callback_id = args->GetList()[0].GetString();
  std::string vm_name = args->GetList()[1].GetString();
  double bytes = args->GetList()[2].GetDouble();
  crostini::disk::ResizeCrostiniDisk(
      profile_, std::move(vm_name), bytes,
      base::BindOnce(&CrostiniHandler::ResolveResizeCrostiniDiskCallback,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback_id)));
}

void CrostiniHandler::ResolveResizeCrostiniDiskCallback(
    const std::string& callback_id,
    bool succeeded) {
  ResolveJavascriptCallback(base::Value(std::move(callback_id)),
                            base::Value(succeeded));
}

void CrostiniHandler::HandleCheckCrostiniMicSharingStatus(
    const base::ListValue* args) {
  CHECK_EQ(2U, args->GetList().size());
  std::string callback_id = args->GetList()[0].GetString();
  bool proposed_value = args->GetList()[1].GetBool();
  bool requiresRestart =
      crostini::IsCrostiniRunning(profile_) &&
      crostini::CrostiniManager::GetForProfile(profile_)
              ->crostini_mic_sharing_enabled() != proposed_value;

  ResolveJavascriptCallback(base::Value(std::move(callback_id)),
                            base::Value(requiresRestart));
}

void CrostiniHandler::HandleGetCrostiniActivePorts(
    const base::ListValue* args) {
  AllowJavascript();
  CHECK_EQ(1U, args->GetList().size());

  std::string callback_id = args->GetList()[0].GetString();

  ResolveJavascriptCallback(
      base::Value(callback_id),
      crostini::CrostiniPortForwarder::GetForProfile(profile_)
          ->GetActivePorts());
}

void CrostiniHandler::HandleCheckCrostiniIsRunning(
    const base::ListValue* args) {
  AllowJavascript();
  CHECK_EQ(1U, args->GetList().size());

  std::string callback_id = args->GetList()[0].GetString();

  ResolveJavascriptCallback(base::Value(callback_id),
                            base::Value(crostini::IsCrostiniRunning(profile_)));
}

void CrostiniHandler::OnContainerStarted(
    const crostini::ContainerId& container_id) {
  FireWebUIListener("crostini-status-changed", base::Value(true));
}

void CrostiniHandler::OnContainerShutdown(
    const crostini::ContainerId& container_id) {
  FireWebUIListener("crostini-status-changed", base::Value(false));
}

void CrostiniHandler::HandleShutdownCrostini(const base::ListValue* args) {
  CHECK_EQ(0U, args->GetList().size());

  const std::string vm_name = "termina";

  crostini::CrostiniManager::GetForProfile(profile_)->StopVm(
      std::move(vm_name), std::move(base::DoNothing()));
}

void CrostiniHandler::OnCrostiniMicSharingEnabledChanged(bool enabled) {
  FireWebUIListener("crostini-mic-sharing-enabled-changed",
                    base::Value(enabled));
}

void CrostiniHandler::HandleSetCrostiniMicSharingEnabled(
    const base::ListValue* args) {
  CHECK_EQ(1U, args->GetList().size());
  bool enabled = args->GetList()[0].GetBool();

  crostini::CrostiniManager::GetForProfile(profile_)
      ->SetCrostiniMicSharingEnabled(enabled);
}

void CrostiniHandler::HandleGetCrostiniMicSharingEnabled(
    const base::ListValue* args) {
  CHECK_EQ(1U, args->GetList().size());

  std::string callback_id = args->GetList()[0].GetString();

  ResolveJavascriptCallback(
      base::Value(callback_id),
      base::Value(crostini::CrostiniManager::GetForProfile(profile_)
                      ->crostini_mic_sharing_enabled()));
}

}  // namespace settings
}  // namespace chromeos
