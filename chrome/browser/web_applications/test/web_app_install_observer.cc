// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/web_app_install_observer.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"

namespace web_app {

WebAppInstallObserver::WebAppInstallObserver(AppRegistrar* registrar) {
  observer_.Add(registrar);
}
WebAppInstallObserver::WebAppInstallObserver(
    AppRegistrar* registrar,
    const std::set<AppId>& listening_for_install_app_ids,
    const std::set<AppId>& listening_for_uninstall_app_ids,
    const std::set<AppId>& listening_for_install_with_os_hooks_app_ids)
    : listening_for_install_app_ids_(listening_for_install_app_ids),
      listening_for_uninstall_app_ids_(listening_for_uninstall_app_ids),
      listening_for_install_with_os_hooks_app_ids_(
          listening_for_install_with_os_hooks_app_ids) {
  observer_.Add(registrar);
#if DCHECK_IS_ON()
  DCHECK(!listening_for_install_app_ids_.empty() ||
         !listening_for_uninstall_app_ids_.empty() ||
         !listening_for_install_with_os_hooks_app_ids_.empty());
  for (const AppId& id : listening_for_install_app_ids_) {
    DCHECK(!id.empty()) << "Cannot listen for empty ids.";
  }
  for (const AppId& id : listening_for_uninstall_app_ids_) {
    DCHECK(!id.empty()) << "Cannot listen for empty ids.";
  }
  for (const AppId& id : listening_for_install_with_os_hooks_app_ids_) {
    DCHECK(!id.empty()) << "Cannot listen for empty ids.";
  }
#endif
}

WebAppInstallObserver::WebAppInstallObserver(Profile* profile)
    : WebAppInstallObserver(
          &WebAppProviderBase::GetProviderBase(profile)->registrar()) {}

WebAppInstallObserver::WebAppInstallObserver(
    Profile* profile,
    const std::set<AppId>& listening_for_install_app_ids,
    const std::set<AppId>& listening_for_uninstall_app_id,
    const std::set<AppId>& listening_for_install_with_os_hooks_app_ids)
    : WebAppInstallObserver(
          &WebAppProviderBase::GetProviderBase(profile)->registrar(),
          listening_for_install_app_ids,
          listening_for_uninstall_app_id,
          listening_for_install_with_os_hooks_app_ids) {}

WebAppInstallObserver::~WebAppInstallObserver() = default;

// static
std::unique_ptr<WebAppInstallObserver>
WebAppInstallObserver::CreateInstallListener(
    Profile* registrar,
    const std::set<AppId>& listening_for_install_app_ids) {
  return base::WrapUnique(new WebAppInstallObserver(
      registrar, listening_for_install_app_ids, {}, {}));
}

// static
std::unique_ptr<WebAppInstallObserver>
WebAppInstallObserver::CreateInstallWithOsHooksListener(
    Profile* registrar,
    const std::set<AppId>& listening_for_install_with_os_hooks_app_ids) {
  return base::WrapUnique(new WebAppInstallObserver(
      registrar, {}, {}, listening_for_install_with_os_hooks_app_ids));
}

// static
std::unique_ptr<WebAppInstallObserver>
WebAppInstallObserver::CreateUninstallListener(
    Profile* registrar,
    const std::set<AppId>& listening_for_uninstall_app_ids) {
  return base::WrapUnique(new WebAppInstallObserver(
      registrar, {}, listening_for_uninstall_app_ids, {}));
}

AppId WebAppInstallObserver::AwaitNextInstall() {
  base::RunLoop loop;
  AppId id;
  DCHECK(single_app_installed_delegate_.is_null());
  single_app_installed_delegate_ =
      base::BindLambdaForTesting([&](const AppId& app_id) {
        id = app_id;
        loop.Quit();
      });
  loop.Run();
  return id;
}

// static
AppId WebAppInstallObserver::AwaitNextUninstall(
    WebAppInstallObserver* install_observer) {
  base::RunLoop loop;
  AppId id;
  DCHECK(install_observer->single_app_uninstalled_delegate_.is_null());
  install_observer->single_app_uninstalled_delegate_ =
      base::BindLambdaForTesting([&](const AppId& app_id) {
        id = app_id;
        loop.Quit();
      });
  loop.Run();
  return id;
}

AppId WebAppInstallObserver::AwaitAllInstalls() {
  base::RunLoop loop;
  AppId id;
  DCHECK(all_apps_installed_delegate_.is_null());
  SetWebAppInstalledDelegate(
      base::BindLambdaForTesting([&](const AppId& app_id) {
        id = app_id;
        loop.Quit();
      }));
  loop.Run();
  return id;
}

// static
AppId WebAppInstallObserver::AwaitAllUninstalls(
    WebAppInstallObserver* install_observer) {
  base::RunLoop loop;
  AppId id;
  DCHECK(install_observer->app_uninstalled_delegate_.is_null());
  install_observer->SetWebAppUninstalledDelegate(
      base::BindLambdaForTesting([&](const AppId& app_id) {
        id = app_id;
        loop.Quit();
      }));
  loop.Run();
  return id;
}

void WebAppInstallObserver::SetWebAppInstalledDelegate(
    WebAppInstalledDelegate delegate) {
  all_apps_installed_delegate_ = delegate;
}

void WebAppInstallObserver::SetWebAppInstalledWithOsHooksDelegate(
    WebAppInstalledWithOsHooksDelegate delegate) {
  app_installed_with_os_hooks_delegate_ = delegate;
}

void WebAppInstallObserver::SetWebAppWillBeUninstalledDelegate(
    WebAppWillBeUninstalledDelegate delegate) {
  app_will_be_uninstalled_delegate_ = delegate;
}

void WebAppInstallObserver::SetWebAppUninstalledDelegate(
    WebAppUninstalledDelegate delegate) {
  app_uninstalled_delegate_ = delegate;
}

void WebAppInstallObserver::SetWebAppProfileWillBeDeletedDelegate(
    WebAppProfileWillBeDeletedDelegate delegate) {
  app_profile_will_be_deleted_delegate_ = delegate;
}

void WebAppInstallObserver::SetWebAppWillBeUpdatedFromSyncDelegate(
    WebAppWillBeUpdatedFromSyncDelegate delegate) {
  app_will_be_updated_from_sync_delegate_ = delegate;
}

const std::set<AppId>& WebAppInstallObserver::listening_for_install_app_ids() {
  return listening_for_install_app_ids_;
}

const std::set<AppId>&
WebAppInstallObserver::listening_for_uninstall_app_ids() {
  return listening_for_uninstall_app_ids_;
}

void WebAppInstallObserver::OnWebAppInstalled(const AppId& app_id) {
  listening_for_install_app_ids_.erase(app_id);
  if (!listening_for_install_app_ids_.empty())
    return;

  if (single_app_installed_delegate_)
    std::move(single_app_installed_delegate_).Run(app_id);

  if (all_apps_installed_delegate_)
    all_apps_installed_delegate_.Run(app_id);
}

void WebAppInstallObserver::OnWebAppInstalledWithOsHooks(const AppId& app_id) {
  listening_for_install_with_os_hooks_app_ids_.erase(app_id);
  if (!listening_for_install_with_os_hooks_app_ids_.empty())
    return;

  if (app_installed_with_os_hooks_delegate_)
    app_installed_with_os_hooks_delegate_.Run(app_id);
}

void WebAppInstallObserver::OnWebAppsWillBeUpdatedFromSync(
    const std::vector<const WebApp*>& new_apps_state) {
  if (app_will_be_updated_from_sync_delegate_)
    app_will_be_updated_from_sync_delegate_.Run(new_apps_state);
}

void WebAppInstallObserver::OnWebAppWillBeUninstalled(const AppId& app_id) {
  if (app_will_be_uninstalled_delegate_)
    app_will_be_uninstalled_delegate_.Run(app_id);
}

void WebAppInstallObserver::OnWebAppUninstalled(const AppId& app_id) {
  listening_for_uninstall_app_ids_.erase(app_id);
  if (!listening_for_uninstall_app_ids_.empty())
    return;

  if (app_uninstalled_delegate_)
    app_uninstalled_delegate_.Run(app_id);
}

void WebAppInstallObserver::OnWebAppProfileWillBeDeleted(const AppId& app_id) {
  if (app_profile_will_be_deleted_delegate_)
    app_profile_will_be_deleted_delegate_.Run(app_id);
}

AppId AwaitNextInstallWithOsHooks(Profile* registrar,
                                  const std::set<AppId>& app_ids) {
  base::RunLoop loop;
  AppId id;
  std::unique_ptr<WebAppInstallObserver> observer;
  if (!app_ids.empty()) {
    observer = WebAppInstallObserver::CreateInstallWithOsHooksListener(
        registrar, app_ids);
  } else {
    observer = std::make_unique<WebAppInstallObserver>(registrar);
  }
  observer->SetWebAppInstalledWithOsHooksDelegate(
      base::BindLambdaForTesting([&](const AppId& app_id) {
        id = app_id;
        loop.Quit();
      }));
  loop.Run();
  return id;
}

}  // namespace web_app
