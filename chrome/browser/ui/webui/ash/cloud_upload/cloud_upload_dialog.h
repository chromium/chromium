// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_CLOUD_UPLOAD_CLOUD_UPLOAD_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_CLOUD_UPLOAD_CLOUD_UPLOAD_DIALOG_H_

#include <vector>

#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload.mojom.h"
#include "chrome/browser/ui/webui/ash/system_web_dialog_delegate.h"
#include "storage/browser/file_system/file_system_url.h"
#include "ui/gfx/geometry/size.h"

class Profile;

namespace ash::cloud_upload {

// The string conversions of ash::cloud_upload::mojom::UserAction.
const char kUserActionCancel[] = "cancel";
const char kUserActionSetUpGoogleDrive[] = "setup-drive";
const char kUserActionSetUpOneDrive[] = "setup-onedrive";
const char kUserActionUploadToGoogleDrive[] = "upload-drive";
const char kUserActionUploadToOneDrive[] = "upload-onedrive";
const char kUserActionConfirmOrUploadToGoogleDrive[] =
    "confirm-or-upload-google-drive";
const char kUserActionConfirmOrUploadToOneDrive[] =
    "confirm-or-upload-onedrive";

// Either OneDrive for the Office PWA or Drive for Drive Web editing.
enum class CloudProvider {
  kGoogleDrive,
  kOneDrive,
};

// Opens the `file_urls` from the `cloud_provider`. Runs setup for for Office
// files if it has not been run before. Uploads the files to the cloud if
// needed.
bool OpenFilesWithCloudProvider(
    Profile* profile,
    const std::vector<storage::FileSystemURL>& file_urls,
    const CloudProvider cloud_provider);

// Open office files if they are in the correct cloud already.
// Otherwise move the files before opening.
void OpenOrMoveFiles(Profile* profile,
                     const std::vector<storage::FileSystemURL>& file_urls,
                     const CloudProvider cloud_provider);

// Defines the web dialog used to help users upload Office files to the cloud.
class CloudUploadDialog : public SystemWebDialogDelegate {
 public:
  using UploadRequestCallback =
      base::OnceCallback<void(const std::string& action)>;

  CloudUploadDialog(const CloudUploadDialog&) = delete;
  CloudUploadDialog& operator=(const CloudUploadDialog&) = delete;

  // Creates and shows a new dialog for the cloud upload workflow. Returns true
  // if a new dialog has been effectively created.
  static bool Show(Profile* profile,
                   const std::vector<storage::FileSystemURL>& file_urls,
                   const mojom::DialogPage dialog_page);

  void OnDialogShown(content::WebUI* webui) override;
  void OnDialogClosed(const std::string& json_retval) override;

 protected:
  CloudUploadDialog(mojom::DialogArgsPtr args,
                    UploadRequestCallback callback,
                    const mojom::DialogPage dialog_page);
  ~CloudUploadDialog() override;
  bool ShouldCloseDialogOnEscape() const override;
  bool ShouldShowCloseButton() const override;
  void GetDialogSize(gfx::Size* size) const override;

 private:
  mojom::DialogArgsPtr dialog_args_;
  UploadRequestCallback callback_;
  mojom::DialogPage dialog_page_;
};

}  // namespace ash::cloud_upload

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_CLOUD_UPLOAD_CLOUD_UPLOAD_DIALOG_H_
