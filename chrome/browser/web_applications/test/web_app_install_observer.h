// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_WEB_APP_INSTALL_OBSERVER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_WEB_APP_INSTALL_OBSERVER_H_

#include <memory>
#include <set>

#include "base/callback.h"
#include "base/scoped_observer.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/app_registrar_observer.h"
#include "chrome/browser/web_applications/components/web_app_id.h"

namespace web_app {

class AppRegistrar;
class WebApp;

class WebAppInstallObserver final : public AppRegistrarObserver {
 public:
  explicit WebAppInstallObserver(AppRegistrar* registrar);
  explicit WebAppInstallObserver(Profile* profile);

  // Restricts this observer to only listen for the given
  // |listening_for_install_app_ids|. Settings these means that the
  // WebAppInstalledDelegate doesn't get called until all of the ids in
  // |listening_for_install_app_ids| are installed.
  static std::unique_ptr<WebAppInstallObserver> CreateInstallListener(
      Profile* registrar,
      const std::set<AppId>& listening_for_install_app_ids);

  // Restricts this observer to only listen for the given
  // |listening_for_install_with_os_hooks_app_ids|. Settings these means that
  // the WebAppInstalledWithOsHooksDelegate doesn't get called until all of the
  // ids in |listening_for_install_with_os_hooks_app_ids| are installed. This
  // also applies to AwaitNextInstallWithOsHooks().
  static std::unique_ptr<WebAppInstallObserver>
  CreateInstallWithOsHooksListener(
      Profile* registrar,
      const std::set<AppId>& listening_for_install_app_ids);

  // Restricts this observer to only listen for the given
  // |listening_for_uninstall_app_ids|. Settings these means that the
  // WebAppUninstalledDelegate doesn't get called until all of the ids in
  // |listening_for_uninstall_app_ids| are uninstalled.
  static std::unique_ptr<WebAppInstallObserver> CreateUninstallListener(
      Profile* registrar,
      const std::set<AppId>& listening_for_uninstall_app_ids);

  WebAppInstallObserver(const WebAppInstallObserver&) = delete;
  WebAppInstallObserver& operator=(const WebAppInstallObserver&) = delete;

  ~WebAppInstallObserver() override;

  // Convenience method to wait for the next install.
  AppId AwaitNextInstall();
  // Convenience method to wait for the next uninstall.
  static AppId AwaitNextUninstall(WebAppInstallObserver* observer);

  // Convenience method to wait for all installations of
  // |listening_for_install_app_ids| specified above. Calls
  // SetWebAppInstalledDelegate with a base::RunLoop quit closure, and then runs
  // the loop. Will DCHECK if an install delegate is already populated.
  // TODO(dmurph): Refactor to be static like AwaitNextUninstall().
  AppId AwaitAllInstalls();

  // Convenience method to wait for all uninstallations of
  // |listening_for_uninstall_app_ids| specified above. Calls
  // SetWebAppUninstalledDelegate with a base::RunLoop quit closure, and then
  // runs the loop. Will DCHECK if an uninstall delegate is already populated.
  static AppId AwaitAllUninstalls(WebAppInstallObserver* install_observer);

  using WebAppInstalledDelegate =
      base::RepeatingCallback<void(const AppId& app_id)>;
  using SingleAppInstalledDelegate =
      base::OnceCallback<void(const AppId& app_id)>;
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
  using SingleAppUninstalledDelegate =
      base::OnceCallback<void(const AppId& app_id)>;

  using WebAppProfileWillBeDeletedDelegate =
      base::RepeatingCallback<void(const AppId& app_id)>;
  void SetWebAppProfileWillBeDeletedDelegate(
      WebAppProfileWillBeDeletedDelegate delegate);

  using WebAppWillBeUpdatedFromSyncDelegate = base::RepeatingCallback<void(
      const std::vector<const WebApp*>& new_apps_state)>;
  void SetWebAppWillBeUpdatedFromSyncDelegate(
      WebAppWillBeUpdatedFromSyncDelegate delegate);

  const std::set<AppId>& listening_for_install_app_ids();
  const std::set<AppId>& listening_for_uninstall_app_ids();

  // AppRegistrarObserver:
  void OnWebAppInstalled(const AppId& app_id) override;
  void OnWebAppInstalledWithOsHooks(const AppId& app_id) override;
  void OnWebAppsWillBeUpdatedFromSync(
      const std::vector<const WebApp*>& new_apps_state) override;
  void OnWebAppWillBeUninstalled(const AppId& app_id) override;
  void OnWebAppUninstalled(const AppId& app_id) override;
  void OnWebAppProfileWillBeDeleted(const AppId& app_id) override;

 private:
  // Restricts this observer to only listen for the given
  // |listening_for_install_app_ids| and |listening_for_uninstall_app_ids|.
  // Settings these means that the WebAppInstalledDelegate or the
  // WebAppUninstalledDelegate don't get called until all of the ids in
  // |listening_for_install_app_ids| or |listening_for_uninstall_app_ids| are
  // installed or uninstalled (respectively).
  explicit WebAppInstallObserver(
      AppRegistrar* registrar,
      const std::set<AppId>& listening_for_install_app_ids,
      const std::set<AppId>& listening_for_uninstall_app_ids,
      const std::set<AppId>& listening_for_install_with_os_hooks_app_ids);
  explicit WebAppInstallObserver(
      Profile* profile,
      const std::set<AppId>& listening_for_install_app_ids,
      const std::set<AppId>& listening_for_uninstall_app_ids,
      const std::set<AppId>& listening_for_install_with_os_hooks_app_ids);

  std::set<AppId> listening_for_install_app_ids_;
  std::set<AppId> listening_for_uninstall_app_ids_;
  std::set<AppId> listening_for_install_with_os_hooks_app_ids_;

  WebAppInstalledDelegate all_apps_installed_delegate_;
  SingleAppInstalledDelegate single_app_installed_delegate_;
  WebAppInstalledWithOsHooksDelegate app_installed_with_os_hooks_delegate_;
  WebAppWillBeUpdatedFromSyncDelegate app_will_be_updated_from_sync_delegate_;
  WebAppWillBeUninstalledDelegate app_will_be_uninstalled_delegate_;
  WebAppUninstalledDelegate app_uninstalled_delegate_;
  SingleAppUninstalledDelegate single_app_uninstalled_delegate_;
  WebAppProfileWillBeDeletedDelegate app_profile_will_be_deleted_delegate_;

  ScopedObserver<AppRegistrar, AppRegistrarObserver> observer_{this};

};

// Convenience method to crreate an observer to wait for the next install
// finished with OS hooks deployment, or for all installations of |app_ids|.
AppId AwaitNextInstallWithOsHooks(Profile* registrar,
                                  const std::set<AppId>& app_ids = {});

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_WEB_APP_INSTALL_OBSERVER_H_
