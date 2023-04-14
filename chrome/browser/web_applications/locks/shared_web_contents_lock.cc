// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/locks/shared_web_contents_lock.h"

#include <memory>

#include "chrome/browser/web_applications/locks/lock.h"
#include "components/services/storage/indexed_db/locks/partitioned_lock_manager.h"

namespace web_app {

SharedWebContentsLockDescription::SharedWebContentsLockDescription()
    : LockDescription({}, LockDescription::Type::kBackgroundWebContents) {}
SharedWebContentsLockDescription::~SharedWebContentsLockDescription() = default;

SharedWebContentsLock::SharedWebContentsLock(
    base::WeakPtr<WebAppLockManager> lock_manager,
    std::unique_ptr<content::PartitionedLockHolder> holder,
    content::WebContents& shared_web_contents)
    : Lock(std::move(holder)),
      WithSharedWebContentsResources(std::move(lock_manager),
                                     shared_web_contents) {}
SharedWebContentsLock::~SharedWebContentsLock() = default;
}  // namespace web_app
