# SQLite Migration VFS (`components/sqlite_migration_vfs`)

This component provides a Virtual File System (VFS) and supporting primitives for online SQLite database migration in Chromium.

## Overview

Filesystem operations heavily favor sequential, contiguous I/O. Instead of tracking migration status on individual database pages, database migration operates on contiguous **64-page blocks** (`Block`).

Migrating contiguous blocks reduces disk seeks, simplifies progress tracking, and ensures predictable I/O patterns.

## `PageMigrationTracker`

`PageMigrationTracker` is an in-memory, thread-safe tracker that manages the migration state across a database's pages.

### Public API & Caller Paths

The public interface provides only the exact methods needed by the two operational paths:

#### 1. Foreground Path (`xRead` / `xWrite`)
* **`IsMigrated(page)`**:
  - Used by `xWrite` to decide whether to write to the source only (not yet migrated) or double-write to both databases (already migrated).
  - Used by `xRead` to decide whether to perform optional double-read verification.
  - **Dynamic DB Growth**: If `page > total_pages_` (pages allocated after migration started), returns `true` so new pages are immediately double-written.
  - Non-blocking and lock-free so foreground database I/O never contends or deadlocks.
  - CHECKs that `page >= 1` (SQLite pages are 1-based).
  - TODO(crbug.com/476343961): Handle concurrent `xWrite` during in-flight block migration without blocking. Introduce a `needs_migration_retry` flag set by `xWrite` when touching an active block, followed by a post-migration retry pass and full read-through comparison before handover.

#### 2. Background Migration Path
* **`GetNextBlockToMigrateAndLockIt(previous_block)`**:
  - Scans for the next unmigrated and unlocked block (starting from `previous_block` if provided, wrapping around if needed), acquires its lock, and returns the `Block` (containing `start_page` and `end_page`).
  - Skips blocks currently being migrated by other workers.
  - Returns `std::nullopt` when all blocks have been migrated or are in-flight.
* **`MarkMigrationAsCompletedAndUnlockIt(Block, result)`**:
  - Marks the block as migrated if `result == BlockMigrationResult::kSuccess`, increments the completion counter, and releases the block's lock.
  - If migration fails, callers pass an error result (`kReadError`, `kWriteError`, or `kEncryptionError`) to release the lock without marking the block as migrated.

#### 3. Status & Completion
* **`IsMigrationComplete()`**:
  - Returns true if all blocks in the initial database snapshot have been migrated.
  - TODO(crbug.com/476343961): Decide final cutover / promotion actions once migration is complete, including a full read-through comparison verification before handover.
