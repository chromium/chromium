// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/locks/with_app_resources.h"

#include "chrome/browser/web_applications/locks/web_app_lock_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"

namespace web_app {

WithAppResources::~WithAppResources() = default;

ExtensionsManager& WithAppResources::extensions_manager() {
  CHECK(lock_manager_);
  return lock_manager_->provider().extensions_manager();
}
IsolatedWebAppInstallationManager&
WithAppResources::isolated_web_app_installation_manager() {
  CHECK(lock_manager_);
  return lock_manager_->provider().isolated_web_app_installation_manager();
}
WebAppRegistrar& WithAppResources::registrar() {
  CHECK(lock_manager_);
  return lock_manager_->provider().registrar_unsafe();
}
WebAppSyncBridge& WithAppResources::sync_bridge() {
  CHECK(lock_manager_);
  return lock_manager_->provider().sync_bridge_unsafe();
}
WebAppInstallFinalizer& WithAppResources::install_finalizer() {
  CHECK(lock_manager_);
  return lock_manager_->provider().install_finalizer();
}
OsIntegrationManager& WithAppResources::os_integration_manager() {
  CHECK(lock_manager_);
  return lock_manager_->provider().os_integration_manager();
}
WebAppInstallManager& WithAppResources::install_manager() {
  CHECK(lock_manager_);
  return lock_manager_->provider().install_manager();
}
WebAppIconManager& WithAppResources::icon_manager() {
  CHECK(lock_manager_);
  return lock_manager_->provider().icon_manager();
}
WebAppTranslationManager& WithAppResources::translation_manager() {
  CHECK(lock_manager_);
  return lock_manager_->provider().translation_manager();
}
WebAppUiManager& WithAppResources::ui_manager() {
  CHECK(lock_manager_);
  return lock_manager_->provider().ui_manager();
}
WebAppOriginAssociationManager& WithAppResources::origin_association_manager() {
  CHECK(lock_manager_);
  return lock_manager_->provider().origin_association_manager();
}

WithAppResources::WithAppResources() = default;

void WithAppResources::GrantWithAppResources(WebAppLockManager& lock_manager) {
  lock_manager_ = lock_manager.GetWeakPtr();
}

}  // namespace web_app
