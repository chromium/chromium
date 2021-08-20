// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/web_app_test_registry_observer_adapter.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_provider.h"

namespace web_app {

WebAppTestRegistryObserverAdapter::WebAppTestRegistryObserverAdapter(
    WebAppRegistrar* registrar) {
  observation_.Observe(registrar);
}

WebAppTestRegistryObserverAdapter::WebAppTestRegistryObserverAdapter(
    Profile* profile)
    : WebAppTestRegistryObserverAdapter(
          &WebAppProvider::GetForTest(profile)->registrar()) {}

WebAppTestRegistryObserverAdapter::~WebAppTestRegistryObserverAdapter() =
    default;

void WebAppTestRegistryObserverAdapter::SetWebAppInstalledDelegate(
    WebAppInstalledDelegate delegate) {
  app_installed_delegate_ = delegate;
}

void WebAppTestRegistryObserverAdapter::SetWebAppInstalledWithOsHooksDelegate(
    WebAppInstalledWithOsHooksDelegate delegate) {
  app_installed_with_os_hooks_delegate_ = delegate;
}

void WebAppTestRegistryObserverAdapter::SetWebAppWillBeUninstalledDelegate(
    WebAppWillBeUninstalledDelegate delegate) {
  app_will_be_uninstalled_delegate_ = delegate;
}

void WebAppTestRegistryObserverAdapter::SetWebAppUninstalledDelegate(
    WebAppUninstalledDelegate delegate) {
  app_uninstalled_delegate_ = delegate;
}

void WebAppTestRegistryObserverAdapter::SetWebAppProfileWillBeDeletedDelegate(
    WebAppProfileWillBeDeletedDelegate delegate) {
  app_profile_will_be_deleted_delegate_ = delegate;
}

void WebAppTestRegistryObserverAdapter::SetWebAppWillBeUpdatedFromSyncDelegate(
    WebAppWillBeUpdatedFromSyncDelegate delegate) {
  app_will_be_updated_from_sync_delegate_ = delegate;
}

void WebAppTestRegistryObserverAdapter::SetWebAppManifestUpdateDelegate(
    WebAppManifestUpdateDelegate delegate) {
  app_manifest_updated_delegate_ = delegate;
}

void WebAppTestRegistryObserverAdapter::OnWebAppInstalled(const AppId& app_id) {
  if (app_installed_delegate_)
    app_installed_delegate_.Run(app_id);
}

void WebAppTestRegistryObserverAdapter::OnWebAppInstalledWithOsHooks(
    const AppId& app_id) {
  if (app_installed_with_os_hooks_delegate_)
    app_installed_with_os_hooks_delegate_.Run(app_id);
}

void WebAppTestRegistryObserverAdapter::OnWebAppsWillBeUpdatedFromSync(
    const std::vector<const WebApp*>& new_apps_state) {
  if (app_will_be_updated_from_sync_delegate_)
    app_will_be_updated_from_sync_delegate_.Run(new_apps_state);
}

void WebAppTestRegistryObserverAdapter::OnWebAppWillBeUninstalled(
    const AppId& app_id) {
  if (app_will_be_uninstalled_delegate_)
    app_will_be_uninstalled_delegate_.Run(app_id);
}

void WebAppTestRegistryObserverAdapter::OnWebAppUninstalled(
    const AppId& app_id) {
  if (app_uninstalled_delegate_)
    app_uninstalled_delegate_.Run(app_id);
}

void WebAppTestRegistryObserverAdapter::OnWebAppProfileWillBeDeleted(
    const AppId& app_id) {
  if (app_profile_will_be_deleted_delegate_)
    app_profile_will_be_deleted_delegate_.Run(app_id);
}

}  // namespace web_app
