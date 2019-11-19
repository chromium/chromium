// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_CROSTINI_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_CROSTINI_HANDLER_H_

#include <vector>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/crostini/crostini_export_import.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/usb/cros_usb_detector.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "chromeos/dbus/session_manager/session_manager_client.h"

class Profile;

namespace crostini {
enum class CrostiniResult;
}

namespace chromeos {
namespace settings {

class CrostiniHandler : public ::settings::SettingsPageUIHandler,
                        public crostini::InstallerViewStatusObserver,
                        public crostini::CrostiniExportImport::Observer,
                        public chromeos::CrosUsbDeviceObserver {
 public:
  explicit CrostiniHandler(Profile* profile);
  ~CrostiniHandler() override;

  // SettingsPageUIHandler
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

 private:
  void HandleRequestCrostiniInstallerView(const base::ListValue* args);
  void HandleRequestRemoveCrostini(const base::ListValue* args);
  // Callback for the "getSharedPathsDisplayText" message.  Converts actual
  // paths in chromeos to values suitable to display to users.
  // E.g. /home/chronos/u-<hash>/Downloads/foo => "Downloads > foo".
  void HandleGetCrostiniSharedPathsDisplayText(const base::ListValue* args);
  // Remove a specified path from being shared.
  void HandleRemoveCrostiniSharedPath(const base::ListValue* args);
  // Returns a list of available USB devices.
  void HandleGetCrostiniSharedUsbDevices(const base::ListValue* args);
  // Set the share state of a USB device.
  void HandleSetCrostiniUsbDeviceShared(const base::ListValue* args);
  // chromeos::SharedUsbDeviceObserver.
  void OnUsbDevicesChanged() override;
  // Export the crostini container.
  void HandleExportCrostiniContainer(const base::ListValue* args);
  // Import the crostini container.
  void HandleImportCrostiniContainer(const base::ListValue* args);
  // Handle a request for the CrostiniInstallerView status.
  void HandleCrostiniInstallerStatusRequest(const base::ListValue* args);
  // Handle the CrostiniInstallerView opening/closing.
  void OnCrostiniInstallerViewStatusChanged(bool open) override;
  // Handle a request for the CrostiniExportImport operation status.
  void HandleCrostiniExportImportOperationStatusRequest(
      const base::ListValue* args);
  // CrostiniExportImport::Observer:
  void OnCrostiniExportImportOperationStatusChanged(bool in_progress) override;
  // Handle a request for querying status of ARC adb sideloading.
  void HandleQueryArcAdbRequest(const base::ListValue* args);
  // Handle a request for enabling adb sideloading in ARC.
  void HandleEnableArcAdbRequest(const base::ListValue* args);
  // Handle a request for disabling adb sideloading in ARC.
  void HandleDisableArcAdbRequest(const base::ListValue* args);
  // Callback of HandleQueryArcAdbRequest.
  void OnQueryAdbSideload(
      SessionManagerClient::AdbSideloadResponseCode response_code,
      bool enabled);
  // Returns whether the current user can change adb sideloading configuration
  // on current device.
  bool CheckEligibilityToChangeArcAdbSideloading() const;

  Profile* profile_;
  // weak_ptr_factory_ should always be last member.
  base::WeakPtrFactory<CrostiniHandler> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CrostiniHandler);
};

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_CROSTINI_HANDLER_H_
