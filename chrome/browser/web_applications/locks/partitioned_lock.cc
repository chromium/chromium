// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/locks/partitioned_lock.h"

#include <ostream>

namespace web_app {

PartitionedLock::~PartitionedLock() {
  Release();
}

PartitionedLock::PartitionedLock(PartitionedLock&& other) noexcept {
  DCHECK(!this->is_locked())
      << "Cannot move a lock onto an active lock: " << *this;
  this->lock_id_ = std::move(other.lock_id_);
  this->lock_released_callback_ = std::move(other.lock_released_callback_);
  this->request_location_ = std::move(other.request_location_);
  DCHECK(!other.is_locked());
}
PartitionedLock::PartitionedLock(PartitionedLockId lock_id,
                                 base::Location request_location,
                                 LockReleasedCallback lock_released_callback,
                                 base::PassKey<PartitionedLockManager>)
    : lock_id_(std::move(lock_id)),
      request_location_(std::move(request_location)),
      lock_released_callback_(std::move(lock_released_callback)) {}

PartitionedLock& PartitionedLock::operator=(PartitionedLock&& other) noexcept {
  DCHECK(!this->is_locked())
      << "Cannot move a lock onto an active lock: " << *this;
  this->lock_id_ = std::move(other.lock_id_);
  this->lock_released_callback_ = std::move(other.lock_released_callback_);
  this->request_location_ = std::move(other.request_location_);
  DCHECK(!other.is_locked());
  return *this;
}

void PartitionedLock::Release() {
  if (is_locked()) {
    std::move(lock_released_callback_).Run(lock_id_);
  }
}

std::ostream& operator<<(std::ostream& out, const PartitionedLock& lock) {
  return out << "<PartitionedLock>{is_locked_: " << lock.is_locked()
             << ", lock_id_: " << lock.lock_id()
             << ", request_location_: " << lock.request_location().ToString()
             << "}";
}

}  // namespace web_app
