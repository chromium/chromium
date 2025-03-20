// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WIN_CLOUD_SYNCED_FOLDER_CHECKER_H_
#define CHROME_BROWSER_WIN_CLOUD_SYNCED_FOLDER_CHECKER_H_

#include "base/feature_list.h"

namespace base {
class FilePath;
}

namespace cloud_synced_folder_checker {

namespace features {
BASE_FEATURE(kCloudSyncedFolderChecker,
             "CloudSyncedFolderChecker",
             base::FEATURE_ENABLED_BY_DEFAULT);
}  // namespace features

struct CloudSyncStatus {
  // True if the file/folder is synced with a cloud storage provider (e.g.,
  // OneDrive).
  bool synced = false;
  // True if the user's Desktop folder is synced in the cloud storage.
  bool desktop_synced = false;
  // True if the user's Documents folder is synced in the cloud storage.
  bool documents_synced = false;
};

// Determines OneDrive synced status.
CloudSyncStatus EvaluateOneDriveSyncStatus();

// Determines if a file or directory is managed by a cloud storage provider and
// is currently synchronized, using the `PKEY_StorageProviderState` property.
bool IsCloudStorageSynced(const base::FilePath& file_path);

}  // namespace cloud_synced_folder_checker

#endif  // CHROME_BROWSER_WIN_CLOUD_SYNCED_FOLDER_CHECKER_H_
