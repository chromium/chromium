// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/locks/all_apps_lock.h"

#include "chrome/browser/web_applications/locks/lock.h"
#include "components/services/storage/indexed_db/locks/partitioned_lock_manager.h"

namespace web_app {

AllAppsLockDescription::AllAppsLockDescription()
    : LockDescription({}, LockDescription::Type::kAllAppsLock) {}
AllAppsLockDescription::~AllAppsLockDescription() = default;

AllAppsLock::AllAppsLock(base::WeakPtr<WebAppLockManager> lock_manager,
                         std::unique_ptr<content::PartitionedLockHolder> holder)
    : Lock(std::move(holder), lock_manager), WithAppResources(lock_manager) {}
AllAppsLock::~AllAppsLock() = default;

}  // namespace web_app
