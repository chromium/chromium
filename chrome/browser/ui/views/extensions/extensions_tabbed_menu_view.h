// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_TABBED_MENU_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_TABBED_MENU_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/extensions/extension_action_view_controller.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_button.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace views {
class TabbedPane;
}  // namespace views

class ExtensionsMenuItemView;
class ExtensionsContainer;

// ExtensionsTabbedMenuView is the extensions menu dialog with a tabbed pane.
// TODO(crbug.com/1263311): Brief explanation of each tabs goal after
// implementing them.
class ExtensionsTabbedMenuView : public views::BubbleDialogDelegateView {
 public:
  METADATA_HEADER(ExtensionsTabbedMenuView);
  ExtensionsTabbedMenuView(views::View* anchor_view,
                           Browser* browser,
                           ExtensionsContainer* extensions_container,
                           ExtensionsToolbarButton::ButtonType button_type,
                           bool allow_pinning);
  ~ExtensionsTabbedMenuView() override;
  ExtensionsTabbedMenuView(const ExtensionsTabbedMenuView&) = delete;
  const ExtensionsTabbedMenuView& operator=(const ExtensionsTabbedMenuView&) =
      delete;

  // Displays the ExtensionsTabbedMenu under `anchor_view` with the selected tab
  // open by the `selected_tab_index` given. Only one menu is allowed to be
  // shown at a time (outside of tests).
  static views::Widget* ShowBubble(
      views::View* anchor_view,
      Browser* browser,
      ExtensionsContainer* extensions_container_,
      ExtensionsToolbarButton::ButtonType button_type,
      bool allow_pinning);

  // Returns true if there is currently an ExtensionsTabbedMenuView showing
  // (across all browsers and profiles).
  static bool IsShowing();

  // Hides the currently-showing ExtensionsTabbedMenuView, if any exists.
  static void Hide();

  // Returns the currently-showing ExtensionsTabbedMenuView, if any exists.
  static ExtensionsTabbedMenuView* GetExtensionsTabbedMenuViewForTesting();

  // Returns the currently-showing extension items in the installed tab, if any
  // exists.
  std::vector<ExtensionsMenuItemView*> GetInstalledItemsForTesting() const;

  // Returns the index of the currently selected tab.
  size_t GetSelectedTabIndex() const;

  // views::BubbleDialogDelegateView:
  std::u16string GetAccessibleWindowTitle() const override;

 private:
  // Initially creates the tabs and opens the corresponding one based on the
  // `button_type`.
  void Populate(ExtensionsToolbarButton::ButtonType button_type_);

  // Adds a menu item in the installed extensions for a newly-added extension.
  void CreateAndInsertInstalledExtension(
      const ToolbarActionsModel::ActionId& id,
      int index);

  Browser* const browser_;
  ExtensionsContainer* const extensions_container_;
  ToolbarActionsModel* const toolbar_model_;
  bool const allow_pinning_;

  // The view containing the menu's two tabs.
  raw_ptr<views::TabbedPane> tabbed_pane_ = nullptr;

  // The view containing the installed extension menu items. This is
  // separated for easy insertion and iteration of menu items. The children are
  // guaranteed to only be ExtensionMenuItemViews.
  views::View* installed_items_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_TABBED_MENU_VIEW_H_
