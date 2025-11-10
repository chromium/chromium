// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_ITEM_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_ITEM_VIEW_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/extensions/extensions_menu_view_model.h"
#include "chrome/browser/ui/views/extensions/extension_context_menu_controller.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/layout/flex_layout_view.h"

class Browser;
class ExtensionContextMenuController;
class ExtensionsMenuButton;
class HoverButton;
class ToolbarActionViewModel;
class ToolbarActionsModel;

namespace views {
class ToggleButton;
}  // namespace views

DECLARE_ELEMENT_IDENTIFIER_VALUE(kExtensionMenuItemViewElementId);

// Single row inside the extensions menu for every installed extension. Includes
// information about the extension, a button to pin the extension to the toolbar
// and a button for accessing the associated context menu.
class ExtensionMenuItemView : public views::FlexLayoutView,
                              public ExtensionContextMenuController::Observer {
  METADATA_HEADER(ExtensionMenuItemView, views::FlexLayoutView)

 public:
  ExtensionMenuItemView(Browser* browser,
                        std::unique_ptr<ToolbarActionViewModel> view_model,
                        bool allow_pinning);

  // Constructor for the kExtensionsMenuAccessControl feature.
  ExtensionMenuItemView(
      Browser* browser,
      bool is_enterprise,
      std::unique_ptr<ToolbarActionViewModel> view_model,
      base::RepeatingCallback<void(bool)> site_access_toggle_callback,
      views::Button::PressedCallback site_permissions_button_callback);
  ExtensionMenuItemView(const ExtensionMenuItemView&) = delete;
  ExtensionMenuItemView& operator=(const ExtensionMenuItemView&) = delete;
  ~ExtensionMenuItemView() override;

  // Updates the controller and child views to be on sync with the parent views.
  void Update(ExtensionsMenuViewModel::MenuItemInfo menu_item);

  // Updates the pin button.
  void UpdatePinButton(bool is_force_pinned, bool is_pinned);

  // Updates the context menu button given `is_action_pinned`.
  void UpdateContextMenuButton(bool is_action_pinned);

  ToolbarActionViewModel* view_model() { return view_model_.get(); }
  const ToolbarActionViewModel* view_model() const { return view_model_.get(); }

  bool IsContextMenuRunningForTesting() const;
  ExtensionsMenuButton* primary_action_button_for_testing();
  views::ToggleButton* site_access_toggle_for_testing();
  HoverButton* context_menu_button_for_testing();
  HoverButton* pin_button_for_testing();
  HoverButton* site_permissions_button_for_testing();

 private:
  // ExtensionContextMenuController::Observer:
  void OnContextMenuShown() override;
  void OnContextMenuClosed() override;

  // Sets ups the context menu button controllers. Must be called by the
  // constructor.
  void SetupContextMenuButton();

  // Handles the context menu button press. This is passed as a callback to
  // `context_menu_button_`.
  void OnContextMenuPressed();

  // Handles the pin button press. This is passed as a callback to
  // `pin_button_`.
  void OnPinButtonPressed();

  const raw_ptr<Browser> browser_;

  // View Model for an action that is shown in the toolbar.
  std::unique_ptr<ToolbarActionViewModel> view_model_;

  // Model for the browser actions toolbar that provides information such as the
  // action pin status or visibility.
  const raw_ptr<ToolbarActionsModel> model_;

  raw_ptr<ExtensionsMenuButton> primary_action_button_;

  raw_ptr<views::ToggleButton> site_access_toggle_ = nullptr;

  // Button that displays the extension site access and opens its site
  // permissions page.
  raw_ptr<HoverButton> site_permissions_button_ = nullptr;

  raw_ptr<HoverButton> pin_button_ = nullptr;

  raw_ptr<HoverButton> context_menu_button_ = nullptr;
  // Controller responsible for showing the context menu for an extension.
  std::unique_ptr<ExtensionContextMenuController> context_menu_controller_;
};

BEGIN_VIEW_BUILDER(/* no export */,
                   ExtensionMenuItemView,
                   views::FlexLayoutView)
END_VIEW_BUILDER

DEFINE_VIEW_BUILDER(/* no export */, ExtensionMenuItemView)

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_ITEM_VIEW_H_
