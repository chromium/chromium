// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/shelf/shelf_extension_app_updater.h"

#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/extensions/gfx_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/unloaded_extension_reason.h"

ShelfExtensionAppUpdater::ShelfExtensionAppUpdater(
    Delegate* delegate,
    content::BrowserContext* browser_context,
    bool extensions_only)
    : ShelfAppUpdater(delegate, browser_context),
      extensions_only_(extensions_only) {
  StartObservingExtensionRegistry();

  if (extensions_only)
    return;

  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(browser_context);
  if (prefs)
    prefs->AddObserver(this);
}

ShelfExtensionAppUpdater::~ShelfExtensionAppUpdater() {
  StopObservingExtensionRegistry();

  if (extensions_only_)
    return;

  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(browser_context());
  if (prefs)
    prefs->RemoveObserver(this);
}

void ShelfExtensionAppUpdater::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension) {
  if (ShouldHandleExtension(extension))
    delegate()->OnAppInstalled(browser_context, extension->id());
}

void ShelfExtensionAppUpdater::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UnloadedExtensionReason reason) {
  if (!ShouldHandleExtension(extension))
    return;

  if (reason == extensions::UnloadedExtensionReason::UNINSTALL) {
    delegate()->OnAppUninstalledPrepared(browser_context, extension->id(),
                                         /*by_migration=*/false);
  } else {
    delegate()->OnAppUpdated(browser_context, extension->id(),
                             /*reload_icon=*/true);
  }
}

void ShelfExtensionAppUpdater::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UninstallReason reason) {
  if (ShouldHandleExtension(extension))
    delegate()->OnAppUninstalled(browser_context, extension->id());
}

void ShelfExtensionAppUpdater::OnShutdown(
    extensions::ExtensionRegistry* registry) {
  DCHECK_EQ(extension_registry_, registry);
  StopObservingExtensionRegistry();
}

void ShelfExtensionAppUpdater::OnPackageListInitialRefreshed() {
  const ArcAppListPrefs* prefs = ArcAppListPrefs::Get(browser_context());
  const std::vector<std::string> package_names = prefs->GetPackagesFromPrefs();
  for (const auto& package_name : package_names)
    UpdateEquivalentApp(package_name);
}

void ShelfExtensionAppUpdater::OnPackageInstalled(
    const arc::mojom::ArcPackageInfo& package_info) {
  UpdateEquivalentApp(package_info.package_name);
}

void ShelfExtensionAppUpdater::OnPackageRemoved(const std::string& package_name,
                                                bool uninstalled) {
  UpdateEquivalentApp(package_name);
}

void ShelfExtensionAppUpdater::StartObservingExtensionRegistry() {
  DCHECK(!extension_registry_);
  extension_registry_ = extensions::ExtensionRegistry::Get(browser_context());
  extension_registry_->AddObserver(this);
}

void ShelfExtensionAppUpdater::StopObservingExtensionRegistry() {
  if (!extension_registry_)
    return;
  extension_registry_->RemoveObserver(this);
  extension_registry_ = nullptr;
}

void ShelfExtensionAppUpdater::UpdateApp(const std::string& app_id) {
  delegate()->OnAppUpdated(browser_context(), app_id, /*reload_icon=*/true);
}

void ShelfExtensionAppUpdater::UpdateEquivalentApp(
    const std::string& arc_package_name) {
  const std::vector<std::string> extension_ids =
      extensions::util::GetEquivalentInstalledExtensions(browser_context(),
                                                         arc_package_name);
  for (const auto& iter : extension_ids)
    UpdateApp(iter);
}

bool ShelfExtensionAppUpdater::ShouldHandleExtension(
    const extensions::Extension* extension) const {
  return !extensions_only_ || extension->is_extension();
}
