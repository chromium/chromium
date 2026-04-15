// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SQLITE_VFS_SHARED_LOCKS_H_
#define COMPONENTS_SQLITE_VFS_SHARED_LOCKS_H_

#include <stdint.h>

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
//
// TODO(crbug.com/486665177): Add the eight WAL locks to support multiple
// connections for databases that use a write-ahead log.
class COMPONENT_EXPORT(SQLITE_VFS) SharedLocks {
 public:
  // Returns a new unmapped shared memory region sized appropriately to hold
  // the shared locks.
  static base::UnsafeSharedMemoryRegion CreateRegion();

  // Maps `region` and returns a SharedLocks object backed by it. Returns no
  // value in case of failure to map `region` into the process's address space.
  static std::optional<SharedLocks> Create(
      const base::UnsafeSharedMemoryRegion& region);

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

 private:
  // Creates a new SharedLocks object backed by the given mapping, which must
  // have been created via `CreateRegion()`.
  explicit SharedLocks(base::WritableSharedMemoryMapping mapping);

  // Returns the atomic for the primary database lock.
  using DatabaseLock = base::subtle::SharedAtomic<uint32_t>;
  DatabaseLock& GetDatabaseLock();

  base::WritableSharedMemoryMapping mapping_;
};

}  // namespace sqlite_vfs

#endif  // COMPONENTS_SQLITE_VFS_SHARED_LOCKS_H_
