// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DRIVEFS_SYNC_STATUS_TRACKER_H_
#define CHROMEOS_ASH_COMPONENTS_DRIVEFS_SYNC_STATUS_TRACKER_H_

#include <memory>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"

namespace drivefs {

// Note: The order here matters when resolving the status of directories.
// The precedence increases from top to bottom. E.g., a directory containing
// one file with SyncStatus=kInProgress and one file with SyncStatus=kError will
// be reported with SyncStatus=kError.
enum SyncStatus {
  kNotFound,
  kInProgress,
  kError,
};

// Cache for sync status coming from DriveFs.
// Allows quick insertion, removal, and look up by file path.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_DRIVEFS) SyncStatusTracker {
 public:
  SyncStatusTracker();
  ~SyncStatusTracker();

  SyncStatusTracker(const SyncStatusTracker&) = delete;
  SyncStatusTracker& operator=(const SyncStatusTracker&) = delete;

  void AddSyncStatusForPath(const base::FilePath& path, SyncStatus status);
  SyncStatus GetSyncStatusForPath(const base::FilePath& path);
  void RemovePath(const base::FilePath& path);

 private:
  struct TrieNode {
    explicit TrieNode(SyncStatus status);
    ~TrieNode();
    SyncStatus status;
    typedef base::flat_map<base::FilePath::StringType,
                           std::unique_ptr<TrieNode>>
        PathToChildMap;
    PathToChildMap children;
  };
  std::unique_ptr<TrieNode> root_ = nullptr;
};

}  // namespace drivefs

#endif  // CHROMEOS_ASH_COMPONENTS_DRIVEFS_SYNC_STATUS_TRACKER_H_
