// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_CLOUD_UPLOAD_CLOUD_UPLOAD_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_CLOUD_UPLOAD_CLOUD_UPLOAD_DIALOG_H_

#include "chrome/browser/ui/webui/chromeos/system_web_dialog_delegate.h"

namespace chromeos::cloud_upload {

// Defines the web dialog used to help users upload Office files to the cloud.
class CloudUploadDialog : public SystemWebDialogDelegate {
 public:
  CloudUploadDialog(const CloudUploadDialog&) = delete;
  CloudUploadDialog& operator=(const CloudUploadDialog&) = delete;

  // Creates and shows a new dialog for the cloud upload workflow. Returns true
  // if a new dialog has been effectively created.
  static bool Show();

 protected:
  CloudUploadDialog();
  ~CloudUploadDialog() override;
  bool ShouldShowCloseButton() const override;
};

}  // namespace chromeos::cloud_upload

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_CLOUD_UPLOAD_CLOUD_UPLOAD_DIALOG_H_
