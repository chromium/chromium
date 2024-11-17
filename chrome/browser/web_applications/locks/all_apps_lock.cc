// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/locks/all_apps_lock.h"

#include "chrome/browser/web_applications/locks/lock.h"
#include "chrome/browser/web_applications/locks/partitioned_lock_manager.h"

namespace web_app {

AllAppsLockDescription::AllAppsLockDescription()
    : LockDescription({}, LockDescription::Type::kAllAppsLock) {}
AllAppsLockDescription::AllAppsLockDescription(AllAppsLockDescription&&) =
    default;
AllAppsLockDescription::~AllAppsLockDescription() = default;

AllAppsLock::AllAppsLock() = default;
AllAppsLock::~AllAppsLock() = default;

void AllAppsLock::GrantLock(WebAppLockManager& lock_manager) {
  GrantLockResources(lock_manager);
  GrantWithAppResources(lock_manager);
}

}  // namespace web_app
