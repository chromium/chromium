// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/locks/app_lock.h"

#include "chrome/browser/web_applications/locks/lock.h"
#include "components/services/storage/indexed_db/locks/partitioned_lock_manager.h"

namespace web_app {

AppLockDescription::AppLockDescription(const AppId& app_id)
    : LockDescription({app_id}, LockDescription::Type::kApp) {}
AppLockDescription::AppLockDescription(base::flat_set<AppId> app_ids)
    : LockDescription(std::move(app_ids), LockDescription::Type::kApp) {}
AppLockDescription::~AppLockDescription() = default;

AppLock::AppLock(base::WeakPtr<WebAppLockManager> lock_manager,
                 std::unique_ptr<content::PartitionedLockHolder> holder)
    : Lock(std::move(holder)), WithAppResources(std::move(lock_manager)) {}
AppLock::~AppLock() = default;

}  // namespace web_app
