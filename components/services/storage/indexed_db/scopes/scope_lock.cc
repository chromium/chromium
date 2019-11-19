// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/indexed_db/scopes/scope_lock.h"

#include <ostream>

namespace content {

ScopeLock::ScopeLock() = default;

ScopeLock::~ScopeLock() {
  Release();
}

ScopeLock::ScopeLock(ScopeLock&& other) noexcept {
  DCHECK(!this->is_locked())
      << "Cannot move a lock onto an active lock: " << *this;
  this->range_ = std::move(other.range_);
  this->level_ = other.level_;
  this->lock_released_callback_ = std::move(other.lock_released_callback_);
  DCHECK(!other.is_locked());
}
ScopeLock::ScopeLock(ScopeLockRange range,
                     int level,
                     LockReleasedCallback lock_released_callback)
    : range_(std::move(range)),
      level_(level),
      lock_released_callback_(std::move(lock_released_callback)) {}

ScopeLock& ScopeLock::operator=(ScopeLock&& other) noexcept {
  DCHECK(!this->is_locked())
      << "Cannot move a lock onto an active lock: " << *this;
  this->range_ = std::move(other.range_);
  this->level_ = other.level_;
  this->lock_released_callback_ = std::move(other.lock_released_callback_);
  DCHECK(!other.is_locked());
  return *this;
}

void ScopeLock::Release() {
  if (is_locked())
    std::move(lock_released_callback_).Run(level_, range_);
}

std::ostream& operator<<(std::ostream& out, const ScopeLock& lock) {
  return out << "<ScopeLock>{is_locked_: " << lock.is_locked()
             << ", level_: " << lock.level_ << ", range_: " << lock.range_
             << "}";
}

bool operator<(const ScopeLock& x, const ScopeLock& y) {
  if (x.level_ != y.level_)
    return x.level_ < y.level_;
  if (x.range_.begin != y.range_.begin)
    return x.range_.begin < y.range_.begin;
  return x.range_.end < y.range_.end;
}
bool operator==(const ScopeLock& x, const ScopeLock& y) {
  return x.level_ == y.level_ && x.range_.begin == y.range_.begin &&
         x.range_.end == y.range_.end;
}
bool operator!=(const ScopeLock& x, const ScopeLock& y) {
  return !(x == y);
}

}  // namespace content
