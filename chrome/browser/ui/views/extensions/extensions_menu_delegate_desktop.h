// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_DELEGATE_DESKTOP_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_DELEGATE_DESKTOP_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/ui/extensions/extensions_menu_view_model.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_handler.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/permissions_manager.h"
#include "extensions/common/extension.h"
#include "ui/views/view_tracker.h"

namespace views {
class View;
}  // namespace views

class ExtensionsMenuViewModel;
class Browser;
class ExtensionsContainerViews;
class ExtensionsMenuMainPageView;
class ExtensionsMenuSitePermissionsPageView;
class ToolbarActionsModel;

// TODO(crbug.com/449814184): Separate extensions UI business logic (e.g what
// text should appear on a button) versus UI platform logic (e.g updating the
// view).
class ExtensionsMenuDelegateDesktop : public ExtensionsMenuViewModel::Delegate,
                                      public ExtensionsMenuViewModel::Observer,
                                      public ExtensionsMenuHandler {
 public:
  ExtensionsMenuDelegateDesktop(
      Browser* browser,
      ExtensionsContainer* extensions_container,
      ExtensionsContainerViews* extensions_container_views,
      views::View* bubble_contents);
  ExtensionsMenuDelegateDesktop(const ExtensionsMenuDelegateDesktop&) = delete;
  const ExtensionsMenuDelegateDesktop& operator=(
      const ExtensionsMenuDelegateDesktop&) = delete;
  ~ExtensionsMenuDelegateDesktop() override;

  // ExtensionsMenuViewModel::Delegate:
  std::unique_ptr<ExtensionActionViewModel> CreateActionViewModel(
      const extensions::ExtensionId& extension_id) override;

  // ExtensionsMenuViewModel::Observer:
  void OnPageNavigation() override;
  void OnHostAccessRequestAdded(const extensions::ExtensionId& extension_id,
                                int index) override;
  void OnHostAccessRequestUpdated(const extensions::ExtensionId& extension_id,
                                  int index) override;
  void OnHostAccessRequestRemoved(const extensions::ExtensionId& extension_id,
                                  int index) override;
  void OnHostAccessRequestsCleared() override;
  void OnShowHostAccessRequestsInToolbarChanged(
      const extensions::ExtensionId& extension_id,
      bool can_show_requests) override;
  void OnActionAdded(ExtensionActionViewModel* action_model,
                     int index) override;
  void OnActionRemoved(const ToolbarActionsModel::ActionId& action_id,
                       int index) override;
  void OnActionUpdated(const ToolbarActionsModel::ActionId& action_id) override;
  void OnActionIconUpdated(
      const ToolbarActionsModel::ActionId& action_id) override;
  void OnActionsInitialized() override;
  void OnToolbarPinnedActionsChanged() override;
  void OnUserPermissionsSettingsChanged() override;

  // ExtensionsMenuHandler:
  void OpenMainPage() override;
  void OpenSitePermissionsPage(
      const extensions::ExtensionId& extension_id) override;
  void CloseBubble() override;
  void OnActionButtonClicked(
      const extensions::ExtensionId& extension_id) override;
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

  // Accessors used by tests:
  // Returns the main page iff it's the `current_page_` one.
  ExtensionsMenuMainPageView* GetMainPageViewForTesting();
  // Returns the site permissions page iff it's the `current_page_` one.
  ExtensionsMenuSitePermissionsPageView* GetSitePermissionsPageForTesting();

 private:
  // Switches the current page to `page`.
  void SwitchToPage(std::unique_ptr<views::View> page);

  // Updates the menu's main page.
  void UpdateMainPage(ExtensionsMenuMainPageView* main_page);

  // Updates the menu's site permissions page.
  void UpdateSitePermissionsPage(
      ExtensionsMenuSitePermissionsPageView* site_permissions_page);

  // Populates menu entries in `main_page`.
  void PopulateMainPage(ExtensionsMenuMainPageView* main_page);

  // Inserts an entry for `extension_id` in `main_page` at `index`.
  void InsertMenuEntry(ExtensionsMenuMainPageView* main_page,
                       ExtensionActionViewModel* action_model,
                       int index);

  const raw_ptr<Browser> browser_;
  const raw_ref<ExtensionsContainer> extensions_container_;
  const raw_ptr<ExtensionsContainerViews> extensions_container_views_;
  const raw_ptr<views::View> bubble_contents_;

  // The platform-agnostic menu view model.
  std::unique_ptr<ExtensionsMenuViewModel> menu_model_;
  base::ScopedObservation<ExtensionsMenuViewModel,
                          ExtensionsMenuViewModel::Observer>
      menu_model_observation_{this};

  const raw_ptr<ToolbarActionsModel> toolbar_model_;

  // The current page visible in `bubble_contents_`.
  views::ViewTracker current_page_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_DELEGATE_DESKTOP_H_
