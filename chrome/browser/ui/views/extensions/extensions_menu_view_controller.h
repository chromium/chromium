// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_VIEW_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_navigation_handler.h"

namespace views {
class BubbleDialogDelegate;
class View;
}  // namespace views

class Browser;
class ExtensionsContainer;
class ExtensionsMenuPageView;
class ExtensionsMenuMainPageView;
class ToolbarActionsModel;

class ExtensionsMenuViewController : public ExtensionsMenuNavigationHandler,
                                     public TabStripModelObserver {
 public:
  ExtensionsMenuViewController(Browser* browser,
                               ExtensionsContainer* extensions_container,
                               views::View* bubble_contents,
                               views::BubbleDialogDelegate* dialog_delegate);
  ExtensionsMenuViewController(const ExtensionsMenuViewController&) = delete;
  const ExtensionsMenuViewController& operator=(
      const ExtensionsMenuViewController&) = delete;
  ~ExtensionsMenuViewController() override = default;

  // ExtensionsMenuNavigationHandler:
  void OpenMainPage() override;
  void OpenSitePermissionsPage() override;
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

  // Accessors used by tests:
  ExtensionsMenuMainPageView* GetMainPageViewForTesting();

 private:
  // Switches the current page to `page`.
  void SwitchToPage(std::unique_ptr<ExtensionsMenuPageView> page);

  const raw_ptr<Browser> browser_;
  const raw_ptr<ExtensionsContainer> extensions_container_;
  const raw_ptr<views::View> bubble_contents_;
  const raw_ptr<views::BubbleDialogDelegate> bubble_delegate_;

  const raw_ptr<ToolbarActionsModel> toolbar_model_;

  // The current page visible in `bubble_contents_`.
  raw_ptr<ExtensionsMenuPageView> current_page_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_VIEW_CONTROLLER_H_
