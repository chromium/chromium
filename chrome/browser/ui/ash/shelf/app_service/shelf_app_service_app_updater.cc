// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/shelf/app_service/shelf_app_service_app_updater.h"

#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/app_update.h"

ShelfAppServiceAppUpdater::ShelfAppServiceAppUpdater(
    Delegate* delegate,
    content::BrowserContext* browser_context)
    : ShelfAppUpdater(delegate, browser_context) {
  apps::AppServiceProxy* proxy = apps::AppServiceProxyFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context));

  proxy->AppRegistryCache().ForEachApp([this](const apps::AppUpdate& update) {
    if (update.Readiness() == apps::Readiness::kReady)
      this->installed_apps_.insert(update.AppId());
  });
  Observe(&proxy->AppRegistryCache());
}

ShelfAppServiceAppUpdater::~ShelfAppServiceAppUpdater() = default;

void ShelfAppServiceAppUpdater::OnAppUpdate(const apps::AppUpdate& update) {
  if (!update.ReadinessChanged() && !update.PausedChanged() &&
      !update.ShowInShelfChanged() && !update.ShortNameChanged() &&
      !update.PolicyIdsChanged()) {
    return;
  }

  const std::string& app_id = update.AppId();
  if (update.ReadinessChanged()) {
    std::set<std::string>::const_iterator it = installed_apps_.find(app_id);
    switch (update.Readiness()) {
      case apps::Readiness::kReady:
        if (it == installed_apps_.end()) {
          installed_apps_.insert(app_id);
        }
        delegate()->OnAppInstalled(browser_context(), app_id);
        return;
      case apps::Readiness::kUninstalledByUser:
      case apps::Readiness::kUninstalledByMigration:
        if (it != installed_apps_.end()) {
          installed_apps_.erase(it);
          const bool by_migration =
              update.Readiness() == apps::Readiness::kUninstalledByMigration;
          delegate()->OnAppUninstalledPrepared(browser_context(), app_id,
                                               by_migration);
          delegate()->OnAppUninstalled(browser_context(), app_id);
        }
        return;
      case apps::Readiness::kDisabledByPolicy:
        if (update.ShowInShelfChanged()) {
          OnShowInShelfChangedForAppDisabledByPolicy(
              app_id, update.ShowInShelf().value_or(false));
        } else {
          delegate()->OnAppUpdated(browser_context(), app_id,
                                   /*reload_icon=*/true);
        }
        return;
      default:
        delegate()->OnAppUpdated(browser_context(), app_id,
                                 /*reload_icon=*/true);
        return;
    }
  }

  if (update.PolicyIdsChanged())
    delegate()->OnAppInstalled(browser_context(), app_id);

  if (update.ShowInShelfChanged()) {
    if (update.Readiness() == apps::Readiness::kDisabledByPolicy) {
      OnShowInShelfChangedForAppDisabledByPolicy(
          app_id, update.ShowInShelf().value_or(false));
    } else {
      delegate()->OnAppShowInShelfChanged(browser_context(), app_id,
                                          update.ShowInShelf().value_or(true));
    }
  }

  if (update.PausedChanged()) {
    delegate()->OnAppUpdated(browser_context(), app_id, /*reload_icon=*/true);
    return;
  }

  if (update.ShortNameChanged())
    delegate()->OnAppUpdated(browser_context(), app_id, /*reload_icon=*/false);
}

void ShelfAppServiceAppUpdater::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  Observe(nullptr);
}

void ShelfAppServiceAppUpdater::OnShowInShelfChangedForAppDisabledByPolicy(
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
      delegate()->OnAppUninstalledPrepared(browser_context(), app_id,
                                           /*by_migration=*/false);
    }
  }
}
