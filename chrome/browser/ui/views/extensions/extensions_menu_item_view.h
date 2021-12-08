// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_ITEM_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_ITEM_VIEW_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

class Browser;
class ExtensionContextMenuController;
class ExtensionsMenuButton;
class HoverButton;
class Profile;
class ToolbarActionViewController;
class ToolbarActionsModel;

// ExtensionsMenuItemView is a single row inside the extensions menu for a
// particular extension. Includes information about the extension in addition to
// a button to pin the extension to the toolbar and a button for accessing the
// associated context menu.
class ExtensionsMenuItemView : public views::View {
 public:
  METADATA_HEADER(ExtensionsMenuItemView);

  enum class MenuItemType {
    // Used by the extensions tab in ExtensionsTabbedMenu to add a pin button
    // and a context menu button to the item view.
    kExtensions,
    // Used by the site access tab in ExtensionsTabbedMenu to add a dropdown
    // button to the item view.
    kSiteAccess
  };

  static constexpr int kMenuItemHeightDp = 40;
  static constexpr gfx::Size kIconSize{28, 28};

  ExtensionsMenuItemView(
      MenuItemType item_type,
      Browser* browser,
      std::unique_ptr<ToolbarActionViewController> controller,
      bool allow_pinning);
  ExtensionsMenuItemView(const ExtensionsMenuItemView&) = delete;
  ExtensionsMenuItemView& operator=(const ExtensionsMenuItemView&) = delete;
  ~ExtensionsMenuItemView() override;

  // views::View:
  void OnThemeChanged() override;

  // Updates the pin button. `item_type_` must be `ItemType::kExtensions`.
  void UpdatePinButton();

  // Returns whether the action corresponding to this view is pinned to the
  // toolbar. `item_type_` must be `ItemType::kExtensions`.
  bool IsPinned() const;

  // Displays the context menu. `item_type_` must be `ItemType::kExtensions`.
  void ContextMenuPressed();

  // Pins or unpins the action in the toolbar. `item_type_` must be
  // `ItemType::kExtensions`.
  void PinButtonPressed();

  ToolbarActionViewController* view_controller() { return controller_.get(); }
  const ToolbarActionViewController* view_controller() const {
    return controller_.get();
  }

  bool IsContextMenuRunningForTesting() const;
  ExtensionsMenuButton* primary_action_button_for_testing();
  HoverButton* context_menu_button_for_testing() {
    return context_menu_button_;
  }
  HoverButton* pin_button_for_testing() { return pin_button_; }

 private:
  // Adds a pin button as a child view. `item_type_` must be
  // `ItemType::kExtensions`.
  void AddPinButton();

  // Adds a context menu button as a child view. `item_type_` must be
  // `ItemType::kExtensions`.
  void AddContextMenuButton();

  // Maybe adjust |icon_color| to assure high enough contrast with the
  // background.
  SkColor GetAdjustedIconColor(SkColor icon_color) const;

  const MenuItemType item_type_;

  const raw_ptr<Profile> profile_;

  const raw_ptr<ExtensionsMenuButton> primary_action_button_;

  std::unique_ptr<ToolbarActionViewController> controller_;

  raw_ptr<HoverButton> context_menu_button_ = nullptr;

  const raw_ptr<ToolbarActionsModel> model_;

  raw_ptr<HoverButton> pin_button_ = nullptr;

  // This controller is responsible for showing the context menu for an
  // extension.
  std::unique_ptr<ExtensionContextMenuController> context_menu_controller_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_ITEM_VIEW_H_
