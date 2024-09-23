// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_FILES_ONE_DRIVE_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_FILES_ONE_DRIVE_PAGE_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/file_system_provider/observer.h"
#include "chrome/browser/ui/webui/ash/settings/pages/files/mojom/one_drive_handler.mojom.h"
#include "components/prefs/pref_change_registrar.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class Profile;

namespace ash::settings {

// Page handler for settings related to OneDrive.
class OneDrivePageHandler : public one_drive::mojom::PageHandler,
                            public ash::file_system_provider::Observer {
 public:
  OneDrivePageHandler(
      mojo::PendingReceiver<one_drive::mojom::PageHandler> receiver,
      mojo::PendingRemote<one_drive::mojom::Page> page,
      Profile* profile);

  OneDrivePageHandler(const OneDrivePageHandler&) = delete;
  OneDrivePageHandler& operator=(const OneDrivePageHandler&) = delete;

  ~OneDrivePageHandler() override;

 private:
  // one_drive::mojom::PageHandler:
  void GetUserEmailAddress(GetUserEmailAddressCallback callback) override;
  void ConnectToOneDrive(ConnectToOneDriveCallback callback) override;
  void DisconnectFromOneDrive(DisconnectFromOneDriveCallback callback) override;
  void OpenOneDriveFolder(OpenOneDriveFolderCallback callback) override;

  // ash::file_system_provider::Observer overrides.
  void OnProvidedFileSystemMount(
      const ash::file_system_provider::ProvidedFileSystemInfo& file_system_info,
      ash::file_system_provider::MountContext context,
      base::File::Error error) override;
  void OnProvidedFileSystemUnmount(
      const ash::file_system_provider::ProvidedFileSystemInfo& file_system_info,
      base::File::Error error) override;

  void OnAllowUserToRemoveODFSChanged();

  raw_ptr<Profile> profile_;

  // The registrar used to watch prefs changes.
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  mojo::Remote<one_drive::mojom::Page> page_;
  mojo::Receiver<one_drive::mojom::PageHandler> receiver_{this};
  base::WeakPtrFactory<OneDrivePageHandler> weak_ptr_factory_{this};
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_FILES_ONE_DRIVE_PAGE_HANDLER_H_
