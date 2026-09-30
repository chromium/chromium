// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SQLITE_MIGRATION_VFS_PAGE_MIGRATION_TRACKER_H_
#define COMPONENTS_SQLITE_MIGRATION_VFS_PAGE_MIGRATION_TRACKER_H_

#include <stddef.h>

#include <atomic>
#include <memory>
#include <optional>
#include <vector>

#include "base/time/time.h"
#include "components/sqlite_migration_vfs/block_migration_tracker.h"
#include "components/sqlite_migration_vfs/sqlite_migration_metrics.h"

namespace sqlite_migration_vfs {

// Default number of SQLite database pages per migration block.
// Each page is 2KB-4KB so a 64-page block is 128KB-256KB, which means the
// P50 DB has ~40 blocks and the P99 DB has ~600 blocks. Monitor
// `Sql.Migration.{SqlMigrationTarget}.TotalBlocks` to check if this math
// changes.
inline constexpr size_t kPagesPerBlock = 64;

// Tracks the migration progress of SQLite database pages across 64-page blocks.
//
// Thread-safety:
// - All public methods are thread-safe.
// - `IsMigrated()` is non-blocking and lock-free so foreground database I/O
//   (xRead/xWrite) never blocks or deadlocks.
// - Each block coordinates background migration via BlockMigrationTracker.
// - Database pages are 1-based, matching SQLite conventions.
//
// TODO(crbug.com/476343961): Handle concurrent xWrite during active block
// migration without blocking. Introduce a retry mechanism (e.g. a
// needs_migration_retry flag set on xWrite when touching an in-flight block),
// followed by a post-migration retry pass and full read-through comparison
// before final handover.
class PageMigrationTracker {
 public:
  // Represents an abstract range of 1-based database pages.
  struct Block {
    size_t start_page = 0;
    size_t end_page = 0;
    base::TimeTicks lock_time;

    bool operator==(const Block& other) const = default;
  };

  PageMigrationTracker(MigrationTarget target, size_t total_pages);
  ~PageMigrationTracker();

  PageMigrationTracker(const PageMigrationTracker&) = delete;
  PageMigrationTracker& operator=(const PageMigrationTracker&) = delete;

  // (1) Returns true if `page` has already been migrated. Non-blocking.
  //
  // Dynamic scaling: If `page > total_pages_`, returns true because
  // pages allocated after migration began are immediately double-written.
  // CHECKs that `page >= 1`.
  bool IsMigrated(size_t page) const;

  // (2) Returns true if all blocks in the initial database snapshot have been
  // migrated.
  bool IsMigrationComplete() const;

  // (3) Finds the next unmigrated and unlocked block, acquires its lock, and
  // returns the Block range. If `previous_block` is provided, starts scanning
  // from that block (wrapping around if needed) to avoid rescanning from the
  // beginning. Skips blocks currently being migrated by other workers. Returns
  // std::nullopt when all blocks are migrated or in-flight.
  std::optional<Block> GetNextBlockToMigrateAndLockIt(
      const std::optional<Block>& previous_block = std::nullopt);

  // (4) Releases the block's lock. If `result` is kSuccess, marks the block as
  // migrated and updates completion progress.
  void MarkMigrationAsCompletedAndUnlockIt(const Block& block,
                                           BlockMigrationResult result);

  size_t total_pages_for_test() const { return total_pages_; }
  MigrationTarget target() const { return target_; }

 private:
  const MigrationTarget target_;
  const size_t total_pages_;
  std::atomic<size_t> migrated_blocks_count_{0};
  std::atomic<bool> has_started_migration_{false};
  std::atomic<bool> has_completed_migration_{false};
  base::TimeTicks start_time_;

  std::vector<std::unique_ptr<BlockMigrationTracker>> blocks_;
};

}  // namespace sqlite_migration_vfs

#endif  // COMPONENTS_SQLITE_MIGRATION_VFS_PAGE_MIGRATION_TRACKER_H_
