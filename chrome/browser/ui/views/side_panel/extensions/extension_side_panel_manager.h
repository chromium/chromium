// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_EXTENSIONS_EXTENSION_SIDE_PANEL_MANAGER_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_EXTENSIONS_EXTENSION_SIDE_PANEL_MANAGER_H_

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/supports_user_data.h"
#include "chrome/browser/ui/views/side_panel/extensions/extension_side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry_observer.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/extension_id.h"

class Browser;
class SidePanelRegistry;

namespace content {
class BrowserContext;
}

namespace extensions {

class Extension;

// ExtensionSidePanelManager manages ExtensionSidePanelCoordinators for all
// extensions that can display side panel content in a map and updates the map
// when extensions are loaded or unloaded. Registration of an extension's
// SidePanelEntry and creating the view to be shown are delegated to each
// extension's ExtensionSidePanelCoordinator.
class ExtensionSidePanelManager : public SidePanelRegistryObserver,
                                  public extensions::ExtensionRegistryObserver,
                                  public base::SupportsUserData::Data {
 public:
  ExtensionSidePanelManager(const ExtensionSidePanelManager&) = delete;
  ExtensionSidePanelManager& operator=(const ExtensionSidePanelManager&) =
      delete;
  ~ExtensionSidePanelManager() override;

  static ExtensionSidePanelManager* GetOrCreateForBrowser(Browser* browser);

  ExtensionSidePanelCoordinator* GetExtensionCoordinatorForTesting(
      const ExtensionId& extension_id);

  // Called when the BrowserView for `browser_` is being created. Creates
  // ExtensionSidePanelCoordinators (which in turn, registers extension
  // SidePanelEntries) for all enabled extensions that are capable of hosting
  // side panel content.
  void RegisterExtensionEntries();

  // ExtensionRegistryObserver implementation.
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const Extension* extension) override;
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override;

  // SidePanelRegistryObserver implementation.
  void OnRegistryDestroying(SidePanelRegistry* registry) override;

 private:
  ExtensionSidePanelManager(Browser* browser,
                            SidePanelRegistry* global_registry);

  // Creates an ExtensionSidePanelCoordinator for `extension` and adds it to
  // `coordinators_` if the extension is capable of hosting side panel content.
  void MaybeCreateExtensionSidePanelCoordinator(const Extension* extension);

  raw_ptr<Browser> browser_;
  raw_ptr<SidePanelRegistry> global_registry_;

  base::flat_map<ExtensionId, std::unique_ptr<ExtensionSidePanelCoordinator>>
      coordinators_;

  base::ScopedObservation<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observation_{this};
  base::ScopedObservation<SidePanelRegistry, SidePanelRegistryObserver>
      side_panel_registry_observation_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_EXTENSIONS_EXTENSION_SIDE_PANEL_MANAGER_H_
