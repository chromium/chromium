// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sqlite_migration_vfs/block_migration_tracker.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace sqlite_migration_vfs {
namespace {

TEST(BlockMigrationTrackerTest, InitialState) {
  BlockMigrationTracker tracker;
  EXPECT_FALSE(tracker.IsMigrated());
}

TEST(BlockMigrationTrackerTest, SuccessfulMigration) {
  BlockMigrationTracker tracker;
  if (tracker.TryLockForMigration()) {
    EXPECT_FALSE(tracker.IsMigrated());

    // While locked, a second try-lock fails immediately without blocking.
    EXPECT_FALSE(tracker.TryLockForMigration());

    tracker.Unlock(/*success=*/true);
  } else {
    ADD_FAILURE();
  }
  EXPECT_TRUE(tracker.IsMigrated());

  // Once migrated, try-lock always fails.
  EXPECT_FALSE(tracker.TryLockForMigration());
}

TEST(BlockMigrationTrackerTest, FailedMigrationAllowsRetry) {
  BlockMigrationTracker tracker;
  if (tracker.TryLockForMigration()) {
    // Unlock without completing migration (e.g. on error).
    tracker.Unlock(/*success=*/false);
  } else {
    ADD_FAILURE();
  }
  EXPECT_FALSE(tracker.IsMigrated());

  // Another worker can acquire the lock and try again.
  if (tracker.TryLockForMigration()) {
    tracker.Unlock(/*success=*/true);
  } else {
    ADD_FAILURE();
  }
  EXPECT_TRUE(tracker.IsMigrated());
}

}  // namespace
}  // namespace sqlite_migration_vfs
