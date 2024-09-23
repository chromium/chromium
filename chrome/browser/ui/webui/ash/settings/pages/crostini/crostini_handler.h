// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_CROSTINI_CROSTINI_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_CROSTINI_CROSTINI_HANDLER_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/crostini/crostini_export_import.h"
#include "chrome/browser/ash/crostini/crostini_file_selector.h"
#include "chrome/browser/ash/crostini/crostini_manager.h"
#include "chrome/browser/ash/crostini/crostini_port_forwarder.h"
#include "chrome/browser/ash/guest_os/guest_id.h"
#include "chrome/browser/ash/guest_os/guest_os_session_tracker.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/services/app_service/public/cpp/intent.h"

class Profile;

namespace crostini {
enum class CrostiniResult;
struct CrostiniDiskInfo;
}  // namespace crostini

namespace ash::settings {

class CrostiniHandler : public ::settings::SettingsPageUIHandler,
                        public crostini::CrostiniDialogStatusObserver,
                        public crostini::CrostiniExportImport::Observer,
                        public crostini::CrostiniContainerPropertiesObserver,
                        public crostini::CrostiniPortForwarder::Observer,
                        public guest_os::ContainerStartedObserver,
                        public crostini::ContainerShutdownObserver {
 public:
  explicit CrostiniHandler(Profile* profile);

  CrostiniHandler(const CrostiniHandler&) = delete;
  CrostiniHandler& operator=(const CrostiniHandler&) = delete;

  ~CrostiniHandler() override;

  // SettingsPageUIHandler
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

 private:
  void HandleRequestCrostiniInstallerView(const base::Value::List& args);
  void HandleRequestRemoveCrostini(const base::Value::List& args);
  // Export the crostini container.
  void HandleExportCrostiniContainer(const base::Value::List& args);
  // Import the crostini container.
  void HandleImportCrostiniContainer(const base::Value::List& args);
  // Handle a request for the CrostiniInstallerView status.
  void HandleCrostiniInstallerStatusRequest(const base::Value::List& args);
  // crostini::CrostiniDialogStatusObserver
  void OnCrostiniDialogStatusChanged(crostini::DialogType dialog_type,
                                     bool open) override;
  // crostini::CrostiniContainerPropertiesObserver
  void OnContainerOsReleaseChanged(const guest_os::GuestId& container_id,
                                   bool can_upgrade) override;
  // Handle a request for the CrostiniExportImport operation status.
  void HandleCrostiniExportImportOperationStatusRequest(
      const base::Value::List& args);
  // CrostiniExportImport::Observer:
  void OnCrostiniExportImportOperationStatusChanged(bool in_progress) override;
  // Handle a request for querying status of ARC adb sideloading.
  void HandleQueryArcAdbRequest(const base::Value::List& args);
  // Handle a request for enabling adb sideloading in ARC.
  void HandleEnableArcAdbRequest(const base::Value::List& args);
  // Called after establishing whether enabling adb sideloading is allowed for
  // the user and device
  void OnCanEnableArcAdbSideloading(bool can_change_adb_sideloading);
  // Handle a request for disabling adb sideloading in ARC.
  void HandleDisableArcAdbRequest(const base::Value::List& args);
  // Called after establishing whether disabling adb sideloading is allowed for
  // the user and device
  void OnCanDisableArcAdbSideloading(bool can_change_adb_sideloading);
  // Launch the Crostini terminal, with |intent| specifying any non-default
  // container id.
  void LaunchTerminal(apps::IntentPtr intent);
  // Handle a request for showing the container upgrade view.
  void HandleRequestContainerUpgradeView(const base::Value::List& args);
  // Callback of HandleQueryArcAdbRequest.
  void OnQueryAdbSideload(
      SessionManagerClient::AdbSideloadResponseCode response_code,
      bool enabled);
  // Handle a request for the CrostiniUpgraderDialog status.
  void HandleCrostiniUpgraderDialogStatusRequest(const base::Value::List& args);
  // Handle a request for the availability of a container upgrade.
  void HandleCrostiniContainerUpgradeAvailableRequest(
      const base::Value::List& args);
  // Handles a request for forwarding a new port.
  void HandleAddCrostiniPortForward(const base::Value::List& args);
  // Handles a request for removing one port.
  void HandleRemoveCrostiniPortForward(const base::Value::List& args);
  // Handles a request for removing all ports.
  void HandleRemoveAllCrostiniPortForwards(const base::Value::List& args);
  // CrostiniPortForwarder::Observer.
  void OnActivePortsChanged(const base::Value::List& activePorts) override;
  void OnActiveNetworkChanged(const base::Value& interface,
                              const base::Value& ipAddress) override;
  // Handles a request for activating an existing port.
  void HandleActivateCrostiniPortForward(const base::Value::List& args);
  // Handles a request for deactivating an existing port.
  void HandleDeactivateCrostiniPortForward(const base::Value::List& args);
  // Callback of port forwarding requests.
  void OnPortForwardComplete(std::string callback_id, bool success);
  // Fetches disk info for a VM, can be slow (seconds).
  void HandleGetCrostiniDiskInfo(const base::Value::List& args);
  void ResolveGetCrostiniDiskInfoCallback(
      const std::string& callback_id,
      std::unique_ptr<crostini::CrostiniDiskInfo> disk_info);
  // Handles a request to resize a Crostini disk.
  void HandleResizeCrostiniDisk(const base::Value::List& args);
  void ResolveResizeCrostiniDiskCallback(const std::string& callback_id,
                                         bool succeeded);
  // Returns a list of currently forwarded ports.
  void HandleGetCrostiniActivePorts(const base::Value::List& args);
  // Returns the current active network for forwarded ports.
  void HandleGetCrostiniActiveNetworkInfo(const base::Value::List& args);
  // Checks if Crostini is running.
  void HandleCheckCrostiniIsRunning(const base::Value::List& args);
  // Checks if Bruschetta is running.
  void HandleCheckBruschettaIsRunning(const base::Value::List& args);
  // guest_os::ContainerStartedObserver
  void OnContainerStarted(const guest_os::GuestId& container_id) override;
  // crostini::ContainerShutdownObserver
  void OnContainerShutdown(const guest_os::GuestId& container_id) override;
  // Handles a request to shut down Crostini.
  void HandleShutdownCrostini(const base::Value::List& args);
  // Handles a request to shut down Bruschetta.
  void HandleShutdownBruschetta(const base::Value::List& args);
  // Handle a request for checking permission for changing ARC adb sideloading.
  void HandleCanChangeArcAdbSideloadingRequest(const base::Value::List& args);
  // Get permission of changing ARC adb sideloading
  void FetchCanChangeAdbSideloading();
  // Callback of FetchCanChangeAdbSideloading.
  void OnCanChangeArcAdbSideloading(bool can_change_arc_adb_sideloading);
  // Handle a request for creating a container
  void HandleCreateContainer(const base::Value::List& args);
  // Callback of HandleCreateContainer
  void OnContainerCreated(guest_os::GuestId container_id,
                          crostini::CrostiniResult result);
  // Handle a request for deleting a container
  void HandleDeleteContainer(const base::Value::List& args);
  // Handle a request for the running info of all known containers
  void HandleRequestContainerInfo(const base::Value::List& args);
  // Handle a request to set the badge color for a container
  void HandleSetContainerBadgeColor(const base::Value::List& args);
  // Handle a request to stop a running lxd container
  void HandleStopContainer(const base::Value::List& args);

  // Handle a request to open a file selector
  void HandleOpenContainerFileSelector(const base::Value::List& args);
  // Callback for CrostiniFileSelector
  void OnContainerFileSelected(const std::string& callback_id,
                               const base::FilePath& path);

  // Handle a request for the shared vmdevice info of all known containers
  void HandleRequestSharedVmDevices(const base::Value::List& args);
  // Handle a request to query the sharing status of a VmDevice
  void HandleIsVmDeviceShared(const base::Value::List& args);
  // Handle a request to set the sharing status of a VmDevice
  void HandleSetVmDeviceShared(const base::Value::List& args);
  // Handle a request to show the installer for Bruschetta
  void HandleRequestBruschettaInstallerView(const base::Value::List& args);
  // Handle a request to start uninstalling Bruschetta
  void HandleRequestBruschettaUninstallerView(const base::Value::List& args);

  raw_ptr<Profile> profile_;
  base::CallbackListSubscription adb_sideloading_device_policy_subscription_;
  PrefChangeRegistrar pref_change_registrar_;
  std::unique_ptr<crostini::CrostiniFileSelector> file_selector_;

  // |handler_weak_ptr_factory_| is used for callbacks handling messages from
  // the WebUI page, and certain observers. These callbacks usually have the
  // same lifecycle as CrostiniHandler.
  base::WeakPtrFactory<CrostiniHandler> handler_weak_ptr_factory_{this};
  // |callback_weak_ptr_factory_| is used for callbacks passed into crostini
  // functions, which run JS functions/callbacks. However, running JS after
  // being disallowed (i.e. after the user closes the WebUI page) results in a
  // CHECK-fail. To avoid CHECK failing, the WeakPtrs are invalidated in
  // OnJavascriptDisallowed().
  base::WeakPtrFactory<CrostiniHandler> callback_weak_ptr_factory_{this};
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_CROSTINI_CROSTINI_HANDLER_H_
