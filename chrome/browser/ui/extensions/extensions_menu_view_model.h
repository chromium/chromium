// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_EXTENSIONS_MENU_VIEW_MODEL_H_
#define CHROME_BROWSER_UI_EXTENSIONS_EXTENSIONS_MENU_VIEW_MODEL_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/extensions/extensions_menu_view_platform_delegate.h"
#include "chrome/browser/ui/tabs/tab_list_interface.h"
#include "chrome/browser/ui/tabs/tab_list_interface_observer.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_model.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "extensions/browser/permissions_manager.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"

namespace content {
class WebContents;
}  // namespace content

class BrowserWindowInterface;
class ExtensionsMenuViewPlatformDelegate;
class ToolbarActionViewModel;

// The platform agnostic controller for the extensions menu.
// TODO(crbug.com/449814184): Move the observers from
// ExtensionsMenuViewController here.
class ExtensionsMenuViewModel : public extensions::PermissionsManager::Observer,
                                public ToolbarActionsModel::Observer,
                                public TabListInterfaceObserver,
                                public content::WebContentsObserver {
 public:
  // Holds the information about how the extension's menu item should look like.
  // This will be used by the platform delegate as needed.
  struct MenuItemInfo {
    enum class SiteAccessToggleState {
      // Button is not visible.
      kHidden,
      // Button is visible and off.
      kOff,
      // Button is visible and on.
      kOn,
    };

    enum class SitePermissionsButtonAccess {
      // Extension has no site access.
      kNone,
      // Extension has site access when clicked.
      kOnClick,
      // Extension has site access to this site.
      kOnSite,
      // Extension has site access to all sites.
      kOnAllSites
    };

    enum class SitePermissionsButtonState {
      // Button is not visible.
      kHidden,
      // Button is visible, but disabled.
      kDisabled,
      // Button is visible and enabled.
      kEnabled,
    };

    SiteAccessToggleState site_access_toggle_state;
    SitePermissionsButtonAccess site_permissions_button_access;
    SitePermissionsButtonState site_permissions_button_state;
    bool is_enterprise;
  };

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

  // Allows the extension's host access request to the current site.
  void AllowHostAccessRequest(const extensions::ExtensionId& extension_id);

  // Dismisses the extension's host access request to the current site.
  void DismissHostAccessRequest(const extensions::ExtensionId& extension_id);

  // Sets whether the extension can show host access requests in the toolbar.
  void ShowHostAccessRequestsInToolbar(
      const extensions::ExtensionId& extension_id,
      bool show);

  // Grants the extension site access to the current site.
  void GrantSiteAccess(const extensions::ExtensionId& extension_id);

  // Revokes the extension's site access from the current site.
  void RevokeSiteAccess(const extensions::ExtensionId& extension_id);

  // Update the extension's site setting for the current site.
  void UpdateSiteSetting(
      extensions::PermissionsManager::UserSiteSetting site_setting);

  // Reloads the current web contents.
  void ReloadWebContents();

  // Returns the menu item info for extension with `model`.
  MenuItemInfo GetMenuItemInfo(ToolbarActionViewModel* model);

  // PermissionsManager::Observer:
  void OnHostAccessRequestAdded(const extensions::ExtensionId& extension_id,
                                int tab_id) override;
  void OnHostAccessRequestUpdated(const extensions::ExtensionId& extension_id,
                                  int tab_id) override;
  void OnHostAccessRequestRemoved(const extensions::ExtensionId& extension_id,
                                  int tab_id) override;
  void OnHostAccessRequestsCleared(int tab_id) override;
  void OnHostAccessRequestDismissedByUser(
      const extensions::ExtensionId& extension_id,
      const url::Origin& origin) override;
  void OnShowAccessRequestsInToolbarChanged(
      const extensions::ExtensionId& extension_id,
      bool can_show_requests) override;
  void OnUserPermissionsSettingsChanged(
      const extensions::PermissionsManager::UserPermissionsSettings& settings)
      override;

  // ToolbarActionsModel::Observer:
  void OnToolbarActionAdded(
      const ToolbarActionsModel::ActionId& action_id) override;
  void OnToolbarActionRemoved(
      const ToolbarActionsModel::ActionId& action_id) override;
  void OnToolbarActionUpdated(
      const ToolbarActionsModel::ActionId& action_id) override;
  void OnToolbarModelInitialized() override;
  void OnToolbarPinnedActionsChanged() override;

  // TabListInterfaceObserver:
  // Sometimes, menu can stay open when tab changes (e.g keyboard shortcuts) or
  // due to the extension (e.g extension switching the active tab). Thus, we
  // listen for active tab changes to properly update the menu content.
  void OnActiveTabChanged(tabs::TabInterface* tab) override;

  // content::WebContentsObserver:
  void DidFinishNavigation(content::NavigationHandle* handle) override;

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

  base::ScopedObservation<TabListInterface, TabListInterfaceObserver>
      tab_list_interface_observation_{this};
};

#endif  // CHROME_BROWSER_UI_EXTENSIONS_EXTENSIONS_MENU_VIEW_MODEL_H_
