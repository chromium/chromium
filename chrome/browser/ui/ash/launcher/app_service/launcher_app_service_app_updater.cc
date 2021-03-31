// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/app_service/launcher_app_service_app_updater.h"

#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "components/services/app_service/public/mojom/types.mojom.h"

LauncherAppServiceAppUpdater::LauncherAppServiceAppUpdater(
    Delegate* delegate,
    content::BrowserContext* browser_context)
    : LauncherAppUpdater(delegate, browser_context) {
  apps::AppServiceProxyChromeOs* proxy =
      apps::AppServiceProxyFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context));

  proxy->AppRegistryCache().ForEachApp([this](const apps::AppUpdate& update) {
    if (update.Readiness() == apps::mojom::Readiness::kReady)
      this->installed_apps_.insert(update.AppId());
  });
  Observe(&proxy->AppRegistryCache());
}

LauncherAppServiceAppUpdater::~LauncherAppServiceAppUpdater() = default;

void LauncherAppServiceAppUpdater::OnAppUpdate(const apps::AppUpdate& update) {
  if (!update.ReadinessChanged() && !update.PausedChanged() &&
      !update.ShowInShelfChanged()) {
    return;
  }

  const std::string& app_id = update.AppId();
  if (update.ReadinessChanged()) {
    std::set<std::string>::const_iterator it = installed_apps_.find(app_id);
    switch (update.Readiness()) {
      case apps::mojom::Readiness::kReady:
        if (it == installed_apps_.end()) {
          installed_apps_.insert(app_id);
        }
        delegate()->OnAppInstalled(browser_context(), app_id);
        return;
      case apps::mojom::Readiness::kUninstalledByUser:
        if (it != installed_apps_.end()) {
          installed_apps_.erase(it);
          delegate()->OnAppUninstalledPrepared(browser_context(), app_id);
          delegate()->OnAppUninstalled(browser_context(), app_id);
        }
        return;
      case apps::mojom::Readiness::kDisabledByPolicy:
        if (update.ShowInShelfChanged()) {
          OnShowInShelfChanged(app_id, (update.ShowInShelf() ==
                                        apps::mojom::OptionalBool::kTrue));
        } else {
          delegate()->OnAppUpdated(browser_context(), app_id);
        }
        return;
      default:
        delegate()->OnAppUpdated(browser_context(), app_id);
        return;
    }
  }

  if (update.PausedChanged()) {
    delegate()->OnAppUpdated(browser_context(), app_id);
    return;
  }

  if (update.ShowInShelfChanged() &&
      update.Readiness() == apps::mojom::Readiness::kDisabledByPolicy) {
    OnShowInShelfChanged(
        app_id, (update.ShowInShelf() == apps::mojom::OptionalBool::kTrue));
  }
}

void LauncherAppServiceAppUpdater::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  Observe(nullptr);
}

void LauncherAppServiceAppUpdater::OnShowInShelfChanged(
    const std::string& app_id,
    bool show_in_shelf) {
  std::set<std::string>::const_iterator it = installed_apps_.find(app_id);
  if (show_in_shelf) {
    if (it == installed_apps_.end()) {
      installed_apps_.insert(app_id);
      delegate()->OnAppInstalled(browser_context(), app_id);
    }
  } else {
    if (it != installed_apps_.end()) {
      installed_apps_.erase(it);
      delegate()->OnAppUninstalledPrepared(browser_context(), app_id);
    }
  }
}
