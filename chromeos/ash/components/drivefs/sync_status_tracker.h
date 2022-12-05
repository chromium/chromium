// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DRIVEFS_SYNC_STATUS_TRACKER_H_
#define CHROMEOS_ASH_COMPONENTS_DRIVEFS_SYNC_STATUS_TRACKER_H_

#include <cstddef>
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
  kQueued,
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

  void AddSyncStatusForPath(const int64_t id,
                            const base::FilePath& path,
                            SyncStatus status);
  SyncStatus GetSyncStatusForPath(const base::FilePath& path);
  void RemovePath(const int64_t id, const base::FilePath& path);

  size_t LeafCount() const { return id_to_node_.size(); }

 private:
  struct TrieNode {
    typedef base::flat_map<base::FilePath::StringType,
                           std::unique_ptr<TrieNode>>
        PathToChildMap;

    explicit TrieNode(SyncStatus status,
                      base::FilePath::StringType path_part,
                      TrieNode* parent);
    ~TrieNode();

    SyncStatus status;
    PathToChildMap children;
    base::FilePath::StringType path_part;
    TrieNode* parent = nullptr;
  };

  // Remove the node and traverse its parents removing them if they become
  // childless.
  void RemoveNode(const TrieNode* node);

  std::unique_ptr<TrieNode> root_ = nullptr;
  base::flat_map<int64_t, TrieNode*> id_to_node_;
};

}  // namespace drivefs

#endif  // CHROMEOS_ASH_COMPONENTS_DRIVEFS_SYNC_STATUS_TRACKER_H_
