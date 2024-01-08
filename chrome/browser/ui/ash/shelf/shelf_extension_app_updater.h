// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SHELF_SHELF_EXTENSION_APP_UPDATER_H_
#define CHROME_BROWSER_UI_ASH_SHELF_SHELF_EXTENSION_APP_UPDATER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/ash/shelf/shelf_app_updater.h"
#include "extensions/browser/extension_registry_observer.h"

class ShelfExtensionAppUpdater : public ShelfAppUpdater,
                                 public extensions::ExtensionRegistryObserver,
                                 public ArcAppListPrefs::Observer {
 public:
  ShelfExtensionAppUpdater(Delegate* delegate,
                           content::BrowserContext* browser_context,
                           bool extensions_only);

  ShelfExtensionAppUpdater(const ShelfExtensionAppUpdater&) = delete;
  ShelfExtensionAppUpdater& operator=(const ShelfExtensionAppUpdater&) = delete;

  ~ShelfExtensionAppUpdater() override;

  // ExtensionRegistryObserver:
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const extensions::Extension* extension) override;
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const extensions::Extension* extension,
                           extensions::UnloadedExtensionReason reason) override;
  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const extensions::Extension* extension,
                              extensions::UninstallReason reason) override;
  void OnShutdown(extensions::ExtensionRegistry* registry) override;

  // ArcAppListPrefs::Observer
  void OnPackageInstalled(
      const arc::mojom::ArcPackageInfo& package_info) override;
  void OnPackageRemoved(const std::string& package_name,
                        bool uninstalled) override;
  void OnPackageListInitialRefreshed() override;

 private:
  void StartObservingExtensionRegistry();
  void StopObservingExtensionRegistry();

  void UpdateApp(const std::string& app_id);
  void UpdateEquivalentApp(const std::string& arc_package_name);

  bool ShouldHandleExtension(const extensions::Extension* extension) const;

  raw_ptr<extensions::ExtensionRegistry> extension_registry_ = nullptr;

  // Handles life-cycle events for extensions only if true, otherwise handles
  // life-cycle events for both Chrome apps and extensions.
  const bool extensions_only_;
};

#endif  // CHROME_BROWSER_UI_ASH_SHELF_SHELF_EXTENSION_APP_UPDATER_H_
