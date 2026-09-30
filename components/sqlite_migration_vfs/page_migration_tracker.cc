// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sqlite_migration_vfs/page_migration_tracker.h"

#include "base/check.h"
#include "base/check_op.h"
#include "base/thread_annotations.h"

namespace sqlite_migration_vfs {

PageMigrationTracker::PageMigrationTracker(MigrationTarget target,
                                           size_t total_pages)
    : target_(target), total_pages_(total_pages) {
  size_t total_blocks =
      (total_pages == 0) ? 0 : (total_pages - 1) / kPagesPerBlock + 1;
  blocks_.reserve(total_blocks);
  for (size_t i = 0; i < total_blocks; ++i) {
    blocks_.push_back(std::make_unique<BlockMigrationTracker>());
  }
}

PageMigrationTracker::~PageMigrationTracker() = default;

bool PageMigrationTracker::IsMigrated(size_t page) const {
  CHECK_GE(page, 1u);
  // Pages allocated beyond the initial snapshot are considered migrated so
  // that foreground writes immediately double-write to both database files.
  if (page > total_pages_) {
    return true;
  }
  size_t block_index = (page - 1) / kPagesPerBlock;
  return blocks_[block_index]->IsMigrated();
}

// NO_THREAD_SAFETY_ANALYSIS: Clang's static thread-safety analyzer cannot
// track locks on dynamically-indexed vector elements (`blocks_[i]`) that are
// acquired here and released later in `MarkMigrationAsCompletedAndUnlockIt()`,
// nor does `EXCLUSIVE_TRYLOCK_FUNCTION` support `std::optional` return types.
std::optional<PageMigrationTracker::Block>
PageMigrationTracker::GetNextBlockToMigrateAndLockIt(
    const std::optional<Block>& previous_block) NO_THREAD_SAFETY_ANALYSIS {
  size_t start_index = 0;
  if (previous_block.has_value()) {
    CHECK_GE(previous_block->start_page, 1u);
    start_index = (previous_block->start_page - 1) / kPagesPerBlock;
    CHECK_LT(start_index, blocks_.size());
  }
  for (size_t offset = 0; offset < blocks_.size(); ++offset) {
    if (IsMigrationComplete()) {
      return std::nullopt;
    }
    size_t i = (start_index + offset) % blocks_.size();
    if (blocks_[i]->TryLockForMigration()) {
      if (!has_started_migration_.exchange(true, std::memory_order_relaxed)) {
        start_time_ = base::TimeTicks::Now();
        RecordTotalBlocksIntendedToMigrate(target_, blocks_.size());
        RecordMigrationStatus(target_, MigrationStatus::kStarted);
      }
      size_t start_page = i * kPagesPerBlock + 1;
      size_t end = (i + 1) * kPagesPerBlock;
      size_t end_page = (end > total_pages_) ? total_pages_ : end;
      return Block{
          .start_page = start_page,
          .end_page = end_page,
          .lock_time = base::TimeTicks::Now(),
      };
    }
  }
  return std::nullopt;
}

void PageMigrationTracker::MarkMigrationAsCompletedAndUnlockIt(
    const Block& block,
    BlockMigrationResult result) NO_THREAD_SAFETY_ANALYSIS {
  CHECK_GE(block.start_page, 1u);
  size_t block_index = (block.start_page - 1) / kPagesPerBlock;
  CHECK_LT(block_index, blocks_.size());

  RecordBlockMigrationResult(target_, result);
  CHECK(!block.lock_time.is_null());
  RecordBlockMigrationTime(target_, base::TimeTicks::Now() - block.lock_time);

  bool success = (result == BlockMigrationResult::kSuccess);
  blocks_[block_index]->Unlock(success);

  if (success) {
    migrated_blocks_count_.fetch_add(1, std::memory_order_relaxed);
    if (IsMigrationComplete()) {
      if (!has_completed_migration_.exchange(true, std::memory_order_relaxed)) {
        RecordMigrationStatus(target_, MigrationStatus::kCompleted);
        CHECK(!start_time_.is_null());
        RecordTotalMigrationTime(target_, base::TimeTicks::Now() - start_time_);
      }
    }
  }
}

bool PageMigrationTracker::IsMigrationComplete() const {
  return migrated_blocks_count_.load(std::memory_order_relaxed) >=
         blocks_.size();
}

}  // namespace sqlite_migration_vfs
