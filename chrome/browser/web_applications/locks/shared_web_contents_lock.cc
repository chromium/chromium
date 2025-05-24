// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/locks/shared_web_contents_lock.h"

#include <memory>

#include "chrome/browser/web_applications/locks/lock.h"
#include "chrome/browser/web_applications/locks/partitioned_lock_manager.h"

namespace web_app {

SharedWebContentsLockDescription::SharedWebContentsLockDescription()
    : LockDescription({}, LockDescription::Type::kBackgroundWebContents) {}
SharedWebContentsLockDescription::SharedWebContentsLockDescription(
    SharedWebContentsLockDescription&&) = default;
SharedWebContentsLockDescription::~SharedWebContentsLockDescription() = default;

SharedWebContentsLock::SharedWebContentsLock() = default;
SharedWebContentsLock::~SharedWebContentsLock() = default;

void SharedWebContentsLock::GrantLock(
    WebAppLockManager& lock_manager,
    content::WebContents& shared_web_contents) {
  GrantLockResources(lock_manager);
  GrantWithSharedWebContentsResources(lock_manager, shared_web_contents);
}

}  // namespace web_app
