// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/indexed_db/locks/partitioned_lock_manager.h"

#include <memory>
#include <utility>

#include "base/barrier_closure.h"
#include "base/bind.h"

namespace content {

PartitionedLockHolder::PartitionedLockHolder() = default;
PartitionedLockHolder::~PartitionedLockHolder() = default;

PartitionedLockManager::PartitionedLockManager() = default;
PartitionedLockManager::~PartitionedLockManager() = default;

PartitionedLockManager::PartitionedLockRequest::PartitionedLockRequest(
    PartitionedLockId lock_id,
    LockType type)
    : lock_id(std::move(lock_id)), type(type) {}

bool operator<(const PartitionedLockManager::PartitionedLockRequest& x,
               const PartitionedLockManager::PartitionedLockRequest& y) {
  if (x.lock_id != y.lock_id)
    return x.lock_id < y.lock_id;
  return x.type < y.type;
}

bool operator==(const PartitionedLockManager::PartitionedLockRequest& x,
                const PartitionedLockManager::PartitionedLockRequest& y) {
  return x.lock_id == y.lock_id && x.type == y.type;
}

bool operator!=(const PartitionedLockManager::PartitionedLockRequest& x,
                const PartitionedLockManager::PartitionedLockRequest& y) {
  return !(x == y);
}

}  // namespace content
