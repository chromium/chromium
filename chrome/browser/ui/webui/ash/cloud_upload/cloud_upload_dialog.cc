// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_dialog.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/ash/file_manager/file_tasks.h"
#include "chrome/browser/ash/file_manager/open_with_browser.h"
#include "chrome/browser/ash/file_system_provider/mount_path_util.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/drive_upload_handler.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/one_drive_upload_handler.h"
#include "chrome/common/webui_url_constants.h"

namespace ash::cloud_upload {
namespace {

using file_manager::file_tasks::kDriveTaskResultMetricName;
using file_manager::file_tasks::OfficeTaskResult;

const char kOpenWebActionId[] = "OPEN_WEB";

void OpenDriveUrl(const GURL& url) {
  if (url.is_empty()) {
    UMA_HISTOGRAM_ENUMERATION(kDriveTaskResultMetricName,
                              OfficeTaskResult::FAILED);
    return;
  }
  UMA_HISTOGRAM_ENUMERATION(kDriveTaskResultMetricName,
                            OfficeTaskResult::MOVED);
  file_manager::util::OpenNewTabForHostedOfficeFile(url);
}

// Copied from file_tasks.cc.
void OpenODFSUrl(const storage::FileSystemURL& uploaded_file_url) {
  if (!uploaded_file_url.is_valid()) {
    LOG(ERROR) << "Invalid uploaded file URL";
    return;
  }
  ash::file_system_provider::util::FileSystemURLParser parser(
      uploaded_file_url);
  if (!parser.Parse()) {
    LOG(ERROR) << "Path not in FSP";
    return;
  }

  parser.file_system()->ExecuteAction(
      {parser.file_path()}, kOpenWebActionId,
      base::BindOnce([](base::File::Error result) {
        if (result != base::File::Error::FILE_OK) {
          LOG(ERROR) << "Error executing action: " << result;
        }
      }));
}

void OnUploadActionReceived(Profile* profile,
                            const storage::FileSystemURL& file_url,
                            const UploadType upload_type,
                            const std::string& action) {
  if (action == kUserActionUpload) {
    switch (upload_type) {
      case UploadType::kOneDrive:
        OneDriveUploadHandler::Upload(profile, file_url,
                                      base::BindOnce(&OpenODFSUrl));
        break;
      case UploadType::kDrive:
        DriveUploadHandler::Upload(profile, file_url,
                                   base::BindOnce(&OpenDriveUrl));
        break;
    }
  } else if (action == kUserActionCancel) {
    UMA_HISTOGRAM_ENUMERATION(kDriveTaskResultMetricName,
                              OfficeTaskResult::CANCELLED);
  }
}

}  // namespace

bool UploadAndOpen(Profile* profile,
                   const std::vector<storage::FileSystemURL>& file_urls,
                   const UploadType upload_type,
                   bool show_dialog) {
  if (show_dialog) {
    return CloudUploadDialog::Show(profile, file_urls, upload_type);
  }

  bool empty_selection = file_urls.empty();
  DCHECK(!empty_selection);
  if (empty_selection) {
    return false;
  }
  // TODO(crbug.com/1336924) Add support for multi-file selection.
  OnUploadActionReceived(profile, file_urls[0], upload_type, kUserActionUpload);
  return true;
}

// static
bool CloudUploadDialog::Show(
    Profile* profile,
    const std::vector<storage::FileSystemURL>& file_urls,
    UploadType upload_type) {
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
      file_url, upload_type,
      base::BindOnce(&OnUploadActionReceived, profile, file_url, upload_type));

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
                                     const UploadType upload_type,
                                     UploadRequestCallback callback)
    : SystemWebDialogDelegate(GURL(chrome::kChromeUICloudUploadURL),
                              std::u16string() /* title */),
      file_url_(file_url),
      upload_type_(upload_type),
      callback_(std::move(callback)) {}

CloudUploadDialog::~CloudUploadDialog() = default;

std::string CloudUploadDialog::GetDialogArgs() const {
  base::DictionaryValue args;
  args.SetKey("fileName", base::Value(file_url_.path().BaseName().value()));
  switch (upload_type_) {
    case UploadType::kOneDrive:
      args.SetKey("uploadType", base::Value("OneDrive"));
      break;
    case UploadType::kDrive:
      args.SetKey("uploadType", base::Value("Drive"));
  }
  std::string json;
  base::JSONWriter::Write(args, &json);
  return json;
}

bool CloudUploadDialog::ShouldShowCloseButton() const {
  return false;
}

}  // namespace ash::cloud_upload
