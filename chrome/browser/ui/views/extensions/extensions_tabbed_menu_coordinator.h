// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_TABBED_MENU_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_TABBED_MENU_COORDINATOR_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_button.h"
#include "ui/views/view_tracker.h"

namespace views {
class View;
}  // namespace views

class Browser;
class ExtensionsContainer;
class ExtensionsTabbedMenuView;

class ExtensionsTabbedMenuCoordinator {
 public:
  ExtensionsTabbedMenuCoordinator(Browser* browser,
                                  ExtensionsContainer* extensions_container_,
                                  bool allow_pining);
  ExtensionsTabbedMenuCoordinator(const ExtensionsTabbedMenuCoordinator&) =
      delete;
  const ExtensionsTabbedMenuCoordinator& operator=(
      const ExtensionsTabbedMenuCoordinator&) = delete;
  ~ExtensionsTabbedMenuCoordinator();

  // Displays the ExtensionsTabbedMenu under `anchor_view` with the selected tab
  // open by the `selected_tab_index` given.
  void Show(views::View* anchor_view,
            ExtensionsToolbarButton::ButtonType button_type);

  // Hides the currently-showing ExtensionsTabbedMenuView, if it exists.
  void Hide();

  // Returns true if the ExtensionsTabbedMenuView is showing.
  bool IsShowing() const;

  // Returns the currently-showing ExtensionsTabbedMenuView, if it exists.
  ExtensionsTabbedMenuView* GetExtensionsTabbedMenuView();

 private:
  raw_ptr<Browser> browser_;
  raw_ptr<ExtensionsContainer> extensions_container_;
  bool allow_pining_;
  views::ViewTracker extensions_tabbed_menu_view_tracker_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_TABBED_MENU_COORDINATOR_H_
