// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/indexed_db/locks/partitioned_lock.h"

#include <ostream>

namespace content::indexed_db {

PartitionedLock::PartitionedLock() = default;

PartitionedLock::~PartitionedLock() {
  Release();
}

PartitionedLock::PartitionedLock(PartitionedLock&& other) noexcept {
  DCHECK(!this->is_locked())
      << "Cannot move a lock onto an active lock: " << *this;
  this->lock_id_ = std::move(other.lock_id_);
  this->lock_released_callback_ = std::move(other.lock_released_callback_);
  DCHECK(!other.is_locked());
}
PartitionedLock::PartitionedLock(PartitionedLockId range,
                                 LockReleasedCallback lock_released_callback)
    : lock_id_(std::move(range)),
      lock_released_callback_(std::move(lock_released_callback)) {}

PartitionedLock& PartitionedLock::operator=(PartitionedLock&& other) noexcept {
  DCHECK(!this->is_locked())
      << "Cannot move a lock onto an active lock: " << *this;
  this->lock_id_ = std::move(other.lock_id_);
  this->lock_released_callback_ = std::move(other.lock_released_callback_);
  DCHECK(!other.is_locked());
  return *this;
}

void PartitionedLock::Release() {
  if (is_locked())
    std::move(lock_released_callback_).Run(lock_id_);
}

std::ostream& operator<<(std::ostream& out, const PartitionedLock& lock) {
  return out << "<PartitionedLock>{is_locked_: " << lock.is_locked()
             << ", lock_id_: " << lock.lock_id() << "}";
}

bool operator<(const PartitionedLock& x, const PartitionedLock& y) {
  return x.lock_id() < y.lock_id();
}
bool operator==(const PartitionedLock& x, const PartitionedLock& y) {
  return x.lock_id() == y.lock_id();
}
bool operator!=(const PartitionedLock& x, const PartitionedLock& y) {
  return !(x == y);
}

}  // namespace content::indexed_db
