// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_MAIN_PAGE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_MAIN_PAGE_VIEW_H_

#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "ui/views/view.h"

namespace views {
class Label;
class ToggleButton;
}

class Browser;
class ExtensionsMenuNavigationHandler;
class ToolbarActionsModel;

// The main view of the extensions menu.
class ExtensionsMenuMainPageView : public views::View,
                                   public TabStripModelObserver {
 public:
  explicit ExtensionsMenuMainPageView(
      Browser* browser,
      ExtensionsMenuNavigationHandler* navigation_handler);
  ~ExtensionsMenuMainPageView() override = default;
  ExtensionsMenuMainPageView(const ExtensionsMenuMainPageView&) = delete;
  const ExtensionsMenuMainPageView& operator=(
      const ExtensionsMenuMainPageView&) = delete;

  void Update();

  void OnToggleButtonPressed();

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

 private:
  content::WebContents* GetActiveWebContents() const;

  const raw_ptr<Browser> browser_;
  const raw_ptr<ExtensionsMenuNavigationHandler> navigation_handler_;
  const raw_ptr<ToolbarActionsModel> toolbar_model_;

  // Subheader.
  raw_ptr<views::Label> subheader_subtitle_;
  raw_ptr<views::ToggleButton> site_settings_toggle_;
};

BEGIN_VIEW_BUILDER(/* no export */, ExtensionsMenuMainPageView, views::View)
END_VIEW_BUILDER

DEFINE_VIEW_BUILDER(/* no export */, ExtensionsMenuMainPageView)

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_MAIN_PAGE_VIEW_H_
