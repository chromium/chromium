// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_ASH_FILES_PAGE_GOOGLE_DRIVE_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_ASH_FILES_PAGE_GOOGLE_DRIVE_PAGE_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ui/webui/settings/ash/files_page/mojom/google_drive_handler.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class Profile;

namespace ash::settings {

// ChromeOS "Google Drive" settings page UI handler.
class GoogleDrivePageHandler : public google_drive::mojom::PageHandler,
                               public drive::DriveIntegrationServiceObserver {
 public:
  GoogleDrivePageHandler(
      mojo::PendingReceiver<google_drive::mojom::PageHandler> receiver,
      mojo::PendingRemote<google_drive::mojom::Page> page,
      Profile* profile);

  GoogleDrivePageHandler(const GoogleDrivePageHandler&) = delete;
  GoogleDrivePageHandler& operator=(const GoogleDrivePageHandler&) = delete;

  ~GoogleDrivePageHandler() override;

 private:
  // google_drive::mojom::PageHandler:
  void CalculateRequiredSpace() override;
  void GetTotalPinnedSize(GetTotalPinnedSizeCallback callback) override;
  void ClearPinnedFiles(ClearPinnedFilesCallback callback) override;
  void RecordBulkPinningEnabledMetric() override;

  // drive::DriveIntegrationServiceObserver
  void OnBulkPinProgress(const drivefs::pinning::Progress& progress) override;

  void NotifyServiceUnavailable();
  void NotifyProgress(const drivefs::pinning::Progress& progress);

  void OnGetTotalPinnedSize(GetTotalPinnedSizeCallback callback, int64_t size);
  void OnClearPinnedFiles(ClearPinnedFilesCallback callback,
                          drive::FileError error);

  drivefs::pinning::PinManager* GetPinManager();
  drive::DriveIntegrationService* GetDriveService();
  raw_ptr<Profile> profile_;

  mojo::Remote<google_drive::mojom::Page> page_;
  mojo::Receiver<google_drive::mojom::PageHandler> receiver_{this};

  base::WeakPtrFactory<GoogleDrivePageHandler> weak_ptr_factory_{this};
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_ASH_FILES_PAGE_GOOGLE_DRIVE_PAGE_HANDLER_H_
