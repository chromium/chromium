// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_EXTENSIONS_MENU_VIEW_MODEL_H_
#define CHROME_BROWSER_UI_EXTENSIONS_EXTENSIONS_MENU_VIEW_MODEL_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "extensions/browser/permissions_manager.h"
#include "extensions/common/extension_id.h"

namespace content {
class WebContents;
}  // namespace content

class BrowserWindowInterface;
class ExtensionsMenuViewPlatformDelegate;

// The platform agnostic controller for the extensions menu.
// TODO(crbug.com/449814184): Move the observers from
// ExtensionsMenuViewController here.
class ExtensionsMenuViewModel : public extensions::PermissionsManager::Observer,
                                public ToolbarActionsModel::Observer {
 public:
  ExtensionsMenuViewModel(
      BrowserWindowInterface* browser,
      std::unique_ptr<ExtensionsMenuViewPlatformDelegate> platform_delegate);
  ExtensionsMenuViewModel(const ExtensionsMenuViewModel&) = delete;
  const ExtensionsMenuViewModel& operator=(const ExtensionsMenuViewModel&) =
      delete;
  ~ExtensionsMenuViewModel() override;

  // Updates the extension's site access for the current site.
  void UpdateSiteAccess(
      const extensions::ExtensionId& extension_id,
      extensions::PermissionsManager::UserSiteAccess site_access);

  // Grants the extension site access to the current site.
  void GrantSiteAccess(const extensions::ExtensionId& extension_id);

  // Revokes the extension's site access from the current site.
  void RevokeSiteAccess(const extensions::ExtensionId& extension_id);

  // Update the extension's site setting for the current site.
  void UpdateSiteSetting(
      extensions::PermissionsManager::UserSiteSetting site_setting);

  // PermissionsManager::Observer:
  void OnHostAccessRequestAdded(const extensions::ExtensionId& extension_id,
                                int tab_id) override;

  // ToolbarActionsModel::Observer:
  void OnToolbarActionAdded(
      const ToolbarActionsModel::ActionId& action_id) override;
  void OnToolbarActionRemoved(
      const ToolbarActionsModel::ActionId& action_id) override;
  void OnToolbarActionUpdated(
      const ToolbarActionsModel::ActionId& action_id) override;
  void OnToolbarModelInitialized() override;
  void OnToolbarPinnedActionsChanged() override;

 private:
  content::WebContents* GetActiveWebContents();

  // The browser window that the extensions menu is in.
  raw_ptr<BrowserWindowInterface> browser_;

  // The delegate that handles platform-specific UI.
  std::unique_ptr<ExtensionsMenuViewPlatformDelegate> platform_delegate_;

  base::ScopedObservation<extensions::PermissionsManager,
                          extensions::PermissionsManager::Observer>
      permissions_manager_observation_{this};

  const raw_ptr<ToolbarActionsModel> toolbar_model_;
  base::ScopedObservation<ToolbarActionsModel, ToolbarActionsModel::Observer>
      toolbar_model_observation_{this};
};

#endif  // CHROME_BROWSER_UI_EXTENSIONS_EXTENSIONS_MENU_VIEW_MODEL_H_
