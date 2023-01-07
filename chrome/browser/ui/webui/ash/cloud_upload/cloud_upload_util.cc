// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_util.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "content/public/browser/browser_thread.h"

namespace ash::cloud_upload {

storage::FileSystemURL FilePathToFileSystemURL(
    Profile* profile,
    scoped_refptr<storage::FileSystemContext> file_system_context,
    base::FilePath file_path) {
  GURL url;
  if (!file_manager::util::ConvertAbsoluteFilePathToFileSystemUrl(
          profile, file_path, file_manager::util::GetFileManagerURL(), &url)) {
    LOG(ERROR) << "Unable to ConvertAbsoluteFilePathToFileSystemUrl";
    return storage::FileSystemURL();
  }

  return file_system_context->CrackURLInFirstPartyContext(url);
}

void CreateDirectoryOnIOThread(
    scoped_refptr<storage::FileSystemContext> file_system_context,
    storage::FileSystemURL destination_folder_url,
    base::OnceCallback<void(base::File::Error)> complete_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  file_system_context->operation_runner()->CreateDirectory(
      destination_folder_url, /*exclusive=*/false, /*recursive=*/false,
      std::move(complete_callback));
}

void LogErrorOnShowItemInFolder(platform_util::OpenOperationResult result) {
  if (result == platform_util::OPEN_SUCCEEDED) {
    return;
  }
  std::string error_string = "";
  switch (result) {
    case platform_util::OpenOperationResult::OPEN_SUCCEEDED:
      error_string = "OPEN_SUCCEEDED";
      break;
    case platform_util::OpenOperationResult::OPEN_FAILED_PATH_NOT_FOUND:
      error_string = "OPEN_FAILED_PATH_NOT_FOUND";
      break;
    case platform_util::OpenOperationResult::OPEN_FAILED_INVALID_TYPE:
      error_string = "OPEN_FAILED_INVALID_TYPE";
      break;
    case platform_util::OpenOperationResult::
        OPEN_FAILED_NO_HANLDER_FOR_FILE_TYPE:
      error_string = "OPEN_FAILED_NO_HANLDER_FOR_FILE_TYPE";
      break;
    case platform_util::OpenOperationResult::OPEN_FAILED_FILE_ERROR:
      error_string = "OPEN_FAILED_FILE_ERROR";
      break;
  }
  LOG(ERROR) << "Failed to show destination file in Files app : "
             << error_string;
}

}  // namespace ash::cloud_upload
