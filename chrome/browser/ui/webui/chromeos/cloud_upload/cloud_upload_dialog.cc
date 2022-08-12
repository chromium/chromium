// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/cloud_upload/cloud_upload_dialog.h"

#include "base/callback.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "chrome/common/webui_url_constants.h"

namespace chromeos::cloud_upload {
namespace {

void OnUploadActionReceived(const std::string& action) {
  LOG(ERROR) << "ACTION: " << action;
}

}  // namespace

// static
bool CloudUploadDialog::Show(
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
  CloudUploadDialog* dialog =
      new CloudUploadDialog(file_url, base::BindOnce(&OnUploadActionReceived));

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
  args.SetKey("path", base::Value(file_url_.path().BaseName().value()));
  std::string json;
  base::JSONWriter::Write(args, &json);
  return json;
}

bool CloudUploadDialog::ShouldShowCloseButton() const {
  return false;
}

}  // namespace chromeos::cloud_upload
