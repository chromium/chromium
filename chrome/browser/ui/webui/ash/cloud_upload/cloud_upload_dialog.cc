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
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload.mojom-forward.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload.mojom-shared.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_ui.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/drive_upload_handler.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/one_drive_upload_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "ui/gfx/geometry/size.h"

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

void OnCloudSetupComplete(Profile* profile,
                          const std::vector<storage::FileSystemURL>& file_urls,
                          const mojom::CloudProvider cloud_provider,
                          const std::string& action) {
  if (action == kUserActionUpload) {
    for (const auto& file_url : file_urls) {
      switch (cloud_provider) {
        case mojom::CloudProvider::kOneDrive:
          OneDriveUploadHandler::Upload(profile, file_url,
                                        base::BindOnce(&OpenODFSUrl));
          break;
        case mojom::CloudProvider::kGoogleDrive:
          DriveUploadHandler::Upload(profile, file_url,
                                     base::BindOnce(&OpenDriveUrl));
          break;
      }
    }
  } else if (action == kUserActionCancel) {
    UMA_HISTOGRAM_ENUMERATION(kDriveTaskResultMetricName,
                              OfficeTaskResult::CANCELLED);
  }
}

}  // namespace

bool UploadAndOpen(Profile* profile,
                   const std::vector<storage::FileSystemURL>& file_urls,
                   const mojom::CloudProvider cloud_provider,
                   bool show_dialog) {
  if (show_dialog) {
    mojom::DialogPage dialog_page =
        cloud_provider == mojom::CloudProvider::kGoogleDrive
            ? mojom::DialogPage::kGoogleDriveSetup
            : mojom::DialogPage::kOneDriveSetup;
    return CloudUploadDialog::Show(profile, file_urls, cloud_provider,
                                   dialog_page);
  }

  bool empty_selection = file_urls.empty();
  DCHECK(!empty_selection);
  if (empty_selection) {
    return false;
  }
  OnCloudSetupComplete(profile, file_urls, cloud_provider, kUserActionUpload);
  return true;
}

// static
bool CloudUploadDialog::Show(
    Profile* profile,
    const std::vector<storage::FileSystemURL>& file_urls,
    const mojom::CloudProvider cloud_provider,
    const mojom::DialogPage dialog_page) {
  // Allow no more than one upload dialog at a time. In the case of multiple
  // upload requests, they should either be handled simultaneously or queued.
  if (SystemWebDialogDelegate::HasInstance(
          GURL(chrome::kChromeUICloudUploadURL))) {
    return false;
  }

  mojom::DialogArgsPtr args = mojom::DialogArgs::New();
  args->cloud_provider = cloud_provider;
  for (const auto& file_url : file_urls) {
    args->file_names.push_back(file_url.path().BaseName().value());
  }
  args->dialog_page = dialog_page;

  // The pointer is managed by an instance of `views::WebDialogView` and removed
  // in `SystemWebDialogDelegate::OnDialogClosed`.
  UploadRequestCallback uploadCallback =
      base::BindOnce(&OnCloudSetupComplete, profile, file_urls, cloud_provider);
  CloudUploadDialog* dialog = new CloudUploadDialog(
      std::move(args), std::move(uploadCallback), dialog_page);

  dialog->ShowSystemDialog();
  return true;
}

void CloudUploadDialog::OnDialogShown(content::WebUI* webui) {
  DCHECK(dialog_args_);
  static_cast<CloudUploadUI*>(webui->GetController())
      ->SetDialogArgs(std::move(dialog_args_));
}

void CloudUploadDialog::OnDialogClosed(const std::string& json_retval) {
  if (callback_) {
    std::move(callback_).Run(json_retval);
  }
  SystemWebDialogDelegate::OnDialogClosed(json_retval);
}

CloudUploadDialog::CloudUploadDialog(mojom::DialogArgsPtr args,
                                     UploadRequestCallback callback,
                                     const mojom::DialogPage dialog_page)
    : SystemWebDialogDelegate(GURL(chrome::kChromeUICloudUploadURL),
                              std::u16string() /* title */),
      dialog_args_(std::move(args)),
      callback_(std::move(callback)),
      dialog_page_(dialog_page) {}

CloudUploadDialog::~CloudUploadDialog() = default;

bool CloudUploadDialog::ShouldShowCloseButton() const {
  return false;
}

namespace {
const int kDialogWidthForOneDriveSetup = 512;
const int kDialogHeightForOneDriveSetup = 532;

const int kDialogWidthForFileHandlerDialog = 512;
const int kDialogHeightForFileHandlerDialog = 532;

const int kDialogWidthForDriveSetup = 512;
const int kDialogHeightForDriveSetup = 220;
}  // namespace

void CloudUploadDialog::GetDialogSize(gfx::Size* size) const {
  switch (dialog_page_) {
    case mojom::DialogPage::kFileHandlerDialog: {
      size->set_width(kDialogWidthForFileHandlerDialog);
      size->set_height(kDialogHeightForFileHandlerDialog);
      return;
    }
    case mojom::DialogPage::kOneDriveSetup: {
      size->set_width(kDialogWidthForOneDriveSetup);
      size->set_height(kDialogHeightForOneDriveSetup);
      return;
    }
    case mojom::DialogPage::kGoogleDriveSetup: {
      size->set_width(kDialogWidthForDriveSetup);
      size->set_height(kDialogHeightForDriveSetup);
      return;
    }
  }
}

}  // namespace ash::cloud_upload
