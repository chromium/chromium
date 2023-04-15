// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/locks/noop_lock.h"

#include <memory>

#include "chrome/browser/web_applications/locks/lock.h"
#include "components/services/storage/indexed_db/locks/partitioned_lock_manager.h"

namespace web_app {

NoopLockDescription::NoopLockDescription()
    : LockDescription({}, LockDescription::Type::kNoOp) {}
NoopLockDescription::~NoopLockDescription() = default;

NoopLock::NoopLock(std::unique_ptr<content::PartitionedLockHolder> holder)
    : Lock(std::move(holder)) {}
NoopLock::~NoopLock() = default;

}  // namespace web_app
