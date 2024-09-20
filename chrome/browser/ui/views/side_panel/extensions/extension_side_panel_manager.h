// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_EXTENSIONS_EXTENSION_SIDE_PANEL_MANAGER_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_EXTENSIONS_EXTENSION_SIDE_PANEL_MANAGER_H_

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/views/side_panel/extensions/extension_side_panel_coordinator.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/extension_id.h"
#include "ui/actions/action_id.h"

class Browser;
class Profile;
class SidePanelRegistry;

namespace content {
class BrowserContext;
class WebContents;
}

namespace extensions {

class Extension;

// ExtensionSidePanelManager manages ExtensionSidePanelCoordinators for all
// extensions that can display side panel content in a map and updates the map
// when extensions are loaded or unloaded. Registration of an extension's
// SidePanelEntry and creating the view to be shown are delegated to each
// extension's ExtensionSidePanelCoordinator.
class ExtensionSidePanelManager : public ExtensionRegistryObserver {
 public:
  ExtensionSidePanelManager(Browser* browser, SidePanelRegistry* registry);
  ExtensionSidePanelManager(Profile* profile,
                            content::WebContents* web_contents,
                            SidePanelRegistry* tab_registry);

  ExtensionSidePanelManager(const ExtensionSidePanelManager&) = delete;
  ExtensionSidePanelManager& operator=(const ExtensionSidePanelManager&) =
      delete;
  ~ExtensionSidePanelManager() override;

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

  // Called when the tab is about to be discarded.
  void WillDiscard();

 private:
  // Creates an ExtensionSidePanelCoordinator for `extension` and adds it to
  // `coordinators_` if the extension is capable of hosting side panel content.
  void MaybeCreateExtensionSidePanelCoordinator(const Extension* extension);
  // Removes the action item from the action manager for the extension.
  // This should be called on unloading an extension.
  void MaybeRemoveActionItemForExtension(const Extension* extension);
  // Creates an action item for this extension if it is not already created.
  // This is only valid for extensions that are capable of hosting side panel
  // content.
  void MaybeCreateActionItemForExtension(const Extension* extension);
  // Dynamically creates an action id for an extension if it does not exist.
  // This uses the `SidePanelEntry::Key.ToString()` method as an unique string.
  actions::ActionId GetOrCreateActionIdForExtension(const Extension* extension);
  // Callback responsible for initializing action items for all enabled
  // extensions. Triggered by the action manager's notification.
  void InitializeActions();

  // The profile associated with either `browser_` or `web_contents_`.
  raw_ptr<Profile> profile_;

  // The browser that this class is associated with, through its user data. An
  // instance of this class can only be associated with/in the user data of a
  // single browser or WebContents, not both at once. Only one of `browser_` or
  // `web_contents_` should be defined.
  raw_ptr<Browser> browser_;

  // The tab-based WebContents that this class is associated with, through its
  // user data.
  raw_ptr<content::WebContents> web_contents_;

  // The SidePanelRegistry that lives in the same user data that an instance of
  // this class lives in. Owns all extension entries managed by `coordinators_`.
  raw_ptr<SidePanelRegistry> registry_;

  base::flat_map<ExtensionId, std::unique_ptr<ExtensionSidePanelCoordinator>>
      coordinators_;

  // Whether this class is tab-scoped or window-scoped.
  const bool for_tab_;

  base::ScopedObservation<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observation_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_EXTENSIONS_EXTENSION_SIDE_PANEL_MANAGER_H_
