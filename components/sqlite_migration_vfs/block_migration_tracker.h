// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SQLITE_MIGRATION_VFS_BLOCK_MIGRATION_TRACKER_H_
#define COMPONENTS_SQLITE_MIGRATION_VFS_BLOCK_MIGRATION_TRACKER_H_

#include <atomic>

#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"

namespace sqlite_migration_vfs {

// Manages thread-safe locking and migration state for a single block of pages.
//
// Thread-safety model:
// - `is_migrated_` is atomic and read without locks, ensuring foreground
// operations
//   (e.g. xRead/xWrite) never contend on locks or risk deadlock.
// - `lock_` is used solely to coordinate background migration workers so that
// only
//   one worker at a time attempts to migrate this block.
//
// TODO(crbug.com/476343961): Handle concurrent xWrite during in-flight block
// migration without blocking foreground I/O. Add a needs_migration_retry flag
// set by xWrite if a write occurs while a block is locked for migration,
// followed by a post-migration retry pass and full read-through comparison
// before final handover.
class BlockMigrationTracker {
 public:
  BlockMigrationTracker();
  ~BlockMigrationTracker();

  BlockMigrationTracker(const BlockMigrationTracker&) = delete;
  BlockMigrationTracker& operator=(const BlockMigrationTracker&) = delete;

  // Returns true if this block has been migrated. Non-blocking and lock-free.
  bool IsMigrated() const;

  // Attempts to acquire the lock for migration without blocking.
  // Returns true if the lock was acquired and the block is not yet migrated.
  // Returns false immediately if the lock is held by another worker or if the
  // block has already been migrated.
  bool TryLockForMigration() EXCLUSIVE_TRYLOCK_FUNCTION(true, lock_);

  // Releases the lock. If `success` is true, marks the block as migrated.
  void Unlock(bool success) UNLOCK_FUNCTION(lock_);

 private:
  base::Lock lock_;
  std::atomic<bool> is_migrated_{false};
};

}  // namespace sqlite_migration_vfs

#endif  // COMPONENTS_SQLITE_MIGRATION_VFS_BLOCK_MIGRATION_TRACKER_H_
