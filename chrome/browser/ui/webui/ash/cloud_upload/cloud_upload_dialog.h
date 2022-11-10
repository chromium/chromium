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

// Either OneDrive for the Office PWA or Drive for Web Drive editing.
enum class UploadType {
  kOneDrive,
  kDrive,
};

// The string conversions of ash::cloud_upload::mojom::UserAction.
const char kUserActionCancel[] = "cancel";
const char kUserActionUpload[] = "upload";

// Initiates the upload workflow.
bool UploadAndOpen(Profile* profile,
                   const std::vector<storage::FileSystemURL>& file_urls,
                   const mojom::CloudProvider cloud_provider,
                   bool show_dialog);

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
                   const mojom::CloudProvider cloud_provider);

  void OnDialogShown(content::WebUI* webui) override;
  void OnDialogClosed(const std::string& json_retval) override;

 protected:
  CloudUploadDialog(mojom::DialogArgsPtr args, UploadRequestCallback callback);
  ~CloudUploadDialog() override;
  bool ShouldShowCloseButton() const override;
  void GetDialogSize(gfx::Size* size) const override;

 private:
  mojom::DialogArgsPtr dialog_args_;
  UploadRequestCallback callback_;
};

}  // namespace ash::cloud_upload

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_CLOUD_UPLOAD_CLOUD_UPLOAD_DIALOG_H_
