// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_LOCKS_WITH_APP_RESOURCES_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_LOCKS_WITH_APP_RESOURCES_H_

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"

namespace web_app {

class OsIntegrationManager;
class WebAppIconManager;
class WebAppInstallFinalizer;
class WebAppInstallManager;
class WebAppLockManager;
class WebAppRegistrar;
class WebAppSyncBridge;
class WebAppTranslationManager;
class WebAppUiManager;
class WebAppOriginAssociationManager;

// This gives access to web app components that allow read/write access to web
// apps. A lock class that needs read/read access to web apps can inherit from
// this class.
// Note: a future improvement could be to only give read/write access to a list
// of specific web apps.
//
// See `WebAppLockManager` for how to use locks. Destruction of this class will
// release the lock or cancel the lock request if it is not acquired yet.
//
// Note: Accessing a lock will CHECK-fail if the WebAppProvider system has
// shutdown (or the profile has shut down).
class WithAppResources {
 public:
  ~WithAppResources();

  WebAppRegistrar& registrar();
  WebAppSyncBridge& sync_bridge();
  WebAppInstallFinalizer& install_finalizer();
  OsIntegrationManager& os_integration_manager();
  WebAppInstallManager& install_manager();
  WebAppIconManager& icon_manager();
  WebAppTranslationManager& translation_manager();
  WebAppUiManager& ui_manager();
  WebAppOriginAssociationManager& origin_association_manager();

 protected:
  explicit WithAppResources(base::WeakPtr<WebAppLockManager> lock_manager);

 private:
  base::WeakPtr<WebAppLockManager> lock_manager_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_LOCKS_WITH_APP_RESOURCES_H_
