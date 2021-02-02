// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/launcher_extension_app_updater.h"

#include "chrome/browser/chromeos/extensions/gfx_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/unloaded_extension_reason.h"

LauncherExtensionAppUpdater::LauncherExtensionAppUpdater(
    Delegate* delegate,
    content::BrowserContext* browser_context,
    bool extensions_only)
    : LauncherAppUpdater(delegate, browser_context),
      extensions_only_(extensions_only) {
  StartObservingExtensionRegistry();

  if (extensions_only)
    return;

  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(browser_context);
  if (prefs)
    prefs->AddObserver(this);
}

LauncherExtensionAppUpdater::~LauncherExtensionAppUpdater() {
  StopObservingExtensionRegistry();

  if (extensions_only_)
    return;

  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(browser_context());
  if (prefs)
    prefs->RemoveObserver(this);
}

void LauncherExtensionAppUpdater::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension) {
  if (ShouldHandleExtension(extension))
    delegate()->OnAppInstalled(browser_context, extension->id());
}

void LauncherExtensionAppUpdater::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UnloadedExtensionReason reason) {
  if (!ShouldHandleExtension(extension))
    return;

  if (reason == extensions::UnloadedExtensionReason::UNINSTALL)
    delegate()->OnAppUninstalledPrepared(browser_context, extension->id());
  else
    delegate()->OnAppUpdated(browser_context, extension->id());
}

void LauncherExtensionAppUpdater::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UninstallReason reason) {
  if (ShouldHandleExtension(extension))
    delegate()->OnAppUninstalled(browser_context, extension->id());
}

void LauncherExtensionAppUpdater::OnShutdown(
    extensions::ExtensionRegistry* registry) {
  DCHECK_EQ(extension_registry_, registry);
  StopObservingExtensionRegistry();
}

void LauncherExtensionAppUpdater::OnPackageListInitialRefreshed() {
  const ArcAppListPrefs* prefs = ArcAppListPrefs::Get(browser_context());
  const std::vector<std::string> package_names = prefs->GetPackagesFromPrefs();
  for (const auto& package_name : package_names)
    UpdateEquivalentApp(package_name);
}

void LauncherExtensionAppUpdater::OnPackageInstalled(
    const arc::mojom::ArcPackageInfo& package_info) {
  UpdateEquivalentApp(package_info.package_name);
}

void LauncherExtensionAppUpdater::OnPackageRemoved(
    const std::string& package_name,
    bool uninstalled) {
  UpdateEquivalentApp(package_name);
}

void LauncherExtensionAppUpdater::StartObservingExtensionRegistry() {
  DCHECK(!extension_registry_);
  extension_registry_ = extensions::ExtensionRegistry::Get(browser_context());
  extension_registry_->AddObserver(this);
}

void LauncherExtensionAppUpdater::StopObservingExtensionRegistry() {
  if (!extension_registry_)
    return;
  extension_registry_->RemoveObserver(this);
  extension_registry_ = nullptr;
}

void LauncherExtensionAppUpdater::UpdateApp(const std::string& app_id) {
  delegate()->OnAppUpdated(browser_context(), app_id);
}

void LauncherExtensionAppUpdater::UpdateEquivalentApp(
    const std::string& arc_package_name) {
  const std::vector<std::string> extension_ids =
      extensions::util::GetEquivalentInstalledExtensions(browser_context(),
                                                         arc_package_name);
  for (const auto& iter : extension_ids)
    UpdateApp(iter);
}

bool LauncherExtensionAppUpdater::ShouldHandleExtension(
    const extensions::Extension* extension) const {
  return !extensions_only_ || extension->is_extension();
}
