// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/locks/shared_web_contents_with_app_lock.h"

#include <memory>

#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/locks/lock.h"
#include "chrome/browser/web_applications/locks/shared_web_contents_lock.h"
#include "components/services/storage/indexed_db/locks/partitioned_lock_manager.h"

namespace web_app {

SharedWebContentsWithAppLockDescription::
    SharedWebContentsWithAppLockDescription(base::flat_set<AppId> app_ids)
    : LockDescription(std::move(app_ids),
                      LockDescription::Type::kAppAndWebContents) {}
SharedWebContentsWithAppLockDescription::
    ~SharedWebContentsWithAppLockDescription() = default;

SharedWebContentsWithAppLock::SharedWebContentsWithAppLock(
    base::WeakPtr<WebAppLockManager> lock_manager,
    std::unique_ptr<content::PartitionedLockHolder> holder,
    content::WebContents& shared_web_contents)
    : Lock(std::move(holder)),
      WithSharedWebContentsResources(lock_manager, shared_web_contents),
      WithAppResources(lock_manager) {}

SharedWebContentsWithAppLock::~SharedWebContentsWithAppLock() = default;

}  // namespace web_app
