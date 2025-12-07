// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_LOCKS_PARTITIONED_LOCK_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_LOCKS_PARTITIONED_LOCK_H_

#include <iosfwd>

#include "base/functional/callback.h"
#include "base/location.h"
#include "base/types/pass_key.h"
#include "chrome/browser/web_applications/locks/partitioned_lock_id.h"

namespace web_app {
class PartitionedLockManager;

// Represents a granted lock by the PartitionedLockManager. When this object is
// destroyed, the lock is released. Since default construction is supported,
// |is_locked()| can be used to inquire locked status. Also, |Release()| can
// be called to manually release the lock, which appropriately updates the
// |is_locked()| result.
class PartitionedLock {
 public:
  using LockReleasedCallback =
      base::OnceCallback<void(PartitionedLockId lock_id)>;

  PartitionedLock(const PartitionedLock&) = delete;
  PartitionedLock& operator=(const PartitionedLock&) = delete;

  // |lock_released_callback| is called when the lock is released, either by
  // destruction of this object or by the |Released()| call. It will be called
  // synchronously on the sequence runner this lock is released on.
  PartitionedLock(PartitionedLockId lock_id,
                  base::Location request_location,
                  LockReleasedCallback lock_released_callback,
                  base::PassKey<PartitionedLockManager>);
  ~PartitionedLock();

  PartitionedLock(PartitionedLock&& other) noexcept;
  // The lock in `other` is not released (instead it is moved to `this`),
  // and `this` must not be holding a lock.
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

  const base::Location& request_location() const { return request_location_; }

 private:
  PartitionedLockId lock_id_;
  base::Location request_location_;

  // Closure to run when the lock is released. The lock is held when this is
  // non-null.
  LockReleasedCallback lock_released_callback_;
};

// Logging support.
std::ostream& operator<<(std::ostream& out, const PartitionedLock& lock_id);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_LOCKS_PARTITIONED_LOCK_H_
