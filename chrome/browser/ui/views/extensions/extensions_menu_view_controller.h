// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_VIEW_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_navigation_handler.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/view_observer.h"

namespace views {
class BubbleDialogDelegate;
class View;
}  // namespace views

class Browser;
class ExtensionsContainer;
class ExtensionsMenuMainPageView;
class ExtensionsMenuSitePermissionsPageView;
class ToolbarActionsModel;

class ExtensionsMenuViewController
    : public ExtensionsMenuNavigationHandler,
      public TabStripModelObserver,
      public ToolbarActionsModel::Observer,
      public extensions::PermissionsManager::Observer,
      public views::ViewObserver {
 public:
  ExtensionsMenuViewController(Browser* browser,
                               ExtensionsContainer* extensions_container,
                               views::View* bubble_contents,
                               views::BubbleDialogDelegate* dialog_delegate);
  ExtensionsMenuViewController(const ExtensionsMenuViewController&) = delete;
  const ExtensionsMenuViewController& operator=(
      const ExtensionsMenuViewController&) = delete;
  ~ExtensionsMenuViewController() override;

  // ExtensionsMenuNavigationHandler:
  void OpenMainPage() override;
  void OpenSitePermissionsPage(extensions::ExtensionId extension_id) override;
  void CloseBubble() override;

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

  // views::ViewObserver
  void OnViewIsDeleting(views::View* observed_view) override;

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

  // Populates menu items in `main_page`.
  void PopulateMainPage(ExtensionsMenuMainPageView* main_page);

  // Returns the currently active web contents.
  content::WebContents* GetActiveWebContents() const;

  const raw_ptr<Browser> browser_;
  const raw_ptr<ExtensionsContainer> extensions_container_;
  const raw_ptr<views::View> bubble_contents_;
  // TODO(crbug.com/1425522) There are no guarantee this pointer is safe
  // to be used. In practice its lifetime is probably always shorter than
  // `this`. This has to be fixed.
  const raw_ptr<views::BubbleDialogDelegate, DisableDanglingPtrDetection>
      bubble_delegate_;

  const raw_ptr<ToolbarActionsModel> toolbar_model_;
  base::ScopedObservation<ToolbarActionsModel, ToolbarActionsModel::Observer>
      toolbar_model_observation_{this};

  base::ScopedObservation<extensions::PermissionsManager,
                          extensions::PermissionsManager::Observer>
      permissions_manager_observation_{this};

  // The current page visible in `bubble_contents_`.
  raw_ptr<views::View> current_page_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_VIEW_CONTROLLER_H_
