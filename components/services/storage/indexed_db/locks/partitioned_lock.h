// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_INDEXED_DB_LOCKS_PARTITIONED_LOCK_H_
#define COMPONENTS_SERVICES_STORAGE_INDEXED_DB_LOCKS_PARTITIONED_LOCK_H_

#include <iosfwd>

#include "base/functional/callback.h"
#include "components/services/storage/indexed_db/locks/partitioned_lock_id.h"

namespace content::indexed_db {

// Represents a granted lock by the PartitionedLockManager. When this object is
// destroyed, the lock is released. Since default construction is supported,
// |is_locked()| can be used to inquire locked status. Also, |Release()| can
// be called to manually release the lock, which appropriately updates the
// |is_locked()| result.
class PartitionedLock {
 public:
  using LockReleasedCallback =
      base::OnceCallback<void(PartitionedLockId lock_id)>;

  PartitionedLock();

  PartitionedLock(const PartitionedLock&) = delete;
  PartitionedLock& operator=(const PartitionedLock&) = delete;

  ~PartitionedLock();
  PartitionedLock(PartitionedLock&&) noexcept;
  // |lock_released_callback| is called when the lock is released, either by
  // destruction of this object or by the |Released()| call. It will be called
  // synchronously on the sequence runner this lock is released on.
  PartitionedLock(PartitionedLockId lock_id,
                  LockReleasedCallback lock_released_callback);
  // The lock in |other| is not released, and |this| must not be holding a lock.
  PartitionedLock& operator=(PartitionedLock&& other) noexcept;

  // Returns true if this object is holding a lock.
  bool is_locked() const { return !lock_released_callback_.is_null(); }

  // Explicitly releases the granted lock.
  //
  // The lock is also released implicitly when this instance is destroyed.
  // This method is idempotent, i.e. it's valid to call Release() on an
  // instance that does not hold a granted lock.
  void Release();

  const PartitionedLockId& lock_id() const { return lock_id_; }

 private:
  PartitionedLockId lock_id_;

  // Closure to run when the lock is released. The lock is held when this is
  // non-null.
  LockReleasedCallback lock_released_callback_;
};

// Logging support.
std::ostream& operator<<(std::ostream& out, const PartitionedLock& lock_id);

// Equality doesn't take into account whether the lock 'is_locked()' or not,
// only the partition and the lock_id.
bool operator==(const PartitionedLock& x, const PartitionedLock& y);
bool operator!=(const PartitionedLock& x, const PartitionedLock& y);
// Comparison operator to allow sorting for locking / unlocking order.
bool operator<(const PartitionedLock& x, const PartitionedLock& y);

}  // namespace content::indexed_db

#endif  // COMPONENTS_SERVICES_STORAGE_INDEXED_DB_LOCKS_PARTITIONED_LOCK_H_
