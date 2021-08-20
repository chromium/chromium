// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_WEB_APP_TEST_REGISTRY_OBSERVER_ADAPTER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_WEB_APP_TEST_REGISTRY_OBSERVER_ADAPTER_H_

#include "base/callback.h"
#include "base/scoped_observation.h"
#include "chrome/browser/web_applications/components/app_registrar_observer.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/browser/web_applications/web_app_registrar.h"

class Profile;

namespace web_app {

class WebApp;

// This is an adapter for the AppRegistrarObserver. This class registers
// itself as an observer on construction, and will call the respective
// delegates (if set) for all AppRegistrarObserver calls.
class WebAppTestRegistryObserverAdapter : public AppRegistrarObserver {
 public:
  explicit WebAppTestRegistryObserverAdapter(WebAppRegistrar* registrar);
  explicit WebAppTestRegistryObserverAdapter(Profile* profile);

  WebAppTestRegistryObserverAdapter(const WebAppTestRegistryObserverAdapter&) =
      delete;
  WebAppTestRegistryObserverAdapter& operator=(
      const WebAppTestRegistryObserverAdapter&) = delete;

  ~WebAppTestRegistryObserverAdapter() override;

  using WebAppInstalledDelegate =
      base::RepeatingCallback<void(const AppId& app_id)>;
  void SetWebAppInstalledDelegate(WebAppInstalledDelegate delegate);

  using WebAppInstalledWithOsHooksDelegate =
      base::RepeatingCallback<void(const AppId& app_id)>;
  void SetWebAppInstalledWithOsHooksDelegate(
      WebAppInstalledWithOsHooksDelegate delegate);

  using WebAppWillBeUninstalledDelegate =
      base::RepeatingCallback<void(const AppId& app_id)>;
  void SetWebAppWillBeUninstalledDelegate(
      WebAppWillBeUninstalledDelegate delegate);

  using WebAppUninstalledDelegate =
      base::RepeatingCallback<void(const AppId& app_id)>;
  void SetWebAppUninstalledDelegate(WebAppUninstalledDelegate delegate);

  using WebAppProfileWillBeDeletedDelegate =
      base::RepeatingCallback<void(const AppId& app_id)>;
  void SetWebAppProfileWillBeDeletedDelegate(
      WebAppProfileWillBeDeletedDelegate delegate);

  using WebAppWillBeUpdatedFromSyncDelegate = base::RepeatingCallback<void(
      const std::vector<const WebApp*>& new_apps_state)>;
  void SetWebAppWillBeUpdatedFromSyncDelegate(
      WebAppWillBeUpdatedFromSyncDelegate delegate);

  using WebAppManifestUpdateDelegate =
      base::RepeatingCallback<void(const AppId& app_id)>;
  void SetWebAppManifestUpdateDelegate(WebAppManifestUpdateDelegate delegate);

  // AppRegistrarObserver:
  void OnWebAppInstalled(const AppId& app_id) override;
  void OnWebAppInstalledWithOsHooks(const AppId& app_id) override;
  void OnWebAppsWillBeUpdatedFromSync(
      const std::vector<const WebApp*>& new_apps_state) override;
  void OnWebAppWillBeUninstalled(const AppId& app_id) override;
  void OnWebAppUninstalled(const AppId& app_id) override;
  void OnWebAppProfileWillBeDeleted(const AppId& app_id) override;

 private:
  WebAppInstalledDelegate app_installed_delegate_;
  WebAppInstalledWithOsHooksDelegate app_installed_with_os_hooks_delegate_;
  WebAppWillBeUpdatedFromSyncDelegate app_will_be_updated_from_sync_delegate_;
  WebAppWillBeUninstalledDelegate app_will_be_uninstalled_delegate_;
  WebAppUninstalledDelegate app_uninstalled_delegate_;
  WebAppProfileWillBeDeletedDelegate app_profile_will_be_deleted_delegate_;
  WebAppManifestUpdateDelegate app_manifest_updated_delegate_;

  base::ScopedObservation<WebAppRegistrar, AppRegistrarObserver> observation_{
      this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_WEB_APP_TEST_REGISTRY_OBSERVER_ADAPTER_H_
