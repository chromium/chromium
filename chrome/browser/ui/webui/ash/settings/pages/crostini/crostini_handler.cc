// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/crostini/crostini_handler.h"

#include <string>
#include <utility>

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/ash/bruschetta/bruschetta_service.h"
#include "chrome/browser/ash/bruschetta/bruschetta_service_factory.h"
#include "chrome/browser/ash/bruschetta/bruschetta_util.h"
#include "chrome/browser/ash/crostini/crostini_disk.h"
#include "chrome/browser/ash/crostini/crostini_export_import.h"
#include "chrome/browser/ash/crostini/crostini_export_import_factory.h"
#include "chrome/browser/ash/crostini/crostini_features.h"
#include "chrome/browser/ash/crostini/crostini_installer.h"
#include "chrome/browser/ash/crostini/crostini_installer_factory.h"
#include "chrome/browser/ash/crostini/crostini_port_forwarder.h"
#include "chrome/browser/ash/crostini/crostini_port_forwarder_factory.h"
#include "chrome/browser/ash/crostini/crostini_pref_names.h"
#include "chrome/browser/ash/crostini/crostini_shared_devices.h"
#include "chrome/browser/ash/crostini/crostini_shared_devices_factory.h"
#include "chrome/browser/ash/crostini/crostini_types.mojom.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/guest_os/guest_id.h"
#include "chrome/browser/ash/guest_os/guest_os_pref_names.h"
#include "chrome/browser/ash/guest_os/guest_os_session_tracker.h"
#include "chrome/browser/ash/guest_os/guest_os_session_tracker_factory.h"
#include "chrome/browser/ash/guest_os/guest_os_terminal.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/bruschetta/bruschetta_installer_view.h"
#include "chrome/browser/ui/views/bruschetta/bruschetta_uninstaller_view.h"
#include "chrome/browser/ui/views/crostini/crostini_uninstaller_view.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/login/session/session_termination_manager.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "components/prefs/pref_service.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_ui.h"
#include "ui/display/screen.h"

