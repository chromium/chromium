// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/cloud_upload/cloud_upload_dialog.h"

#include "ash/public/cpp/new_window_delegate.h"
#include "base/callback.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/ui/webui/chromeos/cloud_upload/cloud_upload_handler.h"
#include "chrome/common/webui_url_constants.h"

namespace chromeos::cloud_upload {
namespace {

void OpenUrlOnUploadDone(GURL hosted_url) {
  if (!hosted_url.is_valid()) {
    LOG(ERROR) << "Invalid hosted URL";
    return;
  }
  if (!ash::NewWindowDelegate::GetPrimary()) {
    LOG(ERROR) << "Failed to get window delegate";
    return;
  }
  ash::NewWindowDelegate::GetPrimary()->OpenUrl(
      hosted_url, ash::NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      ash::NewWindowDelegate::Disposition::kNewForegroundTab);
}

void OnUploadActionReceived(Profile* profile,
                            const storage::FileSystemURL& file_url,
                            const std::string& action) {
  if (action == kUserActionUpload) {
    CloudUploadHandler::UploadToCloud(profile, file_url,
                                      base::BindOnce(&OpenUrlOnUploadDone));
  }
}

}  // namespace

// static
bool CloudUploadDialog::Show(
    Profile* profile,
    const std::vector<storage::FileSystemURL>& file_urls) {
  // Allow no more than one upload dialog at a time. In the case of multiple
  // upload requests, they should either be handled simultaneously or queued.
  if (SystemWebDialogDelegate::HasInstance(
          GURL(chrome::kChromeUICloudUploadURL))) {
    return false;
  }

  DCHECK(!file_urls.empty());
  // TODO(crbug.com/1336924) Add support for multi-file selection.
  const storage::FileSystemURL file_url = file_urls[0];

  // The pointer is managed by an instance of `views::WebDialogView` and removed
  // in `SystemWebDialogDelegate::OnDialogClosed`.
  CloudUploadDialog* dialog = new CloudUploadDialog(
      file_url, base::BindOnce(&OnUploadActionReceived, profile, file_url));

  dialog->ShowSystemDialog();
  return true;
}

void CloudUploadDialog::OnDialogClosed(const std::string& json_retval) {
  if (callback_) {
    std::move(callback_).Run(json_retval);
  }
  SystemWebDialogDelegate::OnDialogClosed(json_retval);
}

CloudUploadDialog::CloudUploadDialog(const storage::FileSystemURL& file_url,
                                     UploadRequestCallback callback)
    : SystemWebDialogDelegate(GURL(chrome::kChromeUICloudUploadURL),
                              std::u16string() /* title */),
      file_url_(file_url),
      callback_(std::move(callback)) {}

CloudUploadDialog::~CloudUploadDialog() = default;

std::string CloudUploadDialog::GetDialogArgs() const {
  base::DictionaryValue args;
  args.SetKey("fileName", base::Value(file_url_.path().BaseName().value()));
  std::string json;
  base::JSONWriter::Write(args, &json);
  return json;
}

bool CloudUploadDialog::ShouldShowCloseButton() const {
  return false;
}

}  // namespace chromeos::cloud_upload
