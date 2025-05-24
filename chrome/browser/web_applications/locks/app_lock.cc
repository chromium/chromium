// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/locks/app_lock.h"

#include "chrome/browser/web_applications/locks/lock.h"
#include "chrome/browser/web_applications/locks/partitioned_lock_manager.h"

namespace web_app {

AppLockDescription::AppLockDescription(const webapps::AppId& app_id)
    : LockDescription({app_id}, LockDescription::Type::kApp) {}
AppLockDescription::AppLockDescription(base::flat_set<webapps::AppId> app_ids)
    : LockDescription(std::move(app_ids), LockDescription::Type::kApp) {}
AppLockDescription::AppLockDescription(AppLockDescription&&) = default;
AppLockDescription::~AppLockDescription() = default;

AppLock::AppLock() = default;
AppLock::~AppLock() = default;

void AppLock::GrantLock(WebAppLockManager& lock_manager) {
  GrantLockResources(lock_manager);
  GrantWithAppResources(lock_manager);
}

}  // namespace web_app
