// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WIN_CLOUD_SYNCED_FOLDER_CHECKER_H_
#define CHROME_BROWSER_WIN_CLOUD_SYNCED_FOLDER_CHECKER_H_

#include <optional>

#include "base/feature_list.h"
#include "base/files/file_path.h"

namespace cloud_synced_folder_checker {

namespace features {
BASE_DECLARE_FEATURE(kCloudSyncedFolderChecker);
}  // namespace features

struct CloudSyncStatus {
  CloudSyncStatus();
  CloudSyncStatus(const CloudSyncStatus&);
  CloudSyncStatus& operator=(const CloudSyncStatus&);
  CloudSyncStatus(CloudSyncStatus&&);
  CloudSyncStatus& operator=(CloudSyncStatus&&);
  ~CloudSyncStatus();

  // True if the file/folder is synced with a cloud storage provider (e.g.,
  // OneDrive).
  bool synced() const { return one_drive_path.has_value(); }

  // True if the user's Desktop folder is synced in the cloud storage.
  bool desktop_synced() const { return desktop_path.has_value(); }

  // True if the user's Documents folder is synced in the cloud storage.
  bool documents_synced() const { return documents_path.has_value(); }

  // Absolute locations (present only if detection succeeded).
  std::optional<base::FilePath> one_drive_path;
  std::optional<base::FilePath> desktop_path;
  std::optional<base::FilePath> documents_path;
};

// Determines OneDrive synced status.
CloudSyncStatus EvaluateOneDriveSyncStatus();

// Determines if a file or directory is managed by a cloud storage provider and
// is currently synchronized, using the `PKEY_StorageProviderState` property.
bool IsCloudStorageSynced(const base::FilePath& file_path);

}  // namespace cloud_synced_folder_checker

#endif  // CHROME_BROWSER_WIN_CLOUD_SYNCED_FOLDER_CHECKER_H_
