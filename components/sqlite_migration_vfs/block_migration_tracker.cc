// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sqlite_migration_vfs/block_migration_tracker.h"

namespace sqlite_migration_vfs {

BlockMigrationTracker::BlockMigrationTracker() = default;

BlockMigrationTracker::~BlockMigrationTracker() = default;

bool BlockMigrationTracker::IsMigrated() const {
  return is_migrated_.load(std::memory_order_relaxed);
}

bool BlockMigrationTracker::TryLockForMigration() {
  if (is_migrated_.load(std::memory_order_relaxed)) {
    return false;
  }
  if (!lock_.Try()) {
    return false;
  }
  // Double-check migration state after acquiring the lock.
  if (is_migrated_.load(std::memory_order_relaxed)) {
    lock_.Release();
    return false;
  }
  return true;
}

void BlockMigrationTracker::Unlock(bool success) {
  lock_.AssertAcquired();
  if (success) {
    is_migrated_.store(true, std::memory_order_relaxed);
  }
  lock_.Release();
}

}  // namespace sqlite_migration_vfs
