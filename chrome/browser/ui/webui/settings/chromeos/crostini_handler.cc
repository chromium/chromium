// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/crostini_handler.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ash/crostini/crostini_disk.h"
#include "chrome/browser/ash/crostini/crostini_features.h"
#include "chrome/browser/ash/crostini/crostini_installer.h"
#include "chrome/browser/ash/crostini/crostini_port_forwarder.h"
#include "chrome/browser/ash/crostini/crostini_pref_names.h"
#include "chrome/browser/ash/crostini/crostini_types.mojom.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/browser_process.h"
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
                          handler_weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "requestRemoveCrostini",
      base::BindRepeating(&CrostiniHandler::HandleRequestRemoveCrostini,
                          handler_weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "exportCrostiniContainer",
      base::BindRepeating(&CrostiniHandler::HandleExportCrostiniContainer,
                          handler_weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "importCrostiniContainer",
      base::BindRepeating(&CrostiniHandler::HandleImportCrostiniContainer,
                          handler_weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "requestCrostiniInstallerStatus",
      base::BindRepeating(
          &CrostiniHandler::HandleCrostiniInstallerStatusRequest,
          handler_weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "requestCrostiniExportImportOperationStatus",
      base::BindRepeating(
          &CrostiniHandler::HandleCrostiniExportImportOperationStatusRequest,
          handler_weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "requestArcAdbSideloadStatus",
      base::BindRepeating(&CrostiniHandler::HandleQueryArcAdbRequest,
                          handler_weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "getCanChangeArcAdbSideloading",
      base::BindRepeating(
          &CrostiniHandler::HandleCanChangeArcAdbSideloadingRequest,
          handler_weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "enableArcAdbSideload",
      base::BindRepeating(&CrostiniHandler::HandleEnableArcAdbRequest,
                          handler_weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "disableArcAdbSideload",
      base::BindRepeating(&CrostiniHandler::HandleDisableArcAdbRequest,
                          handler_weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "requestCrostiniContainerUpgradeView",
      base::BindRepeating(&CrostiniHandler::HandleRequestContainerUpgradeView,
                          handler_weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "requestCrostiniUpgraderDialogStatus",
      base::BindRepeating(
          &CrostiniHandler::HandleCrostiniUpgraderDialogStatusRequest,
          handler_weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "requestCrostiniContainerUpgradeAvailable",
      base::BindRepeating(
          &CrostiniHandler::HandleCrostiniContainerUpgradeAvailableRequest,
          handler_weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "getCrostiniDiskInfo",
      base::BindRepeating(&CrostiniHandler::HandleGetCrostiniDiskInfo,
                          handler_weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "resizeCrostiniDisk",
      base::BindRepeating(&CrostiniHandler::HandleResizeCrostiniDisk,
                          handler_weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "addCrostiniPortForward",
      base::BindRepeating(&CrostiniHandler::HandleAddCrostiniPortForward,
                          handler_weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "removeCrostiniPortForward",
      base::BindRepeating(&CrostiniHandler::HandleRemoveCrostiniPortForward,
                          handler_weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "removeAllCrostiniPortForwards",
      base::BindRepeating(&CrostiniHandler::HandleRemoveAllCrostiniPortForwards,
                          handler_weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "activateCrostiniPortForward",
      base::BindRepeating(&CrostiniHandler::HandleActivateCrostiniPortForward,
                          handler_weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "deactivateCrostiniPortForward",
      base::BindRepeating(&CrostiniHandler::HandleDeactivateCrostiniPortForward,
                          handler_weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "getCrostiniActivePorts",
      base::BindRepeating(&CrostiniHandler::HandleGetCrostiniActivePorts,
                          handler_weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "checkCrostiniIsRunning",
      base::BindRepeating(&CrostiniHandler::HandleCheckCrostiniIsRunning,
                          handler_weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "shutdownCrostini",
      base::BindRepeating(&CrostiniHandler::HandleShutdownCrostini,
                          handler_weak_ptr_factory_.GetWeakPtr()));
  if (crostini::CrostiniFeatures::Get()->IsMultiContainerAllowed(profile_)) {
    web_ui()->RegisterMessageCallback(
        "createContainer",
        base::BindRepeating(&CrostiniHandler::HandleCreateContainer,
                            handler_weak_ptr_factory_.GetWeakPtr()));
    web_ui()->RegisterMessageCallback(
        "deleteContainer",
        base::BindRepeating(&CrostiniHandler::HandleDeleteContainer,
                            handler_weak_ptr_factory_.GetWeakPtr()));
    web_ui()->RegisterMessageCallback(
        "requestContainerInfo",
        base::BindRepeating(&CrostiniHandler::HandleRequestContainerInfo,
                            handler_weak_ptr_factory_.GetWeakPtr()));
    web_ui()->RegisterMessageCallback(
        "setContainerBadgeColor",
        base::BindRepeating(&CrostiniHandler::HandleSetContainerBadgeColor,
                            handler_weak_ptr_factory_.GetWeakPtr()));
    web_ui()->RegisterMessageCallback(
        "stopContainer",
        base::BindRepeating(&CrostiniHandler::HandleStopContainer,
                            handler_weak_ptr_factory_.GetWeakPtr()));
  }
}

void CrostiniHandler::OnJavascriptAllowed() {
  auto* crostini_manager = crostini::CrostiniManager::GetForProfile(profile_);
  crostini_manager->AddCrostiniDialogStatusObserver(this);
  crostini_manager->AddCrostiniContainerPropertiesObserver(this);
  crostini_manager->AddContainerStartedObserver(this);
  crostini_manager->AddContainerShutdownObserver(this);
  crostini::CrostiniExportImport::GetForProfile(profile_)->AddObserver(this);
  crostini::CrostiniPortForwarder::GetForProfile(profile_)->AddObserver(this);

  // Observe ADB sideloading device policy and react to its changes
  adb_sideloading_device_policy_subscription_ =
      chromeos::CrosSettings::Get()->AddSettingsObserver(
          chromeos::kDeviceCrostiniArcAdbSideloadingAllowed,
          base::BindRepeating(&CrostiniHandler::FetchCanChangeAdbSideloading,
                              handler_weak_ptr_factory_.GetWeakPtr()));

  // Observe ADB sideloading user policy and react to its changes
  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      crostini::prefs::kCrostiniArcAdbSideloadingUserPref,
      base::BindRepeating(&CrostiniHandler::FetchCanChangeAdbSideloading,
                          handler_weak_ptr_factory_.GetWeakPtr()));

  // Observe changes to containers in general
  pref_change_registrar_.Add(
      crostini::prefs::kCrostiniContainers,
      base::BindRepeating(&CrostiniHandler::HandleRequestContainerInfo,
                          handler_weak_ptr_factory_.GetWeakPtr(),
                          base::Value::List()));
}

void CrostiniHandler::OnJavascriptDisallowed() {
  auto* crostini_manager = crostini::CrostiniManager::GetForProfile(profile_);
  crostini_manager->RemoveCrostiniDialogStatusObserver(this);
  crostini_manager->RemoveCrostiniContainerPropertiesObserver(this);
  crostini_manager->RemoveContainerStartedObserver(this);
  crostini_manager->RemoveContainerShutdownObserver(this);
  crostini::CrostiniExportImport::GetForProfile(profile_)->RemoveObserver(this);
  crostini::CrostiniPortForwarder::GetForProfile(profile_)->RemoveObserver(
      this);

  adb_sideloading_device_policy_subscription_ = {};
  pref_change_registrar_.RemoveAll();
  callback_weak_ptr_factory_.InvalidateWeakPtrs();
}

void CrostiniHandler::HandleRequestCrostiniInstallerView(
    const base::Value::List& args) {
  AllowJavascript();
  crostini::CrostiniInstaller::GetForProfile(Profile::FromWebUI(web_ui()))
      ->ShowDialog(crostini::CrostiniUISurface::kSettings);
}

void CrostiniHandler::HandleRequestRemoveCrostini(
    const base::Value::List& args) {
  AllowJavascript();
  ShowCrostiniUninstallerView(Profile::FromWebUI(web_ui()),
                              crostini::CrostiniUISurface::kSettings);
}

namespace {

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

void CrostiniHandler::HandleExportCrostiniContainer(
    const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  crostini::ContainerId container_id(args[0]);
  VLOG(1) << "Exporting  = " << container_id;

  crostini::CrostiniExportImport::GetForProfile(profile_)->ExportContainer(
      container_id, web_ui()->GetWebContents());
}

void CrostiniHandler::HandleImportCrostiniContainer(
    const base::Value::List& args) {
  CHECK_EQ(0U, args.size());
  crostini::CrostiniExportImport::GetForProfile(profile_)->ImportContainer(
      web_ui()->GetWebContents());
}

void CrostiniHandler::HandleCrostiniInstallerStatusRequest(
    const base::Value::List& args) {
  AllowJavascript();
  CHECK_EQ(0U, args.size());
  bool status = crostini::CrostiniManager::GetForProfile(profile_)
                    ->GetCrostiniDialogStatus(crostini::DialogType::INSTALLER);
  OnCrostiniDialogStatusChanged(crostini::DialogType::INSTALLER, status);
}

void CrostiniHandler::HandleCrostiniExportImportOperationStatusRequest(
    const base::Value::List& args) {
  AllowJavascript();
  CHECK_EQ(0U, args.size());
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

void CrostiniHandler::HandleEnableArcAdbRequest(const base::Value::List& args) {
  CHECK_EQ(0U, args.size());

  crostini::CrostiniFeatures::Get()->CanChangeAdbSideloading(
      profile_, base::BindOnce(&CrostiniHandler::OnCanEnableArcAdbSideloading,
                               handler_weak_ptr_factory_.GetWeakPtr()));
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

void CrostiniHandler::HandleDisableArcAdbRequest(
    const base::Value::List& args) {
  CHECK_EQ(0U, args.size());

  crostini::CrostiniFeatures::Get()->CanChangeAdbSideloading(
      profile_, base::BindOnce(&CrostiniHandler::OnCanDisableArcAdbSideloading,
                               handler_weak_ptr_factory_.GetWeakPtr()));
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

void CrostiniHandler::LaunchTerminal(apps::mojom::IntentPtr intent) {
  crostini::LaunchCrostiniAppWithIntent(
      profile_, crostini::kCrostiniTerminalSystemAppId,
      display::Screen::GetScreen()->GetPrimaryDisplay().id(),
      std::move(intent));
}

void CrostiniHandler::HandleRequestContainerUpgradeView(
    const base::Value::List& args) {
  CHECK_EQ(0U, args.size());
  chromeos::CrostiniUpgraderDialog::Show(
      profile_,
      base::BindOnce(&CrostiniHandler::LaunchTerminal,
                     handler_weak_ptr_factory_.GetWeakPtr(),
                     /*intent=*/nullptr),
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

void CrostiniHandler::HandleQueryArcAdbRequest(const base::Value::List& args) {
  AllowJavascript();
  CHECK_EQ(0U, args.size());

  chromeos::SessionManagerClient* client =
      chromeos::SessionManagerClient::Get();
  client->QueryAdbSideload(
      base::BindOnce(&CrostiniHandler::OnQueryAdbSideload,
                     handler_weak_ptr_factory_.GetWeakPtr()));
}

void CrostiniHandler::HandleCanChangeArcAdbSideloadingRequest(
    const base::Value::List& args) {
  AllowJavascript();
  CHECK_EQ(0U, args.size());

  FetchCanChangeAdbSideloading();
}

void CrostiniHandler::FetchCanChangeAdbSideloading() {
  crostini::CrostiniFeatures::Get()->CanChangeAdbSideloading(
      profile_, base::BindOnce(&CrostiniHandler::OnCanChangeArcAdbSideloading,
                               handler_weak_ptr_factory_.GetWeakPtr()));
}

void CrostiniHandler::OnCanChangeArcAdbSideloading(
    bool can_change_arc_adb_sideloading) {
  FireWebUIListener("crostini-can-change-arc-adb-sideload-changed",
                    base::Value(can_change_arc_adb_sideloading));
}

void CrostiniHandler::HandleCrostiniUpgraderDialogStatusRequest(
    const base::Value::List& args) {
  AllowJavascript();
  CHECK_EQ(0U, args.size());
  bool is_open = crostini::CrostiniManager::GetForProfile(profile_)
                     ->GetCrostiniDialogStatus(crostini::DialogType::UPGRADER);
  OnCrostiniDialogStatusChanged(crostini::DialogType::UPGRADER, is_open);
}

void CrostiniHandler::HandleCrostiniContainerUpgradeAvailableRequest(
    const base::Value::List& args) {
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
    const base::Value::List& args) {
  CHECK_EQ(5U, args.size());

  std::string callback_id = args[0].GetString();
  crostini::ContainerId container_id(args[1]);
  int port_number = args[2].GetInt();
  int protocol_type = args[3].GetInt();
  std::string label = args[4].GetString();

  if (!crostini::CrostiniFeatures::Get()->IsPortForwardingAllowed(profile_)) {
    OnPortForwardComplete(callback_id, false);
    return;
  }

  crostini::CrostiniPortForwarder::GetForProfile(profile_)->AddPort(
      container_id, port_number,
      static_cast<crostini::CrostiniPortForwarder::Protocol>(protocol_type),
      std::move(label),
      base::BindOnce(&CrostiniHandler::OnPortForwardComplete,
                     callback_weak_ptr_factory_.GetWeakPtr(),
                     std::move(callback_id)));
}

void CrostiniHandler::HandleRemoveCrostiniPortForward(
    const base::Value::List& args) {
  const auto& list = args;
  CHECK_EQ(4U, list.size());

  std::string callback_id = list[0].GetString();
  crostini::ContainerId container_id(list[1]);
  int port_number = list[2].GetInt();
  int protocol_type = list[3].GetInt();

  if (!crostini::CrostiniFeatures::Get()->IsPortForwardingAllowed(profile_)) {
    OnPortForwardComplete(callback_id, false);
    return;
  }

  crostini::CrostiniPortForwarder::GetForProfile(profile_)->RemovePort(
      container_id, port_number,
      static_cast<crostini::CrostiniPortForwarder::Protocol>(protocol_type),
      base::BindOnce(&CrostiniHandler::OnPortForwardComplete,
                     callback_weak_ptr_factory_.GetWeakPtr(),
                     std::move(callback_id)));
}

void CrostiniHandler::HandleRemoveAllCrostiniPortForwards(
    const base::Value::List& args) {
  CHECK_EQ(1U, args.size());

  if (!crostini::CrostiniFeatures::Get()->IsPortForwardingAllowed(profile_)) {
    return;
  }

  crostini::CrostiniPortForwarder::GetForProfile(profile_)->RemoveAllPorts(
      crostini::ContainerId(args[0]));
}

void CrostiniHandler::HandleActivateCrostiniPortForward(
    const base::Value::List& args) {
  const auto& list = args;
  CHECK_EQ(4U, list.size());

  std::string callback_id = list[0].GetString();
  crostini::ContainerId container_id(list[1]);
  int port_number = list[2].GetInt();
  int protocol_type = list[3].GetInt();

  if (!crostini::CrostiniFeatures::Get()->IsPortForwardingAllowed(profile_)) {
    OnPortForwardComplete(callback_id, false);
    return;
  }

  crostini::CrostiniPortForwarder::GetForProfile(profile_)->ActivatePort(
      container_id, port_number,
      static_cast<crostini::CrostiniPortForwarder::Protocol>(protocol_type),
      base::BindOnce(&CrostiniHandler::OnPortForwardComplete,
                     callback_weak_ptr_factory_.GetWeakPtr(),
                     std::move(callback_id)));
}

void CrostiniHandler::HandleDeactivateCrostiniPortForward(
    const base::Value::List& args) {
  const auto& list = args;
  CHECK_EQ(4U, list.size());

  std::string callback_id = list[0].GetString();
  crostini::ContainerId container_id(list[1]);
  int port_number = list[2].GetInt();
  int protocol_type = list[3].GetInt();

  if (!crostini::CrostiniFeatures::Get()->IsPortForwardingAllowed(profile_)) {
    OnPortForwardComplete(callback_id, false);
    return;
  }

  crostini::CrostiniPortForwarder::GetForProfile(profile_)->DeactivatePort(
      container_id, port_number,
      static_cast<crostini::CrostiniPortForwarder::Protocol>(protocol_type),
      base::BindOnce(&CrostiniHandler::OnPortForwardComplete,
                     callback_weak_ptr_factory_.GetWeakPtr(),
                     std::move(callback_id)));
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

void CrostiniHandler::HandleGetCrostiniDiskInfo(const base::Value::List& args) {
  AllowJavascript();
  CHECK_EQ(3U, args.size());
  std::string callback_id = args[0].GetString();
  std::string vm_name = args[1].GetString();
  bool full_info = args[2].GetBool();
  crostini::disk::GetDiskInfo(
      base::BindOnce(&CrostiniHandler::ResolveGetCrostiniDiskInfoCallback,
                     callback_weak_ptr_factory_.GetWeakPtr(),
                     std::move(callback_id)),
      profile_, std::move(vm_name), full_info);
}

void CrostiniHandler::HandleResizeCrostiniDisk(const base::Value::List& args) {
  CHECK_EQ(3U, args.size());
  std::string callback_id = args[0].GetString();
  std::string vm_name = args[1].GetString();
  double bytes = args[2].GetDouble();
  crostini::disk::ResizeCrostiniDisk(
      profile_, std::move(vm_name), bytes,
      base::BindOnce(&CrostiniHandler::ResolveResizeCrostiniDiskCallback,
                     callback_weak_ptr_factory_.GetWeakPtr(),
                     std::move(callback_id)));
}

void CrostiniHandler::ResolveResizeCrostiniDiskCallback(
    const std::string& callback_id,
    bool succeeded) {
  ResolveJavascriptCallback(base::Value(std::move(callback_id)),
                            base::Value(succeeded));
}

void CrostiniHandler::HandleGetCrostiniActivePorts(
    const base::Value::List& args) {
  AllowJavascript();
  CHECK_EQ(1U, args.size());

  std::string callback_id = args[0].GetString();

  ResolveJavascriptCallback(
      base::Value(callback_id),
      crostini::CrostiniPortForwarder::GetForProfile(profile_)
          ->GetActivePorts());
}

void CrostiniHandler::HandleCheckCrostiniIsRunning(
    const base::Value::List& args) {
  AllowJavascript();
  CHECK_EQ(1U, args.size());

  std::string callback_id = args[0].GetString();

  ResolveJavascriptCallback(base::Value(callback_id),
                            base::Value(crostini::IsCrostiniRunning(profile_)));
}

void CrostiniHandler::OnContainerStarted(
    const crostini::ContainerId& container_id) {
  FireWebUIListener("crostini-status-changed", base::Value(true));
  HandleRequestContainerInfo(base::Value::List());
}

void CrostiniHandler::OnContainerShutdown(
    const crostini::ContainerId& container_id) {
  FireWebUIListener("crostini-status-changed", base::Value(false));
  HandleRequestContainerInfo(base::Value::List());
}

void CrostiniHandler::HandleShutdownCrostini(const base::Value::List& args) {
  CHECK_EQ(0U, args.size());

  const std::string vm_name = "termina";

  crostini::CrostiniManager::GetForProfile(profile_)->StopVm(std::move(vm_name),
                                                             base::DoNothing());
}

void CrostiniHandler::HandleCreateContainer(const base::Value::List& args) {
  CHECK_EQ(3U, args.size());
  crostini::ContainerId container_id(args[0]);
  GURL image_server_url(args[1].GetString());
  std::string image_alias(args[2].GetString());

  if (!crostini::CrostiniFeatures::Get()->IsMultiContainerAllowed(profile_)) {
    return;
  }

  if (!args[1].GetString().empty() && !image_server_url.is_valid()) {
    LOG(ERROR) << "Malformed data. image_server_url=" << args[1].GetString()
               << ", image_alias=" << image_alias;
    return;
  }
  VLOG(1) << "Creating container_id = " << container_id;

  crostini::CrostiniManager::RestartOptions options;
  if (image_server_url.is_valid()) {
    options.image_server_url = image_server_url.spec();
    VLOG(1) << "image_server_url = " << image_server_url;
  }
  if (!image_alias.empty()) {
    options.image_alias = image_alias;
    VLOG(1) << "image_alias = " << image_alias;
  }

  crostini::CrostiniManager::GetForProfile(profile_)
      ->RestartCrostiniWithOptions(container_id, std::move(options),
                                   base::DoNothing());
  apps::mojom::IntentPtr intent = apps::mojom::Intent::New();
  intent->extras = container_id.ToMap();

  // The Terminal will be added as an observer to the above restart.
  LaunchTerminal(std::move(intent));
}

void CrostiniHandler::HandleDeleteContainer(const base::Value::List& args) {
  CHECK_EQ(1U, args.size());

  if (!crostini::CrostiniFeatures::Get()->IsMultiContainerAllowed(profile_)) {
    return;
  }

  crostini::ContainerId container_id(args[0]);
  if (container_id == crostini::DefaultContainerId()) {
    LOG(ERROR) << "Deleting " << container_id << " not permitted";
    return;
  }
  VLOG(1) << "Deleting " << container_id;

  auto* crostini_manager = crostini::CrostiniManager::GetForProfile(profile_);
  crostini::CrostiniManager::RestartOptions options;
  options.stop_after_lxd_available = true;
  crostini_manager->RestartCrostiniWithOptions(
      container_id, std::move(options),
      base::BindOnce(
          [](base::WeakPtr<crostini::CrostiniManager> crostini_manager,
             crostini::ContainerId container_id,
             crostini::CrostiniResult result) {
            if (crostini_manager &&
                result == crostini::CrostiniResult::SUCCESS) {
              crostini_manager->DeleteLxdContainer(container_id,
                                                   base::DoNothing());
            }
          },
          crostini_manager->GetWeakPtr(), container_id));
}

void CrostiniHandler::HandleRequestContainerInfo(
    const base::Value::List& args) {
  constexpr char kIdKey[] = "id";
  constexpr char kIpv4Key[] = "ipv4";

  if (!crostini::CrostiniFeatures::Get()->IsMultiContainerAllowed(profile_)) {
    return;
  }
  base::Value::List container_info_list;

  const base::Value::List& containers =
      profile_->GetPrefs()
          ->Get(crostini::prefs::kCrostiniContainers)
          ->GetList();

  for (const auto& dict : containers) {
    crostini::ContainerId container_id(dict);
    base::Value::Dict container_info_value;
    container_info_value.Set(kIdKey, container_id.ToDictValue());
    auto info =
        crostini::CrostiniManager::GetForProfile(profile_)->GetContainerInfo(
            container_id);
    if (info) {
      container_info_value.Set(kIpv4Key, info->ipv4_address);
    }

    SkColor badge_color =
        crostini::GetContainerBadgeColor(profile_, container_id);
    std::string badge_color_str =
        base::StringPrintf("#%02x%02x%02x", SkColorGetR(badge_color),
                           SkColorGetG(badge_color), SkColorGetB(badge_color));
    container_info_value.Set("badge_color", badge_color_str);

    container_info_list.Append(std::move(container_info_value));
  }

  FireWebUIListener("crostini-container-info",
                    base::Value(std::move(container_info_list)));
}

void CrostiniHandler::HandleSetContainerBadgeColor(
    const base::Value::List& args) {
  CHECK_EQ(2U, args.size());

  crostini::ContainerId container_id(args[0]);
  SkColor badge_color(args[1].FindDoubleKey("value").value());

  crostini::SetContainerBadgeColor(profile_, container_id, badge_color);
}

void CrostiniHandler::HandleStopContainer(const base::Value::List& args) {
  CHECK_EQ(1U, args.size());

  if (!crostini::CrostiniFeatures::Get()->IsMultiContainerAllowed(profile_)) {
    return;
  }

  crostini::ContainerId container_id(args[0]);
  if (crostini::ShouldStopVm(profile_, container_id)) {
    crostini::CrostiniManager::GetForProfile(profile_)->StopVm(
        container_id.vm_name, base::DoNothing());
  } else {
    crostini::CrostiniManager::GetForProfile(profile_)->StopLxdContainer(
        container_id, base::DoNothing());
  }
}

}  // namespace settings
}  // namespace chromeos
