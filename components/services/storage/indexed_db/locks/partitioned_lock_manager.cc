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
    int level,
    PartitionedLockRange range,
    LockType type)
    : level(level), range(std::move(range)), type(type) {}

bool operator<(const PartitionedLockManager::PartitionedLockRequest& x,
               const PartitionedLockManager::PartitionedLockRequest& y) {
  if (x.level != y.level)
    return x.level < y.level;
  return x.range < y.range;
}

bool operator==(const PartitionedLockManager::PartitionedLockRequest& x,
                const PartitionedLockManager::PartitionedLockRequest& y) {
  return x.level == y.level && x.range == y.range && x.type == y.type;
}

bool operator!=(const PartitionedLockManager::PartitionedLockRequest& x,
                const PartitionedLockManager::PartitionedLockRequest& y) {
  return !(x == y);
}

}  // namespace content
