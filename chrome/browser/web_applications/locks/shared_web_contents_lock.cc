// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/locks/shared_web_contents_lock.h"

#include "chrome/browser/web_applications/locks/lock.h"

namespace web_app {

SharedWebContentsLockDescription::SharedWebContentsLockDescription()
    : LockDescription({}, LockDescription::Type::kBackgroundWebContents) {}
SharedWebContentsLockDescription::~SharedWebContentsLockDescription() = default;

WithSharedWebContentsResources::WithSharedWebContentsResources(
    base::WeakPtr<WebAppLockManager> lock_manager,
    content::WebContents& shared_web_contents)
    : lock_manager_(std::move(lock_manager)),
      shared_web_contents_(shared_web_contents) {}
WithSharedWebContentsResources::~WithSharedWebContentsResources() = default;

content::WebContents& WithSharedWebContentsResources::shared_web_contents()
    const {
  CHECK(lock_manager_);
  return *shared_web_contents_;
}

SharedWebContentsLock::SharedWebContentsLock(
    base::WeakPtr<WebAppLockManager> lock_manager,
    std::unique_ptr<content::PartitionedLockHolder> holder,
    content::WebContents& shared_web_contents)
    : Lock(std::move(holder)),
      WithSharedWebContentsResources(std::move(lock_manager),
                                     shared_web_contents) {}
SharedWebContentsLock::~SharedWebContentsLock() = default;
}  // namespace web_app
