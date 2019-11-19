// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_ITEM_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_ITEM_VIEW_H_

#include <memory>

#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

class Browser;
class ExtensionContextMenuController;
class ExtensionsMenuButton;
class ToolbarActionViewController;
class ToolbarActionsModel;

namespace views {
class ImageButton;
}  // namespace views

// ExtensionsMenuItemView is a single row inside the extensions menu for a
// particular extension. Includes information about the extension in addition to
// a button to pin the extension to the toolbar and a button for accessing the
// associated context menu.
class ExtensionsMenuItemView : public views::View,
                               public views::ButtonListener {
 public:
  static constexpr int kSecondaryIconSizeDp = 16;

  ExtensionsMenuItemView(
      Browser* browser,
      std::unique_ptr<ToolbarActionViewController> controller);
  ~ExtensionsMenuItemView() override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  void UpdatePinButton();

  bool IsContextMenuRunning();

  bool IsPinned();

  ExtensionsMenuButton* primary_action_button_for_testing();
  ToolbarActionViewController* view_controller_for_testing() {
    return controller_.get();
  }

 private:
  ExtensionsMenuButton* const primary_action_button_;

  std::unique_ptr<ToolbarActionViewController> controller_;

  views::ImageButton* context_menu_button_ = nullptr;

  ToolbarActionsModel* const model_;

  views::ImageButton* pin_button_ = nullptr;

  // This controller is responsible for showing the context menu for an
  // extension.
  std::unique_ptr<ExtensionContextMenuController> context_menu_controller_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionsMenuItemView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_ITEM_VIEW_H_
