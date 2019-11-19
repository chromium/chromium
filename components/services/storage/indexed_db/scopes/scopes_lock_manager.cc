// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/indexed_db/scopes/scopes_lock_manager.h"

#include <memory>
#include <utility>

#include "base/barrier_closure.h"
#include "base/bind.h"

namespace content {

ScopesLocksHolder::ScopesLocksHolder() = default;
ScopesLocksHolder::~ScopesLocksHolder() = default;

ScopesLockManager::ScopesLockManager() {}
ScopesLockManager::~ScopesLockManager() = default;

ScopesLockManager::ScopeLockRequest::ScopeLockRequest(int level,
                                                      ScopeLockRange range,
                                                      LockType type)
    : level(level), range(std::move(range)), type(type) {}

bool operator<(const ScopesLockManager::ScopeLockRequest& x,
               const ScopesLockManager::ScopeLockRequest& y) {
  if (x.level != y.level)
    return x.level < y.level;
  return x.range < y.range;
}

bool operator==(const ScopesLockManager::ScopeLockRequest& x,
                const ScopesLockManager::ScopeLockRequest& y) {
  return x.level == y.level && x.range == y.range && x.type == y.type;
}

bool operator!=(const ScopesLockManager::ScopeLockRequest& x,
                const ScopesLockManager::ScopeLockRequest& y) {
  return !(x == y);
}

}  // namespace content
