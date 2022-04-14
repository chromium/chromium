// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_INDEXED_DB_LOCKS_LEVELED_LOCK_H_
#define COMPONENTS_SERVICES_STORAGE_INDEXED_DB_LOCKS_LEVELED_LOCK_H_

#include <iosfwd>

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/component_export.h"
#include "components/services/storage/indexed_db/locks/leveled_lock_range.h"

namespace content {

// Represents a granted lock in the LeveledLockManager. When this object is
// destroyed, the lock is released. Since default construction is supported,
// |is_locked()| can be used to inquire locked status. Also, |Release()| can
// be called to manually release the lock, which appropriately updates the
// |is_locked()| result.
class COMPONENT_EXPORT(LOCK_MANAGER) LeveledLock {
 public:
  using LockReleasedCallback =
      base::OnceCallback<void(int level, LeveledLockRange range)>;

  LeveledLock();

  LeveledLock(const LeveledLock&) = delete;
  LeveledLock& operator=(const LeveledLock&) = delete;

  ~LeveledLock();
  LeveledLock(LeveledLock&&) noexcept;
  // |lock_released_callback| is called when the lock is released, either by
  // destruction of this object or by the |Released()| call. It will be called
  // synchronously on the sequence runner this lock is released on.
  LeveledLock(LeveledLockRange range,
              int level,
              LockReleasedCallback lock_released_callback);
  // The lock in |other| is not released, and |this| must not be holding a lock.
  LeveledLock& operator=(LeveledLock&& other) noexcept;

  // Returns true if this object is holding a lock.
  bool is_locked() const { return !lock_released_callback_.is_null(); }

  // Explicitly releases the granted lock.
  //
  // The lock is also released implicitly when this instance is destroyed.
  // This method is idempotent, i.e. it's valid to call Release() on an
  // instance that does not hold a granted lock.
  void Release();

  int level() const { return level_; }
  const LeveledLockRange& range() const { return range_; }

 private:
  LeveledLockRange range_;
  int level_ = 0;
  // Closure to run when the lock is released. The lock is held when this is
  // non-null.
  LockReleasedCallback lock_released_callback_;
};

// Logging support.
COMPONENT_EXPORT(LOCK_MANAGER)
std::ostream& operator<<(std::ostream& out, const LeveledLock& range);

// Equality doesn't take into account whether the lock 'is_locked()' or not,
// only the level and the range.
COMPONENT_EXPORT(LOCK_MANAGER)
bool operator==(const LeveledLock& x, const LeveledLock& y);
COMPONENT_EXPORT(LOCK_MANAGER)
bool operator!=(const LeveledLock& x, const LeveledLock& y);
// Comparison operator to allow sorting for locking / unlocking order.
COMPONENT_EXPORT(LOCK_MANAGER)
bool operator<(const LeveledLock& x, const LeveledLock& y);

}  // namespace content

#endif  // COMPONENTS_SERVICES_STORAGE_INDEXED_DB_LOCKS_LEVELED_LOCK_H_
