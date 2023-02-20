// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DRIVEFS_SYNC_STATUS_TRACKER_H_
#define CHROMEOS_ASH_COMPONENTS_DRIVEFS_SYNC_STATUS_TRACKER_H_

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <ostream>
#include <string>
#include <string_view>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"

namespace drivefs {

// Note: The order here matters when resolving the status of directories.
// The precedence increases from top to bottom. E.g., a directory containing
// one file with SyncStatus=kInProgress and one file with SyncStatus=kError will
// be reported with SyncStatus=kError.
enum class SyncStatus {
  kNotFound,
  kMoved,
  kCompleted,
  kQueued,
  kInProgress,
  kError,
};
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_DRIVEFS)
std::ostream& operator<<(std::ostream& os, const SyncStatus& status);

struct SyncState {
  SyncStatus status;
  float progress;  // Range: 0 to 1.
  base::FilePath path;

  friend std::ostream& operator<<(std::ostream& os, const SyncState& state) {
    return os << "('" << state.path << "', " << state.status << ", "
              << (int)(state.progress * 100.f) << "%"
              << ") ";
  }
  bool operator==(const SyncState& state) const {
    return state.path == path && state.status == status &&
           std::fabs(state.progress - progress) < 1e-4;
  }

  inline static SyncState CreateNotFound(const base::FilePath path) {
    return {SyncStatus::kNotFound, 0, std::move(path)};
  }
};

// Cache for sync statuses coming from arbitrary cloud providers.
// Allows quick insertion, removal, and look up by file path.
// How to use it:
// 1. Update the tracker by making calls to SetCompleted, SetQueued,
// SetInProgress, and SetError;
// 2. The state of any path can be tracked at any time using GetSyncState;
// 3. In time intervals (recommended: between 0 and 1 second), call
// GetChangesAndClean to get all accumulated changes so far and efficiently
// report them without any duplicates. This will also clear up the memory used
// by nodes that no longer need to be tracked (completed or removed).
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_DRIVEFS) SyncStatusTracker {
 public:
  SyncStatusTracker();
  ~SyncStatusTracker();
  SyncStatusTracker(const SyncStatusTracker&) = delete;
  SyncStatusTracker& operator=(const SyncStatusTracker&) = delete;

  void SetCompleted(const int64_t id, const base::FilePath& path) {
    // Completed events might fire more than once for the same id.
    // Only use completed events to update currently tracked nodes.
    if (id_to_node_.count(id)) {
      SetSyncState(id, path, SyncStatus::kCompleted);
    }
  }

  void SetQueued(const int64_t id,
                 const base::FilePath& path,
                 const int64_t total) {
    SetSyncState(id, path, SyncStatus::kQueued, 0, total);
  }

  void SetInProgress(const int64_t id,
                     const base::FilePath& path,
                     const int64_t transferred,
                     const int64_t total) {
    if (transferred == total) {
      SetCompleted(id, path);
      return;
    }
    SetSyncState(id, path, SyncStatus::kInProgress, transferred, total);
  }

  void SetError(const int64_t id, const base::FilePath& path) {
    SetSyncState(id, path, SyncStatus::kError);
  }

  SyncState GetSyncState(const base::FilePath path) const;

  // Returns a vector of all paths that changed status or progress since the
  // creation of the SyncStatusTracker instance or since the last call to this
  // function. Upon calling it, the trie is reset to a pristine state, meaning
  // it will report 0 accumulated changes until more changes are registered
  // through SetCompleted, SetQueued, SetInProgress, and SetError.
  const std::vector<const SyncState> GetChangesAndClean();

  size_t GetFileCount() const { return id_to_node_.size(); }

 private:
  struct NodeState;
  struct Node;

  Node* FindNode(const base::FilePath& path) const;

  void SetSyncState(const int64_t id,
                    const base::FilePath& path,
                    const SyncStatus status,
                    const int64_t transferred = 0,
                    const int64_t total = 0);

  bool ShouldRemoveNode(const Node* node) const;

  // Remove the node and traverse its parents removing them if they become
  // childless.
  void RemoveNode(const Node* node);

  // Sets the new state on the node and propagate the state delta to the node's
  // ancestors until the root.
  void SetNodeState(Node* node,
                    const SyncStatus status,
                    int64_t transferred,
                    int64_t total);

  const SyncState GetNodeState(
      const Node* node,
      const base::FilePath path = base::FilePath()) const;

  const base::FilePath GetNodePath(const Node* node) const;

  std::unique_ptr<Node> root_ = std::make_unique<Node>();
  base::flat_map<int64_t, Node*> id_to_node_;
};

struct SyncStatusTracker::NodeState {
 public:
  // Sets the state with the provided new values and returns a "delta" NodeState
  // representing what changes were applied. If delta is pristine, no changes
  // were applied.
  NodeState Set(SyncStatus new_status, int64_t transferred, int64_t total);

  void ApplyDelta(const NodeState& status);

  inline void MarkAsDirty() { is_dirty_ = true; }
  inline void MarkAsPristine() { is_dirty_ = false; }
  inline bool IsDirty() const { return is_dirty_; }
  inline bool IsPristine() const { return !is_dirty_; }

  float GetProgress() const {
    return total_
               ? static_cast<double>(transferred_) / static_cast<double>(total_)
               : 0;
  }

  SyncStatus GetStatus() const;
  void SetStatus(const SyncStatus status);

 private:
  int32_t queued_count_ = 0;
  int32_t in_progress_count_ = 0;
  int32_t error_count_ = 0;
  int64_t transferred_ = 0;
  int64_t total_ = 0;
  // Tracks whether the status that GetStatus() would return has changed since
  // the NodeState instance was created or since the instance was last
  // marked as pristine, whichever happened last.
  bool is_dirty_ = false;
};

struct SyncStatusTracker::Node {
  Node();
  ~Node();

  typedef base::flat_map<base::FilePath::StringType, std::unique_ptr<Node>>
      PathToChildMap;

  int64_t id = 0;
  PathToChildMap children;
  base::FilePath::StringType path_part;
  Node* parent = nullptr;
  NodeState state;
};

}  // namespace drivefs

#endif  // CHROMEOS_ASH_COMPONENTS_DRIVEFS_SYNC_STATUS_TRACKER_H_
