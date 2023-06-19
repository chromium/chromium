// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_CLOUD_UPLOAD_CLOUD_UPLOAD_UTIL_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_CLOUD_UPLOAD_CLOUD_UPLOAD_UTIL_H_

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "chrome/browser/ash/file_manager/io_task.h"
#include "chrome/browser/platform_util.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"

class Profile;

namespace ash::cloud_upload {

// Type of the source location from which a given file is being uploaded.
enum class SourceType {
  LOCAL = 0,
  READ_ONLY = 1,
  CLOUD = 2,
  kMaxValue = CLOUD,
};

// The result of the "Upload to cloud" workflow for Office files.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class OfficeFilesUploadResult {
  kSuccess = 0,
  kOtherError = 1,
  kFileSystemNotFound = 2,
  kMoveOperationCancelled = 3,
  kMoveOperationError = 4,
  kMoveOperationNeedPassword = 5,
  kCopyOperationCancelled = 6,
  kCopyOperationError = 7,
  kCopyOperationNeedPassword = 8,
  kPinningFailedDiskFull = 9,
  kCloudAuthError = 10,
  kCloudMetadataError = 11,
  kCloudQuotaFull = 12,
  kCloudError = 13,
  kMaxValue = kCloudError,
};

const char kGenericErrorMessage[] = "Something went wrong. Try again.";

// Converts an absolute FilePath into a filesystem URL.
storage::FileSystemURL FilePathToFileSystemURL(
    Profile* profile,
    scoped_refptr<storage::FileSystemContext> file_system_context,
    base::FilePath file_path);

// Creates a directory from a filesystem URL. The callback is called without
// error if the directory already exists.
void CreateDirectoryOnIOThread(
    scoped_refptr<storage::FileSystemContext> file_system_context,
    storage::FileSystemURL destination_folder_url,
    base::OnceCallback<void(base::File::Error)> complete_callback);

// Returns the type of the source location from which the file is getting
// uploaded (see SourceType values).
SourceType GetSourceType(Profile* profile,
                         const storage::FileSystemURL& source_path);

// Returns the operation type (move or copy) for the upload flow based on the
// source path of the file to upload.
::file_manager::io_task::OperationType GetOperationTypeForUpload(
    Profile* profile,
    const storage::FileSystemURL& source_path);

}  // namespace ash::cloud_upload

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_CLOUD_UPLOAD_CLOUD_UPLOAD_UTIL_H_
