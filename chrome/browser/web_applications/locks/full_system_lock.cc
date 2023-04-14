// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/locks/full_system_lock.h"

#include <memory>

#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/locks/lock.h"
#include "components/services/storage/indexed_db/locks/partitioned_lock_manager.h"

namespace web_app {

FullSystemLockDescription::FullSystemLockDescription()
    : LockDescription({}, LockDescription::Type::kFullSystem) {}
FullSystemLockDescription::~FullSystemLockDescription() = default;

FullSystemLock::FullSystemLock(
    base::WeakPtr<WebAppLockManager> lock_manager,
    std::unique_ptr<content::PartitionedLockHolder> holder)
    : Lock(std::move(holder)), WithAppResources(std::move(lock_manager)) {}
FullSystemLock::~FullSystemLock() = default;
}  // namespace web_app
