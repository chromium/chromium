// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_LOCKS_WITH_APP_RESOURCES_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_LOCKS_WITH_APP_RESOURCES_H_

#include "base/memory/weak_ptr.h"

namespace web_app {

class ExtensionsManager;
class IsolatedWebAppInstallationManager;
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
// Note: Accessing resources before the lock is granted or after the
// WebAppProvider system has shutdown (or the profile has shut down) will
// CHECK-fail.
class WithAppResources {
 public:
  ~WithAppResources();

  // Will CHECK-fail if accessed before the lock is granted.
  ExtensionsManager& extensions_manager();
  // Will CHECK-fail if accessed before the lock is granted.
  IsolatedWebAppInstallationManager& isolated_web_app_installation_manager();
  // Will CHECK-fail if accessed before the lock is granted.
  WebAppRegistrar& registrar();
  // Will CHECK-fail if accessed before the lock is granted.
  WebAppSyncBridge& sync_bridge();
  // Will CHECK-fail if accessed before the lock is granted.
  WebAppInstallFinalizer& install_finalizer();
  // Will CHECK-fail if accessed before the lock is granted.
  OsIntegrationManager& os_integration_manager();
  // Will CHECK-fail if accessed before the lock is granted.
  WebAppInstallManager& install_manager();
  // Will CHECK-fail if accessed before the lock is granted.
  WebAppIconManager& icon_manager();
  // Will CHECK-fail if accessed before the lock is granted.
  WebAppTranslationManager& translation_manager();
  // Will CHECK-fail if accessed before the lock is granted.
  WebAppUiManager& ui_manager();
  // Will CHECK-fail if accessed before the lock is granted.
  WebAppOriginAssociationManager& origin_association_manager();

 protected:
  WithAppResources();

  void GrantWithAppResources(WebAppLockManager& lock_manager);

 private:
  base::WeakPtr<WebAppLockManager> lock_manager_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_LOCKS_WITH_APP_RESOURCES_H_
