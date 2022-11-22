// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_CLOUD_UPLOAD_CLOUD_UPLOAD_UTIL_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_CLOUD_UPLOAD_CLOUD_UPLOAD_UTIL_H_

#include "base/callback.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "chrome/browser/platform_util.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"

class Profile;

namespace ash::cloud_upload {

// The default folder where the file should be uploaded.
const char kDestinationFolder[] = "from Chromebook";

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

// After Files app has been opened on a given location, this is used as callback
// to log any encountered error.
void LogErrorOnShowItemInFolder(platform_util::OpenOperationResult result);

}  // namespace ash::cloud_upload

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_CLOUD_UPLOAD_CLOUD_UPLOAD_UTIL_H_
