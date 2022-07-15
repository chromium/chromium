// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/cloud_upload/cloud_upload_dialog.h"

#include "base/logging.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

namespace chromeos::cloud_upload {

// static
bool CloudUploadDialog::Show() {
  // Allow no more than one upload dialog at a time. In the case of multiple
  // upload requests, they should either be handled simultaneously or queued.
  if (SystemWebDialogDelegate::HasInstance(
          GURL(chrome::kChromeUICloudUploadURL))) {
    return false;
  }

  // The pointer is managed by an instance of `views::WebDialogView` and removed
  // in `SystemWebDialogDelegate::OnDialogClosed`.
  CloudUploadDialog* dialog = new CloudUploadDialog();
  dialog->ShowSystemDialog();
  return true;
}

CloudUploadDialog::CloudUploadDialog()
    : SystemWebDialogDelegate(GURL(chrome::kChromeUICloudUploadURL),
                              std::u16string() /* title */) {}

CloudUploadDialog::~CloudUploadDialog() = default;

bool CloudUploadDialog::ShouldShowCloseButton() const {
  return false;
}

CloudUploadDialogUI::CloudUploadDialogUI(content::WebUI* web_ui)
    : ui::WebDialogUI(web_ui) {
  content::WebUIDataSource::CreateAndAdd(Profile::FromWebUI(web_ui),
                                         chrome::kChromeUICloudUploadHost);
}

CloudUploadDialogUI::~CloudUploadDialogUI() = default;

}  // namespace chromeos::cloud_upload
