// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/shelf/app_service/shelf_app_service_shortcut_updater.h"

#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/services/app_service/public/cpp/shortcut/shortcut_registry_cache.h"
#include "components/services/app_service/public/cpp/shortcut/shortcut_update.h"

ShelfAppServiceShortcutUpdater::ShelfAppServiceShortcutUpdater(
    Delegate* delegate,
    Profile* profile)
    : ShelfAppUpdater(delegate, profile) {
  shortcut_registry_cache_observation_.Observe(
      apps::AppServiceProxyFactory::GetForProfile(profile)
          ->ShortcutRegistryCache());
}

ShelfAppServiceShortcutUpdater::~ShelfAppServiceShortcutUpdater() = default;

void ShelfAppServiceShortcutUpdater::OnShortcutUpdated(
    const apps::ShortcutUpdate& update) {
  CHECK(delegate());
  delegate()->OnShortcutUpdated(update);
}

void ShelfAppServiceShortcutUpdater::OnShortcutRemoved(
    const apps::ShortcutId& id) {
  CHECK(delegate());
  delegate()->OnShortcutRemoved(id);
}

void ShelfAppServiceShortcutUpdater::OnShortcutRegistryCacheWillBeDestroyed(
    apps::ShortcutRegistryCache* cache) {
  shortcut_registry_cache_observation_.Reset();
}
