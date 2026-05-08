// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SQLITE_VFS_SHARED_LOCKS_H_
#define COMPONENTS_SQLITE_VFS_SHARED_LOCKS_H_

#include <stdint.h>

#include <array>
#include <optional>

#include "base/component_export.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/shared_memory_safety_checker.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "components/sqlite_vfs/lock_state.h"

namespace sqlite_vfs {

// Manages the set of cross-connection (within or across processes) locks for
// a SQLite database.
//
// Each database that supports multiple connections requires that each
// connection use the same set of shared locks to coordinate access. A database
// using a rollback journal requires only the primary database lock. This is
// the only supported scenario at present.
class COMPONENT_EXPORT(SQLITE_VFS) SharedLocks {
 public:
  // Returns a new unmapped shared memory region sized appropriately to hold
  // the shared locks. If `wal_mode` is true, the region is sized to hold the
  // locks for a wal-mode database.
  static base::UnsafeSharedMemoryRegion CreateRegion(bool wal_mode);

  // Maps `region` and returns a SharedLocks object backed by it. Returns no
  // value in case of failure to map `region` into the process's address space.
  // If `wal_mode` is true, the region must have been created to hold the locks
  // for a wal-mode database.
  static std::optional<SharedLocks> Create(
      const base::UnsafeSharedMemoryRegion& region,
      bool wal_mode);

  SharedLocks(const SharedLocks&) = delete;
  SharedLocks& operator=(const SharedLocks&) = delete;

  SharedLocks(SharedLocks&&) = default;
  SharedLocks& operator=(SharedLocks&&) = default;

  ~SharedLocks();

  // Lock operations on the primary database lock corresponding to SQLite file
  // operations; see https://sqlite.org/c3ref/io_methods.html.

  // Attempts to raise the lock from `current_mode` to `mode`, writing the new
  // mode into `current_mode` and returning `SQLITE_OK` on success. Returns a
  // SQLite error code on failure, in which case `current_mode` may have been
  // modified (e.g., an attempt to acquire an exclusive lock may acquire a
  // pending lock before failing with SQLITE_BUSY). Returns `SQLITE_IOERR_LOCK`
  // if the locks have been abandoned.
  int Lock(int mode, int& current_mode);

  // Lowers the lock from a higher level down to `mode`, which must be either
  // `SQLITE_LOCK_SHARED` or `SQLITE_LOCK_NONE`. Sets `current_mode` to `mode`
  // and returns `SQLITE_OK`.
  int Unlock(int mode, int& current_mode);

  // Returns true if any connection holds the reserved lock.
  bool IsReserved();

  // Marks the locks as abandoned, causing all subsequent attempts to raise the
  // primary database lock to a higher level by any party to fail with
  // `SQLITE_IOERR_LOCK`. Returns the state of the primary database lock at the
  // time of abandonment. Determination of this state is made based on a
  // snapshot of the lock at the moment that the lock is abandoned. This is the
  // only point where it is possible to know the state of the lock owing to the
  // nature of atomic bitwise operations on the lock itself; the lock level may
  // be raised to pending or reserved after abandonment, although the callers
  // requesting such will properly detect that the lock has been abandoned.
  LockState Abandon();

  // Returns true if the database lock has been marked as abandoned.
  bool IsAbandoned() const;

  // Acquires or releases `num_locks` WAL locks beginning at index `lock_index`.
  // Shared locks are acquired and released individually (`num_locks` ==
  // 1). Exclusive locks are acquired in a range (`num_locks` >= 1).
  enum class LockOperation { kAcquire, kRelease };
  enum class LockType { kShared, kExclusive };
  int ShmLock(int lock_index,
              int num_locks,
              LockOperation operation,
              LockType type);

  // Enforces a memory barrier.
  void ShmBarrier();

 private:
  // The primary database lock.
  using DatabaseLock = base::subtle::SharedAtomic<uint32_t>;

  // A WAL lock.
  using WalLock = base::subtle::SharedAtomic<uint32_t>;

  // The number of WAL locks.
  static constexpr int kNumWalLocks = 8;

  // The primary database lock and the WAL-index locks.
  struct DatabaseAndWalLocks {
    DatabaseLock primary_lock;

    // The eight WAL locks:
    // 0: WAL_WRITE_LOCK. Held exclusively while a read/write connection is
    //    appending to the WAL or is recovering the WAL-index.
    // 1: WAL_CKPT_LOCK. Held exclusively while a read/write connection is
    //    performing a checkpoint or is recovering the WAL-index.
    // 2: WAL_RECOVER_LOCK. Held exclusively while a read/write connection is
    //    recovering the WAL-index.
    // 3-7: WAL_READ_LOCK(I). Read locks corresponding to the read-marks in the
    //      WAL-index. One of the read locks is held shared by any connection
    //      that is within a transaction. One read lock is held exclusively
    //      while updating the corresponding read-mark in the WAL-index. All but
    //      the first read locks are held exclusively when resetting the WAL
    //      after a complete checkpoint or while recovering the WAL-index.
    std::array<WalLock, kNumWalLocks> wal_locks;
  };

  SharedLocks(base::WritableSharedMemoryMapping mapping, bool wal_mode);

  // Returns the atomic for the primary database lock.
  DatabaseLock& GetDatabaseLock();

  // Returns the specified WAL lock.
  WalLock& GetWalLock(int index);

  base::WritableSharedMemoryMapping mapping_;
  bool wal_mode_;
};

}  // namespace sqlite_vfs

#endif  // COMPONENTS_SQLITE_VFS_SHARED_LOCKS_H_
