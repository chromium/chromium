// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/locks/noop_lock.h"

#include <memory>

#include "chrome/browser/web_applications/locks/lock.h"
#include "chrome/browser/web_applications/locks/partitioned_lock_manager.h"
#include "chrome/browser/web_applications/locks/web_app_lock_manager.h"

namespace web_app {

NoopLockDescription::NoopLockDescription()
    : LockDescription({}, LockDescription::Type::kNoOp) {}
NoopLockDescription::NoopLockDescription(NoopLockDescription&&) = default;
NoopLockDescription::~NoopLockDescription() = default;

NoopLock::NoopLock() = default;
NoopLock::~NoopLock() = default;

void NoopLock::GrantLock(WebAppLockManager& lock_manager) {
  GrantLockResources(lock_manager);
}

}  // namespace web_app
