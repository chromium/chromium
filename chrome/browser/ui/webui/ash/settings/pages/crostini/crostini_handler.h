// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_CROSTINI_CROSTINI_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_CROSTINI_CROSTINI_HANDLER_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/crostini/crostini_export_import.h"
#include "chrome/browser/ash/crostini/crostini_manager.h"
#include "chrome/browser/ash/crostini/crostini_port_forwarder.h"
#include "chrome/browser/ash/guest_os/guest_id.h"
#include "chrome/browser/ash/guest_os/guest_os_session_tracker.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/services/app_service/public/cpp/intent.h"
#include "content/public/browser/web_ui_message_handler.h"

class Profile;

namespace crostini {
enum class CrostiniResult;
struct CrostiniDiskInfo;
}  // namespace crostini

namespace ash::settings {

class CrostiniHandler : public content::WebUIMessageHandler,
                        public crostini::CrostiniDialogStatusObserver,
                        public crostini::CrostiniExportImport::Observer,
                        public crostini::CrostiniPortForwarder::Observer,
                        public guest_os::ContainerStartedObserver,
                        public crostini::ContainerShutdownObserver {
 public:
  explicit CrostiniHandler(Profile* profile);

  CrostiniHandler(const CrostiniHandler&) = delete;
  CrostiniHandler& operator=(const CrostiniHandler&) = delete;

  ~CrostiniHandler() override;

  // content::WebUIMessageHandler
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

 private:
  void HandleRequestCrostiniInstallerView(const base::ListValue& args);
  void HandleRequestRemoveCrostini(const base::ListValue& args);
  // Export the crostini container.
  void HandleExportCrostiniContainer(const base::ListValue& args);
  // Import the crostini container.
  void HandleImportCrostiniContainer(const base::ListValue& args);
  // Export a guest disk image.
  void HandleExportDiskImage(const base::ListValue& args);
  // Import a guest disk image.
  void HandleImportDiskImage(const base::ListValue& args);
  // Handle a request for the CrostiniInstallerView status.
  void HandleCrostiniInstallerStatusRequest(const base::ListValue& args);
  // crostini::CrostiniDialogStatusObserver
  void OnCrostiniDialogStatusChanged(crostini::DialogType dialog_type,
                                     bool open) override;
  // Handle a request for the CrostiniExportImport operation status.
  void HandleCrostiniExportImportOperationStatusRequest(
      const base::ListValue& args);
  // CrostiniExportImport::Observer:
  void OnCrostiniExportImportOperationStatusChanged(bool in_progress) override;
  // Handle a request for querying status of ARC adb sideloading.
  void HandleQueryArcAdbRequest(const base::ListValue& args);
  // Handle a request for enabling adb sideloading in ARC.
  void HandleEnableArcAdbRequest(const base::ListValue& args);
  // Called after establishing whether enabling adb sideloading is allowed for
  // the user and device
  void OnCanEnableArcAdbSideloading(bool can_change_adb_sideloading);
  // Handle a request for disabling adb sideloading in ARC.
  void HandleDisableArcAdbRequest(const base::ListValue& args);
  // Called after establishing whether disabling adb sideloading is allowed for
  // the user and device
  void OnCanDisableArcAdbSideloading(bool can_change_adb_sideloading);
  // Launch the Crostini terminal, with |intent| specifying any non-default
  // container id.
  void LaunchTerminal(apps::IntentPtr intent);
  // Callback of HandleQueryArcAdbRequest.
  void OnQueryAdbSideload(
      SessionManagerClient::AdbSideloadResponseCode response_code,
      bool enabled);
  // Handles a request for forwarding a new port.
  void HandleAddCrostiniPortForward(const base::ListValue& args);
  // Handles a request for removing one port.
  void HandleRemoveCrostiniPortForward(const base::ListValue& args);
  // Handles a request for removing all ports.
  void HandleRemoveAllCrostiniPortForwards(const base::ListValue& args);
  // CrostiniPortForwarder::Observer.
  void OnActivePortsChanged(const base::ListValue& activePorts) override;
  void OnActiveNetworkChanged(const base::Value& interface,
                              const base::Value& ipAddress) override;
  // Handles a request for activating an existing port.
  void HandleActivateCrostiniPortForward(const base::ListValue& args);
  // Handles a request for deactivating an existing port.
  void HandleDeactivateCrostiniPortForward(const base::ListValue& args);
  // Callback of port forwarding requests.
  void OnPortForwardComplete(std::string callback_id, bool success);
  // Fetches disk info for a VM, can be slow (seconds).
  void HandleGetCrostiniDiskInfo(const base::ListValue& args);
  void ResolveGetCrostiniDiskInfoCallback(
      std::string callback_id,
      std::unique_ptr<crostini::CrostiniDiskInfo> disk_info);
  // Handles a request to resize a Crostini disk.
  void HandleResizeCrostiniDisk(const base::ListValue& args);
  void ResolveResizeCrostiniDiskCallback(std::string callback_id,
                                         bool succeeded);
  // Returns a list of currently forwarded ports.
  void HandleGetCrostiniActivePorts(const base::ListValue& args);
  // Returns the current active network for forwarded ports.
  void HandleGetCrostiniActiveNetworkInfo(const base::ListValue& args);
  // Checks if Crostini is running.
  void HandleCheckCrostiniIsRunning(const base::ListValue& args);
  // Checks if Bruschetta is running.
  void HandleCheckBruschettaIsRunning(const base::ListValue& args);
  // guest_os::ContainerStartedObserver
  void OnContainerStarted(const guest_os::GuestId& container_id) override;
  // crostini::ContainerShutdownObserver
  void OnContainerShutdown(const guest_os::GuestId& container_id) override;
  // Handles a request to shut down Crostini.
  void HandleShutdownCrostini(const base::ListValue& args);
  // Handles a request to shut down Bruschetta.
  void HandleShutdownBruschetta(const base::ListValue& args);
  // Handle a request for checking permission for changing ARC adb sideloading.
  void HandleCanChangeArcAdbSideloadingRequest(const base::ListValue& args);
  // Get permission of changing ARC adb sideloading
  void FetchCanChangeAdbSideloading();
  // Callback of FetchCanChangeAdbSideloading.
  void OnCanChangeArcAdbSideloading(bool can_change_arc_adb_sideloading);
  // Handle a request for the running info of all known containers
  void HandleRequestContainerInfo(const base::ListValue& args);
  // Handle a request to set the badge color for a container
  void HandleSetContainerBadgeColor(const base::ListValue& args);
  // Handle a request to show the installer for Bruschetta
  void HandleRequestBruschettaInstallerView(const base::ListValue& args);
  // Handle a request to start uninstalling Bruschetta
  void HandleRequestBruschettaUninstallerView(const base::ListValue& args);

  raw_ptr<Profile> profile_;
  base::CallbackListSubscription adb_sideloading_device_policy_subscription_;
  PrefChangeRegistrar pref_change_registrar_;

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
