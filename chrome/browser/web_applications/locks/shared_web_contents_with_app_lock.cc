// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/locks/shared_web_contents_with_app_lock.h"

#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/locks/lock.h"
#include "chrome/browser/web_applications/locks/shared_web_contents_lock.h"

namespace web_app {

SharedWebContentsWithAppLockDescription::
    SharedWebContentsWithAppLockDescription(base::flat_set<AppId> app_ids)
    : LockDescription(std::move(app_ids),
                      LockDescription::Type::kAppAndWebContents) {}
SharedWebContentsWithAppLockDescription::
    ~SharedWebContentsWithAppLockDescription() = default;

SharedWebContentsWithAppLock::SharedWebContentsWithAppLock(
    std::unique_ptr<content::PartitionedLockHolder> holder,
    content::WebContents& shared_web_contents,
    WebAppRegistrar& registrar,
    WebAppSyncBridge& sync_bridge,
    WebAppInstallFinalizer& install_finalizer,
    OsIntegrationManager& os_integration_manager,
    WebAppInstallManager& install_manager,
    WebAppIconManager& icon_manager,
    WebAppTranslationManager& translation_manager,
    WebAppUiManager& ui_manager)
    : Lock(std::move(holder)),
      WithSharedWebContentsResources(shared_web_contents),
      WithAppResources(registrar,
                       sync_bridge,
                       install_finalizer,
                       os_integration_manager,
                       install_manager,
                       icon_manager,
                       translation_manager,
                       ui_manager) {}

SharedWebContentsWithAppLock::~SharedWebContentsWithAppLock() = default;

}  // namespace web_app