namespace ash::settings {

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
      "exportDiskImage",
      base::BindRepeating(&CrostiniHandler::HandleExportDiskImage,
                          handler_weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "importDiskImage",
      base::BindRepeating(&CrostiniHandler::HandleImportDiskImage,
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
      "getCrostiniActiveNetworkInfo",
      base::BindRepeating(&CrostiniHandler::HandleGetCrostiniActiveNetworkInfo,
                          handler_weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "checkCrostiniIsRunning",
      base::BindRepeating(&CrostiniHandler::HandleCheckCrostiniIsRunning,
                          handler_weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "checkBruschettaIsRunning",
      base::BindRepeating(&CrostiniHandler::HandleCheckBruschettaIsRunning,
                          handler_weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "shutdownCrostini",
      base::BindRepeating(&CrostiniHandler::HandleShutdownCrostini,
                          handler_weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "shutdownBruschetta",
      base::BindRepeating(&CrostiniHandler::HandleShutdownBruschetta,
                          handler_weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "requestContainerInfo",
      base::BindRepeating(&CrostiniHandler::HandleRequestContainerInfo,
                          handler_weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "requestBruschettaInstallerView",
      base::BindRepeating(
          &CrostiniHandler::HandleRequestBruschettaInstallerView,
          handler_weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "requestBruschettaUninstallerView",
      base::BindRepeating(
          &CrostiniHandler::HandleRequestBruschettaUninstallerView,
          handler_weak_ptr_factory_.GetWeakPtr()));
}

void CrostiniHandler::OnJavascriptAllowed() {
  auto* crostini_manager = crostini::CrostiniManager::GetForProfile(profile_);
  crostini_manager->AddCrostiniDialogStatusObserver(this);
  crostini_manager->AddContainerShutdownObserver(this);
  crostini::CrostiniExportImportFactory::GetForProfile(profile_)->AddObserver(
      this);
  crostini::CrostiniPortForwarderFactory::GetForProfile(profile_)->AddObserver(
      this);
  guest_os::GuestOsSessionTrackerFactory::GetForProfile(profile_)
      ->AddContainerStartedObserver(this);

  // Observe ADB sideloading device policy and react to its changes
  adb_sideloading_device_policy_subscription_ =
      CrosSettings::Get()->AddSettingsObserver(
          kDeviceCrostiniArcAdbSideloadingAllowed,
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
      guest_os::prefs::kGuestOsContainers,
      base::BindRepeating(&CrostiniHandler::HandleRequestContainerInfo,
                          handler_weak_ptr_factory_.GetWeakPtr(),
                          base::ListValue()));
}

void CrostiniHandler::OnJavascriptDisallowed() {
  auto* crostini_manager = crostini::CrostiniManager::GetForProfile(profile_);
  crostini_manager->RemoveCrostiniDialogStatusObserver(this);
  crostini_manager->RemoveContainerShutdownObserver(this);
  crostini::CrostiniExportImportFactory::GetForProfile(profile_)
      ->RemoveObserver(this);
  crostini::CrostiniPortForwarderFactory::GetForProfile(profile_)
      ->RemoveObserver(this);
  guest_os::GuestOsSessionTrackerFactory::GetForProfile(profile_)
      ->RemoveContainerStartedObserver(this);

  adb_sideloading_device_policy_subscription_ = {};
  pref_change_registrar_.RemoveAll();
  callback_weak_ptr_factory_.InvalidateWeakPtrs();
}

void CrostiniHandler::HandleRequestCrostiniInstallerView(
    const base::ListValue& args) {
  AllowJavascript();
  crostini::CrostiniInstallerFactory::GetForProfile(
      Profile::FromWebUI(web_ui()))
      ->ShowDialog(crostini::CrostiniUISurface::kSettings);
}

void CrostiniHandler::HandleRequestRemoveCrostini(const base::ListValue& args) {
  AllowJavascript();
  crostini::ShowCrostiniUninstallerView(Profile::FromWebUI(web_ui()));
}

namespace {

base::DictValue CrostiniDiskInfoToValue(
    std::unique_ptr<crostini::CrostiniDiskInfo> disk_info) {
  base::DictValue disk_value;
  if (!disk_info) {
    disk_value.Set("succeeded", false);
    return disk_value;
  }
  disk_value.Set("succeeded", true);
  disk_value.Set("canResize", disk_info->can_resize);
  disk_value.Set("isUserChosenSize", disk_info->is_user_chosen_size);
  disk_value.Set("isLowSpaceAvailable", disk_info->is_low_space_available);
  disk_value.Set("defaultIndex", disk_info->default_index);
  base::ListValue ticks;
  for (const auto& tick : disk_info->ticks) {
    base::DictValue t;
    t.Set("value", static_cast<double>(tick->value));
    t.Set("ariaValue", tick->aria_value);
    t.Set("label", tick->label);
    ticks.Append(std::move(t));
  }
  disk_value.Set("ticks", std::move(ticks));
  return disk_value;
}
}  // namespace

void CrostiniHandler::HandleExportCrostiniContainer(
    const base::ListValue& args) {
  CHECK_EQ(1U, args.size());
  guest_os::GuestId container_id(args[0]);
  VLOG(1) << "Exporting  = " << container_id;

  crostini::CrostiniExportImportFactory::GetForProfile(profile_)
      ->ExportContainer(container_id, web_ui()->GetWebContents());
}

void CrostiniHandler::HandleImportCrostiniContainer(
    const base::ListValue& args) {
  CHECK_EQ(1U, args.size());
  guest_os::GuestId container_id(args[0]);
  VLOG(1) << "Importing  = " << container_id;
  crostini::CrostiniExportImportFactory::GetForProfile(profile_)
      ->ImportContainer(container_id, web_ui()->GetWebContents());
}

void CrostiniHandler::HandleExportDiskImage(const base::ListValue& args) {
  CHECK_EQ(1U, args.size());
  guest_os::GuestId container_id(args[0]);
  VLOG(1) << "Exporting  = " << container_id;

  crostini::CrostiniExportImportFactory::GetForProfile(profile_)
      ->ExportDiskImageFlow(container_id, web_ui()->GetWebContents());
}

void CrostiniHandler::HandleImportDiskImage(const base::ListValue& args) {
  CHECK_EQ(1U, args.size());
  guest_os::GuestId container_id(args[0]);
  VLOG(1) << "Importing  = " << container_id;
  crostini::CrostiniExportImportFactory::GetForProfile(profile_)
      ->ImportDiskImageFlow(container_id, web_ui()->GetWebContents());
}

void CrostiniHandler::HandleCrostiniInstallerStatusRequest(
    const base::ListValue& args) {
  AllowJavascript();
  CHECK_EQ(0U, args.size());
  bool status = crostini::CrostiniManager::GetForProfile(profile_)
                    ->GetCrostiniDialogStatus(crostini::DialogType::INSTALLER);
  OnCrostiniDialogStatusChanged(crostini::DialogType::INSTALLER, status);
}

void CrostiniHandler::HandleCrostiniExportImportOperationStatusRequest(
    const base::ListValue& args) {
  AllowJavascript();
  CHECK_EQ(0U, args.size());
  bool in_progress =
      crostini::CrostiniExportImportFactory::GetForProfile(profile_)
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
      case crostini::DialogType::REMOVER:
        FireWebUIListener("crostini-remover-status-changed",
                          base::Value(status));
        break;
      default:
        NOTREACHED();
    }
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

void CrostiniHandler::HandleEnableArcAdbRequest(const base::ListValue& args) {
  CHECK_EQ(0U, args.size());

  crostini::CrostiniFeatures::Get()->CanChangeAdbSideloading(
      profile_, base::BindOnce(&CrostiniHandler::OnCanEnableArcAdbSideloading,
                               handler_weak_ptr_factory_.GetWeakPtr()));
}

void CrostiniHandler::OnCanEnableArcAdbSideloading(
    bool can_change_adb_sideloading) {
  if (!can_change_adb_sideloading) {
    return;
  }

  LogEvent(CrostiniSettingsEvent::kEnableAdbSideloading);

  PrefService* prefs = g_browser_process->local_state();
  prefs->SetBoolean(prefs::kEnableAdbSideloadingRequested, true);
  prefs->CommitPendingWrite();

  // TODO(crbug.com/479113713): Use better reason and description.
  ash::SessionTerminationManager::Get()->Reboot(
      power_manager::REQUEST_RESTART_OTHER, "Chrome relaunch");
}

void CrostiniHandler::HandleDisableArcAdbRequest(const base::ListValue& args) {
  CHECK_EQ(0U, args.size());

  crostini::CrostiniFeatures::Get()->CanChangeAdbSideloading(
      profile_, base::BindOnce(&CrostiniHandler::OnCanDisableArcAdbSideloading,
                               handler_weak_ptr_factory_.GetWeakPtr()));
}

void CrostiniHandler::OnCanDisableArcAdbSideloading(
    bool can_change_adb_sideloading) {
  if (!can_change_adb_sideloading) {
    return;
  }

  LogEvent(CrostiniSettingsEvent::kDisableAdbSideloading);

  PrefService* prefs = g_browser_process->local_state();
  prefs->SetBoolean(prefs::kFactoryResetRequested, true);
  prefs->CommitPendingWrite();

  chromeos::PowerManagerClient::Get()->RequestRestart(
      power_manager::REQUEST_RESTART_FOR_USER, "disable adb sideloading");
}

void CrostiniHandler::LaunchTerminal(apps::IntentPtr intent) {
  guest_os::LaunchTerminalWithIntent(
      profile_, display::Screen::Get()->GetPrimaryDisplay().id(),
      std::move(intent), base::DoNothing());
}

void CrostiniHandler::OnCrostiniExportImportOperationStatusChanged(
    bool in_progress) {
  // Other side listens with cr.addWebUIListener
  FireWebUIListener("crostini-export-import-operation-status-changed",
                    base::Value(in_progress));
}

void CrostiniHandler::HandleQueryArcAdbRequest(const base::ListValue& args) {
  AllowJavascript();
  CHECK_EQ(0U, args.size());

  SessionManagerClient* client = SessionManagerClient::Get();
  client->QueryAdbSideload(
      base::BindOnce(&CrostiniHandler::OnQueryAdbSideload,
                     handler_weak_ptr_factory_.GetWeakPtr()));
}

void CrostiniHandler::HandleCanChangeArcAdbSideloadingRequest(
    const base::ListValue& args) {
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

void CrostiniHandler::OnActivePortsChanged(const base::ListValue& activePorts) {
  // Other side listens with cr.addWebUIListener
  FireWebUIListener("crostini-port-forwarder-active-ports-changed",
                    activePorts);
}

void CrostiniHandler::OnActiveNetworkChanged(const base::Value& interface,
                                             const base::Value& ipAddress) {
  FireWebUIListener("crostini-active-network-info", interface, ipAddress);
}

void CrostiniHandler::HandleAddCrostiniPortForward(
    const base::ListValue& args) {
  CHECK_EQ(5U, args.size());

  const std::string& callback_id = args[0].GetString();
  guest_os::GuestId container_id(args[1]);
  int port_number = args[2].GetInt();
  int protocol_type = args[3].GetInt();
  const std::string& label = args[4].GetString();

  if (!crostini::CrostiniFeatures::Get()->IsPortForwardingAllowed(profile_)) {
    OnPortForwardComplete(callback_id, false);
    return;
  }

  crostini::CrostiniPortForwarderFactory::GetForProfile(profile_)->AddPort(
      container_id, port_number,
      static_cast<crostini::CrostiniPortForwarder::Protocol>(protocol_type),
      label,
      base::BindOnce(&CrostiniHandler::OnPortForwardComplete,
                     callback_weak_ptr_factory_.GetWeakPtr(), callback_id));
}

void CrostiniHandler::HandleRemoveCrostiniPortForward(
    const base::ListValue& args) {
  const auto& list = args;
  CHECK_EQ(4U, list.size());

  const std::string& callback_id = list[0].GetString();
  guest_os::GuestId container_id(list[1]);
  int port_number = list[2].GetInt();
  int protocol_type = list[3].GetInt();

  if (!crostini::CrostiniFeatures::Get()->IsPortForwardingAllowed(profile_)) {
    OnPortForwardComplete(callback_id, false);
    return;
  }

  crostini::CrostiniPortForwarderFactory::GetForProfile(profile_)->RemovePort(
      container_id, port_number,
      static_cast<crostini::CrostiniPortForwarder::Protocol>(protocol_type),
      base::BindOnce(&CrostiniHandler::OnPortForwardComplete,
                     callback_weak_ptr_factory_.GetWeakPtr(), callback_id));
}

void CrostiniHandler::HandleRemoveAllCrostiniPortForwards(
    const base::ListValue& args) {
  CHECK_EQ(1U, args.size());

  if (!crostini::CrostiniFeatures::Get()->IsPortForwardingAllowed(profile_)) {
    return;
  }

  crostini::CrostiniPortForwarderFactory::GetForProfile(profile_)
      ->RemoveAllPorts(guest_os::GuestId(args[0]));
}

void CrostiniHandler::HandleActivateCrostiniPortForward(
    const base::ListValue& args) {
  const auto& list = args;
  CHECK_EQ(4U, list.size());

  const std::string& callback_id = list[0].GetString();
  guest_os::GuestId container_id(list[1]);
  int port_number = list[2].GetInt();
  int protocol_type = list[3].GetInt();

  if (!crostini::CrostiniFeatures::Get()->IsPortForwardingAllowed(profile_)) {
    OnPortForwardComplete(callback_id, false);
    return;
  }

  crostini::CrostiniPortForwarderFactory::GetForProfile(profile_)->ActivatePort(
      container_id, port_number,
      static_cast<crostini::CrostiniPortForwarder::Protocol>(protocol_type),
      base::BindOnce(&CrostiniHandler::OnPortForwardComplete,
                     callback_weak_ptr_factory_.GetWeakPtr(), callback_id));
}

void CrostiniHandler::HandleDeactivateCrostiniPortForward(
    const base::ListValue& args) {
  const auto& list = args;
  CHECK_EQ(4U, list.size());

  const std::string& callback_id = list[0].GetString();
  guest_os::GuestId container_id(list[1]);
  int port_number = list[2].GetInt();
  int protocol_type = list[3].GetInt();

  if (!crostini::CrostiniFeatures::Get()->IsPortForwardingAllowed(profile_)) {
    OnPortForwardComplete(callback_id, false);
    return;
  }

  crostini::CrostiniPortForwarderFactory::GetForProfile(profile_)
      ->DeactivatePort(
          container_id, port_number,
          static_cast<crostini::CrostiniPortForwarder::Protocol>(protocol_type),
          base::BindOnce(&CrostiniHandler::OnPortForwardComplete,
                         callback_weak_ptr_factory_.GetWeakPtr(), callback_id));
}

void CrostiniHandler::OnPortForwardComplete(std::string callback_id,
                                            bool success) {
  ResolveJavascriptCallback(base::Value(std::move(callback_id)),
                            base::Value(success));
}

void CrostiniHandler::ResolveGetCrostiniDiskInfoCallback(
    std::string callback_id,
    std::unique_ptr<crostini::CrostiniDiskInfo> disk_info) {
  ResolveJavascriptCallback(base::Value(std::move(callback_id)),
                            CrostiniDiskInfoToValue(std::move(disk_info)));
}

void CrostiniHandler::HandleGetCrostiniDiskInfo(const base::ListValue& args) {
  AllowJavascript();
  CHECK_EQ(3U, args.size());
  const std::string& callback_id = args[0].GetString();
  const std::string& vm_name = args[1].GetString();
  bool full_info = args[2].GetBool();
  crostini::disk::GetDiskInfo(
      base::BindOnce(&CrostiniHandler::ResolveGetCrostiniDiskInfoCallback,
                     callback_weak_ptr_factory_.GetWeakPtr(), callback_id),
      profile_, vm_name, full_info);
}

void CrostiniHandler::HandleResizeCrostiniDisk(const base::ListValue& args) {
  CHECK_EQ(3U, args.size());
  const std::string& callback_id = args[0].GetString();
  const std::string& vm_name = args[1].GetString();
  double bytes = args[2].GetDouble();
  crostini::disk::ResizeCrostiniDisk(
      profile_, vm_name, bytes,
      base::BindOnce(&CrostiniHandler::ResolveResizeCrostiniDiskCallback,
                     callback_weak_ptr_factory_.GetWeakPtr(), callback_id));
}

void CrostiniHandler::ResolveResizeCrostiniDiskCallback(std::string callback_id,
                                                        bool succeeded) {
  ResolveJavascriptCallback(base::Value(std::move(callback_id)),
                            base::Value(succeeded));
}

void CrostiniHandler::HandleGetCrostiniActivePorts(
    const base::ListValue& args) {
  AllowJavascript();
  CHECK_EQ(1U, args.size());

  const std::string& callback_id = args[0].GetString();

  ResolveJavascriptCallback(
      base::Value(callback_id),
      crostini::CrostiniPortForwarderFactory::GetForProfile(profile_)
          ->GetActivePorts());
}

void CrostiniHandler::HandleGetCrostiniActiveNetworkInfo(
    const base::ListValue& args) {
  AllowJavascript();
  CHECK_EQ(1U, args.size());

  const std::string& callback_id = args[0].GetString();
  ResolveJavascriptCallback(
      base::Value(callback_id),
      crostini::CrostiniPortForwarderFactory::GetForProfile(profile_)
          ->GetActiveNetworkInfo());
}

void CrostiniHandler::HandleCheckCrostiniIsRunning(
    const base::ListValue& args) {
  AllowJavascript();
  CHECK_EQ(1U, args.size());

  const std::string& callback_id = args[0].GetString();

  ResolveJavascriptCallback(base::Value(callback_id),
                            base::Value(crostini::IsCrostiniRunning(profile_)));
}

void CrostiniHandler::HandleCheckBruschettaIsRunning(
    const base::ListValue& args) {
  AllowJavascript();
  CHECK_EQ(1U, args.size());

  const std::string& callback_id = args[0].GetString();

  ResolveJavascriptCallback(
      base::Value(callback_id),
      base::Value(bruschetta::IsBruschettaRunning(profile_)));
}

void CrostiniHandler::OnContainerStarted(
    const guest_os::GuestId& container_id) {
  if (container_id == crostini::DefaultContainerId()) {
    FireWebUIListener("crostini-status-changed", base::Value(true));
  }
  // After other observers have run, we can send container info.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&CrostiniHandler::HandleRequestContainerInfo,
                                handler_weak_ptr_factory_.GetWeakPtr(),
                                base::ListValue()));
}

void CrostiniHandler::OnContainerShutdown(
    const guest_os::GuestId& container_id) {
  if (container_id == crostini::DefaultContainerId()) {
    FireWebUIListener("crostini-status-changed", base::Value(false));
  }
  // After other observers have run, we can send container info.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&CrostiniHandler::HandleRequestContainerInfo,
                                handler_weak_ptr_factory_.GetWeakPtr(),
                                base::ListValue()));
}

void CrostiniHandler::HandleShutdownCrostini(const base::ListValue& args) {
  CHECK_EQ(0U, args.size());

  crostini::CrostiniManager::GetForProfile(profile_)->StopRunningVms(
      base::DoNothing());
}

void CrostiniHandler::HandleShutdownBruschetta(const base::ListValue& args) {
  CHECK_EQ(0U, args.size());

  bruschetta::BruschettaServiceFactory::GetForProfile(profile_)
      ->StopRunningVms();
}

void CrostiniHandler::HandleRequestContainerInfo(const base::ListValue& args) {
  constexpr char kIdKey[] = "id";
  constexpr char kIpv4Key[] = "ipv4";

  base::ListValue container_info_list;

  // Realistically there should only be either a termina or baguette container.
  std::vector<guest_os::GuestId> containers =
      guest_os::GetContainers(profile_, guest_os::VmType::TERMINA);
  std::vector<guest_os::GuestId> baguette_containers =
      guest_os::GetContainers(profile_, guest_os::VmType::BAGUETTE);
  containers.insert(containers.end(), baguette_containers.begin(),
                    baguette_containers.end());

  for (const auto& container_id : containers) {
    base::DictValue container_info_value;
    container_info_value.Set(kIdKey, container_id.ToDictValue());
    auto info = guest_os::GuestOsSessionTrackerFactory::GetForProfile(profile_)
                    ->GetInfo(container_id);
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

  FireWebUIListener("crostini-container-info", container_info_list);
}

void CrostiniHandler::HandleRequestBruschettaInstallerView(
    const base::ListValue& args) {
  AllowJavascript();
  BruschettaInstallerView::Show(Profile::FromWebUI(web_ui()),
                                CHECK_DEREF(g_browser_process->local_state()),
                                bruschetta::GetBruschettaAlphaId());
}

void CrostiniHandler::HandleRequestBruschettaUninstallerView(
    const base::ListValue& args) {
  AllowJavascript();
  BruschettaUninstallerView::Show(Profile::FromWebUI(web_ui()),
                                  bruschetta::GetBruschettaAlphaId());
}

}  // namespace ash::settings
