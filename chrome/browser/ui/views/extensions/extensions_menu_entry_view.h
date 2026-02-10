// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_ENTRY_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_ENTRY_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/extensions/extensions_menu_view_model.h"
#include "chrome/browser/ui/views/extensions/extension_context_menu_controller.h"
#include "extensions/common/extension_id.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/layout/flex_layout_view.h"

class Browser;
class HoverButton;
class ToolbarActionViewModel;

namespace views {
class ToggleButton;
}  // namespace views

DECLARE_ELEMENT_IDENTIFIER_VALUE(kExtensionsMenuEntryViewElementId);

// The view for a single extension row in the new extensions menu.
class ExtensionsMenuEntryView
    : public views::FlexLayoutView,
      public ExtensionContextMenuController::Observer {
  METADATA_HEADER(ExtensionsMenuEntryView, views::FlexLayoutView)

 public:
  ExtensionsMenuEntryView(
      Browser* browser,
      bool is_enterprise,
      ToolbarActionViewModel* view_model,
      views::Button::PressedCallback action_button_callback,
      base::RepeatingCallback<void(bool)> site_access_toggle_callback,
      views::Button::PressedCallback site_permissions_button_callback);
  ExtensionsMenuEntryView(const ExtensionsMenuEntryView&) = delete;
  ExtensionsMenuEntryView& operator=(const ExtensionsMenuEntryView&) = delete;
  ~ExtensionsMenuEntryView() override;

  // Updates the view according to `entry_state`.
  void Update(ExtensionsMenuViewModel::MenuEntryState entry_state);

  // Updates the action button with the given `button_state`.
  void UpdateActionButton(
      const ExtensionsMenuViewModel::ControlState& button_state);

  // Updates the context menu button given `is_action_pinned`.
  void UpdateContextMenuButton(
      ExtensionsMenuViewModel::ControlState button_state);

  const extensions::ExtensionId& extension_id() { return extension_id_; }

  // Accessors for testing.
  bool IsContextMenuRunningForTesting() const;
  HoverButton* action_button_for_testing() { return action_button_; }
  views::ToggleButton* site_access_toggle_for_testing() {
    return site_access_toggle_;
  }
  HoverButton* site_permissions_button_for_testing() {
    return site_permissions_button_;
  }
  HoverButton* context_menu_button_for_testing() {
    return context_menu_button_;
  }

 private:
  // Sets ups the context menu button controllers. Must be called by the
  // constructor.
  void SetupContextMenuButton(ToolbarActionViewModel* view_model);

  // Handles the context menu button press. This is passed as a callback to
  // `context_menu_button_`.
  void OnContextMenuPressed();

  // ExtensionContextMenuController::Observer:
  void OnContextMenuShown() override;
  void OnContextMenuClosed() override;

  // The id of the extension the entry corresponds to.
  const extensions::ExtensionId extension_id_;

  // Controller responsible for showing the context menu for an extension.
  std::unique_ptr<ExtensionContextMenuController> context_menu_controller_;

  // Child Views
  raw_ptr<HoverButton> action_button_ = nullptr;
  raw_ptr<views::ToggleButton> site_access_toggle_ = nullptr;
  raw_ptr<HoverButton> site_permissions_button_ = nullptr;
  raw_ptr<HoverButton> context_menu_button_ = nullptr;
  raw_ptr<HoverButton> pin_button_ = nullptr;
};

BEGIN_VIEW_BUILDER(/* no export */,
                   ExtensionsMenuEntryView,
                   views::FlexLayoutView)
END_VIEW_BUILDER

DEFINE_VIEW_BUILDER(/* no export */, ExtensionsMenuEntryView)

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_ENTRY_VIEW_H_
