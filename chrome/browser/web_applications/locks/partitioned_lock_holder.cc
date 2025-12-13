// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/locks/partitioned_lock_holder.h"

#include <vector>

#include "base/memory/weak_ptr.h"

namespace web_app {

PartitionedLockHolder::PartitionedLockHolder() = default;
PartitionedLockHolder::~PartitionedLockHolder() = default;

base::WeakPtr<PartitionedLockHolder> PartitionedLockHolder::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void PartitionedLockHolder::Release() {
  locks_.clear();
  is_locked_ = false;
  weak_factory_.InvalidateWeakPtrs();
}

bool PartitionedLockHolder::is_locked() const {
  return is_locked_;
}

}  // namespace web_app
