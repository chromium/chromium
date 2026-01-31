// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_ITEM_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_ITEM_VIEW_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/extensions/extensions_menu_view_model.h"
#include "chrome/browser/ui/views/extensions/extension_context_menu_controller.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/metadata/view_factory.h"

class Browser;
class ExtensionContextMenuController;
class ExtensionsMenuButton;
class HoverButton;
class ToolbarActionViewModel;
class ToolbarActionsModel;

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
  ExtensionMenuItemView(const ExtensionMenuItemView&) = delete;
  ExtensionMenuItemView& operator=(const ExtensionMenuItemView&) = delete;
  ~ExtensionMenuItemView() override;

  // Updates the pin button.
  void UpdatePinButton(bool is_force_pinned, bool is_pinned);

  ToolbarActionViewModel* view_model() { return view_model_.get(); }
  const ToolbarActionViewModel* view_model() const { return view_model_.get(); }

  bool IsContextMenuRunningForTesting() const;
  ExtensionsMenuButton* primary_action_button_for_testing();
  HoverButton* context_menu_button_for_testing();
  HoverButton* pin_button_for_testing();

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
  const std::unique_ptr<ToolbarActionViewModel> view_model_;

  // Model for the browser actions toolbar that provides information such as the
  // action pin status or visibility.
  const raw_ptr<ToolbarActionsModel> model_;

  raw_ptr<ExtensionsMenuButton> primary_action_button_;

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
