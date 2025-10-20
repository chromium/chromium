// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_VIEW_PLATFORM_DELEGATE_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_VIEW_PLATFORM_DELEGATE_VIEWS_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/extensions/extensions_menu_view_model.h"
#include "chrome/browser/ui/extensions/extensions_menu_view_platform_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_handler.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/permissions_manager.h"
#include "extensions/common/extension.h"
#include "ui/views/view_tracker.h"

namespace content {
class WebContents;
}  // namespace content

namespace views {
class View;
}  // namespace views

class ExtensionsMenuViewModel;
class Browser;
class ExtensionsContainer;
class ExtensionsMenuMainPageView;
class ExtensionsMenuSitePermissionsPageView;
class ToolbarActionsModel;

// TODO(crbug.com/449814184): Separate extensions UI business logic (e.g what
// text should appear on a button) versus UI platform logic (e.g updating the
// view).
class ExtensionsMenuViewPlatformDelegateViews
    : public ExtensionsMenuViewPlatformDelegate,
      public ExtensionsMenuHandler,
      public TabStripModelObserver,
      public ToolbarActionsModel::Observer,
      public extensions::PermissionsManager::Observer {
 public:
  ExtensionsMenuViewPlatformDelegateViews(
      Browser* browser,
      ExtensionsContainer* extensions_container,
      views::View* bubble_contents);
  ExtensionsMenuViewPlatformDelegateViews(
      const ExtensionsMenuViewPlatformDelegateViews&) = delete;
  const ExtensionsMenuViewPlatformDelegateViews& operator=(
      const ExtensionsMenuViewPlatformDelegateViews&) = delete;
  ~ExtensionsMenuViewPlatformDelegateViews() override;

  // ExtensionsMenuViewPlatformDelegate:
  void AttachToModel(ExtensionsMenuViewModel* model) override;
  void DetachFromModel() override;
  void OnAccessRequestAdded(const extensions::ExtensionId& extension_id,
                            content::WebContents* web_contents) override;

  // ExtensionsMenuHandler:
  void OpenMainPage() override;
  void OpenSitePermissionsPage(
      const extensions::ExtensionId& extension_id) override;
  void CloseBubble() override;
  void OnSiteSettingsToggleButtonPressed(bool is_on) override;
  void OnSiteAccessSelected(
      const extensions::ExtensionId& extension_id,
      extensions::PermissionsManager::UserSiteAccess site_access) override;
  void OnExtensionToggleSelected(const extensions::ExtensionId& extension_id,
                                 bool is_on) override;
  void OnReloadPageButtonClicked() override;
  void OnAllowExtensionClicked(
      const extensions::ExtensionId& extension_id) override;
  void OnDismissExtensionClicked(
      const extensions::ExtensionId& extension_id) override;
  void OnShowRequestsTogglePressed(const extensions::ExtensionId& extension_id,
                                   bool is_on) override;

  // TabStripModelObserver:
  // Sometimes, menu can stay open when tab changes (e.g keyboard shortcuts) or
  // due to the extension (e.g extension switching the active tab). Thus, we
  // listen for tab changes to properly update the menu content.
  void TabChangedAt(content::WebContents* contents,
                    int index,
                    TabChangeType change_type) override;
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  // ToolbarActionsModel::Observer:
  void OnToolbarActionAdded(
      const ToolbarActionsModel::ActionId& action_id) override;
  void OnToolbarActionRemoved(
      const ToolbarActionsModel::ActionId& action_id) override;
  void OnToolbarActionUpdated(
      const ToolbarActionsModel::ActionId& action_id) override;
  void OnToolbarModelInitialized() override;
  void OnToolbarPinnedActionsChanged() override;

  // PermissionsManager::Observer:
  void OnUserPermissionsSettingsChanged(
      const extensions::PermissionsManager::UserPermissionsSettings& settings)
      override;
  void OnShowAccessRequestsInToolbarChanged(
      const extensions::ExtensionId& extension_id,
      bool can_show_requests) override;
  void OnHostAccessRequestUpdated(const extensions::ExtensionId& extension_id,
                                  int tab_id) override;
  void OnHostAccessRequestRemoved(const extensions::ExtensionId& extension_id,
                                  int tab_id) override;
  void OnHostAccessRequestsCleared(int tab_id) override;
  void OnHostAccessRequestDismissedByUser(
      const extensions::ExtensionId& extension_id,
      const url::Origin& origin) override;

  // Accessors used by tests:
  // Returns the main page iff it's the `current_page_` one.
  ExtensionsMenuMainPageView* GetMainPageViewForTesting();
  // Returns the site permissions page iff it's the `current_page_` one.
  ExtensionsMenuSitePermissionsPageView* GetSitePermissionsPageForTesting();

 private:
  // Switches the current page to `page`.
  void SwitchToPage(std::unique_ptr<views::View> page);

  // Updates current_page for the given `web_contents`.
  void UpdatePage(content::WebContents* web_contents);

  // Updates `main_page` for the given `web_contents`.
  void UpdateMainPage(ExtensionsMenuMainPageView* main_page,
                      content::WebContents* web_contents);

  // Updates `site_permissions_page` for the given `web_contents`.
  void UpdateSitePermissionsPage(
      ExtensionsMenuSitePermissionsPageView* site_permissions_page,
      content::WebContents* web_contents);

  // Populates menu items in `main_page`.
  void PopulateMainPage(ExtensionsMenuMainPageView* main_page);

  // Inserts a menu item for `extension_id` in `main_page` at `index`.
  void InsertMenuItemMainPage(ExtensionsMenuMainPageView* main_page,
                              const extensions::ExtensionId& extension_id,
                              int index);

  // Adds or updates a request access entry for `extension_id` in `main_page` at
  // `index`.
  // TODO(crbug.com/449814184): Remove in favor of
  // ExtensionsMenuPlatformDelegate methods.
  void AddOrUpdateExtensionRequestingAccess(
      ExtensionsMenuMainPageView* main_page,
      const extensions::ExtensionId& extension_id,
      int index,
      content::WebContents* web_contents);

  // Returns the currently active web contents.
  content::WebContents* GetActiveWebContents() const;

  const raw_ptr<Browser> browser_;
  const raw_ptr<ExtensionsContainer> extensions_container_;
  const raw_ptr<views::View> bubble_contents_;

  // The platform-agnostic menu view model.
  raw_ptr<ExtensionsMenuViewModel> menu_model_{nullptr};

  const raw_ptr<ToolbarActionsModel> toolbar_model_;
  base::ScopedObservation<ToolbarActionsModel, ToolbarActionsModel::Observer>
      toolbar_model_observation_{this};

  base::ScopedObservation<extensions::PermissionsManager,
                          extensions::PermissionsManager::Observer>
      permissions_manager_observation_{this};

  // The current page visible in `bubble_contents_`.
  views::ViewTracker current_page_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_VIEW_PLATFORM_DELEGATE_VIEWS_H_
