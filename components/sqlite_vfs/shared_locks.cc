// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sqlite_vfs/shared_locks.h"

#include "base/check.h"
#include "base/check_op.h"
#include "base/notreached.h"
#include "third_party/sqlite/sqlite3.h"

namespace sqlite_vfs {

namespace {

// Properties of the primary database lock.
constexpr uint32_t kMaxSharedLocks = 0x08000000;
constexpr uint32_t kSharedMask = 0x0FFFFFFF;
constexpr uint32_t kReservedBit = 0x20000000;
constexpr uint32_t kPendingBit = 0x40000000;
constexpr uint32_t kAbandonedBit = 0x80000000;

}  // namespace

// static
base::UnsafeSharedMemoryRegion SharedLocks::CreateRegion() {
  return base::UnsafeSharedMemoryRegion::Create(sizeof(DatabaseLock));
}

// static
std::optional<SharedLocks> SharedLocks::Create(
    const base::UnsafeSharedMemoryRegion& region) {
  CHECK(region.IsValid());
  CHECK_GE(region.GetSize(), sizeof(DatabaseLock));
  auto mapping = region.Map();
  if (!mapping.IsValid()) {
    return std::nullopt;
  }
  return SharedLocks(std::move(mapping));
}

SharedLocks::~SharedLocks() = default;

// The primary database lock is encoded over 32-bits:
//   3 3 2 2 2 2 2 2 2 2 2 2 1 1 1 1 1 1 1 1 1 1
//   1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
//  +---+-+-+-----------------------+-------------------------------+
//  |A|P|R|0|                     SHARED COUNT                      |
//  +---+-+-+-----------------------+-------------------------------+
//
// Where
//
//   SHARED COUNT: The number of SHARED locks held by readers.
//   A: Whether the lock is abandoned. If set no further use is permitted.
//   R: The RESERVED lock is held. New shared locks are still permitted.
//   P: The PENDING lock is held. No new shared locks are permitted while any
//      process holds the PENDING lock.
//
// A process holds the EXCLUSIVE lock when it holds the PENDING lock and the
// SHARED COUNT is zero.

int SharedLocks::Lock(int mode, int& current_mode) {
  CHECK(mode == SQLITE_LOCK_SHARED || mode == SQLITE_LOCK_RESERVED ||
        mode == SQLITE_LOCK_EXCLUSIVE);
  CHECK_LT(current_mode, mode);

  auto& database_lock = GetDatabaseLock();
  switch (mode) {
    case SQLITE_LOCK_SHARED: {
      // Try to increment the SHARED lock count as long as the PENDING lock
      // remains unheld and there is room remaining to count a new SHARED lock.
      uint32_t lock_snapshot = database_lock.load();

      if ((lock_snapshot & kAbandonedBit) != 0) {
        return SQLITE_IOERR_LOCK;
      }

      for (int i = 0; i < 5; ++i) {
        if ((lock_snapshot & kPendingBit) != 0 ||
            (lock_snapshot & kSharedMask) == kMaxSharedLocks) {
          break;
        }
        if (database_lock.compare_exchange_strong(lock_snapshot,
                                                  lock_snapshot + 1)) {
          // The SHARED lock was successfully acquired.
          current_mode = SQLITE_LOCK_SHARED;
          return SQLITE_OK;
        }

        if ((lock_snapshot & kAbandonedBit) != 0) {
          return SQLITE_IOERR_LOCK;
        }

        // Perform up to four retries in case this client is racing against
        // other changes to the shared lock.
      }
      return SQLITE_BUSY;
    }

    case SQLITE_LOCK_RESERVED: {
      // To acquire a RESERVED lock, the current connection must already have
      // a shared access to it.
      CHECK_EQ(current_mode, SQLITE_LOCK_SHARED);

      // Acquire a RESERVED lock to prevent a different writer to declare its
      // intention to modify the database. At this point, readers are still
      // allowed to get a SHARED lock on the database.
      const uint32_t lock_snapshot = database_lock.fetch_or(kReservedBit);
      if ((lock_snapshot & kAbandonedBit) != 0) {
        return SQLITE_IOERR_LOCK;
      }

      if ((lock_snapshot & kReservedBit) != 0) {
        return SQLITE_BUSY;
      }

      // The RESERVED lock was successfully acquired.
      current_mode = SQLITE_LOCK_RESERVED;
      return SQLITE_OK;
    }

    case SQLITE_LOCK_EXCLUSIVE: {
      // Acquiring an EXCLUSIVE lock may happen through multiple calls to
      // SandboxedFile::Lock(...) and the PENDING lock may be kept between these
      // calls.

      // To acquire an EXCLUSIVE lock, the current connection must already have
      // at least SHARED lock. Owning RESERVED lock not mandatory.
      CHECK_GE(current_mode, SQLITE_LOCK_SHARED);

      // Acquire the PENDING lock, if not already acquired. Hold it until the
      // EXCLUSIVE lock is obtained. No new SHARED locks will be granted in
      // the meantime, but current SHARED locks remain valid.
      uint32_t lock_snapshot = 0;
      if (current_mode < SQLITE_LOCK_PENDING) {
        lock_snapshot = database_lock.fetch_or(kPendingBit);
        if ((lock_snapshot & kAbandonedBit) != 0) {
          // This instance may have just set `kPendingBit`. There is no need to
          // clear it since all other parties will detect that the instance is
          // abandoned on their next attempt to acquire any lock.
          return SQLITE_IOERR_LOCK;
        }

        if ((lock_snapshot & kPendingBit) != 0) {
          // This connection is not the owner of the PENDING lock.
          return SQLITE_BUSY;
        }
        // The PENDING lock was acquired. Keep it for subsequent calls until all
        // SHARED locks are released.
        current_mode = SQLITE_LOCK_PENDING;
        // Update the copy of the current state of the lock for use below.
        lock_snapshot |= kPendingBit;
      } else {
        // Fetch the current state of the lock for use below.
        lock_snapshot = database_lock.load();

        if ((lock_snapshot & kAbandonedBit) != 0) {
          return SQLITE_IOERR_LOCK;
        }
      }

      // Do not grant the EXCLUSIVE lock until all other readers have released
      // their SHARED locks. This connection still owns and keeps a SHARED lock.
      if ((lock_snapshot & kSharedMask) != 1) {
        return SQLITE_BUSY;
      }

      // There is no active SHARED lock except for this connection. The PENDING
      // lock is owned by this connection so it is valid to grant the EXCLUSIVE
      // lock.
      current_mode = SQLITE_LOCK_EXCLUSIVE;
      return SQLITE_OK;
    }

    default:
      NOTREACHED();  // Not possible as per CHECK at entry.
  }
}

int SharedLocks::Unlock(int mode, int& current_mode) {
  CHECK(mode == SQLITE_LOCK_NONE || mode == SQLITE_LOCK_SHARED);
  CHECK_GT(current_mode, mode);

  auto& database_lock = GetDatabaseLock();

  // Release the RESERVED or RESERVED and PENDING bits, if held.
  if (uint32_t clear_mask =
          (current_mode >= SQLITE_LOCK_PENDING
               ? (kPendingBit | kReservedBit)
               : (current_mode == SQLITE_LOCK_RESERVED ? kReservedBit : 0U))) {
    database_lock.fetch_and(~clear_mask);
  }

  // Release the SHARED lock if no longer needed.
  if (mode == SQLITE_LOCK_NONE) {
    const uint32_t lock_snapshot = database_lock.fetch_sub(1);
    CHECK_GE(lock_snapshot & kSharedMask, 1u);
  }

  // Lock was successfully released.
  current_mode = mode;
  return SQLITE_OK;
}

bool SharedLocks::IsReserved() {
  return (GetDatabaseLock().load() & kReservedBit) != 0;
}

LockState SharedLocks::Abandon() {
  uint32_t previous_state = GetDatabaseLock().fetch_or(kAbandonedBit);

  if ((previous_state & (kReservedBit | kPendingBit)) != 0) {
    return LockState::kWriting;
  }
  if ((previous_state & kSharedMask) != 0) {
    return LockState::kReading;
  }
  return LockState::kNotHeld;
}

SharedLocks::SharedLocks(base::WritableSharedMemoryMapping mapping)
    : mapping_(std::move(mapping)) {
  CHECK_GE(mapping_.size(), sizeof(DatabaseLock));
}

SharedLocks::DatabaseLock& SharedLocks::GetDatabaseLock() {
  return *mapping_.GetMemoryAs<DatabaseLock>();
}

}  // namespace sqlite_vfs
