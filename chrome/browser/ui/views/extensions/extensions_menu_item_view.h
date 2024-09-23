// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_ITEM_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_ITEM_VIEW_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/layout/flex_layout_view.h"

class Browser;
class ExtensionContextMenuController;
class ExtensionsMenuButton;
class HoverButton;
class ToolbarActionViewController;
class ToolbarActionsModel;

namespace views {
class ToggleButton;
}  // namespace views

// Single row inside the extensions menu for every installed extension. Includes
// information about the extension, a button to pin the extension to the toolbar
// and a button for accessing the associated context menu.
class ExtensionMenuItemView : public views::FlexLayoutView {
  METADATA_HEADER(ExtensionMenuItemView, views::FlexLayoutView)

 public:
  enum class SiteAccessToggleState {
    // Button is not visible.
    kHidden,
    // Button is visible and off.
    kOff,
    // Button is visible and on.
    kOn,
  };

  enum class SitePermissionsButtonState {
    // Button is not visible.
    kHidden,
    // Button is visible, but disabled.
    kDisabled,
    // Button is visible and enabled.
    kEnabled,
  };

  // Extension site access displayed in the site permissions button.
  enum class SitePermissionsButtonAccess {
    // Extension has no site access.
    kNone,
    // Extension has site access when clicked.
    kOnClick,
    // Extension has site access to this site.
    kOnSite,
    // Extension has site access to all sites.
    kOnAllSites
  };

  ExtensionMenuItemView(Browser* browser,
                        std::unique_ptr<ToolbarActionViewController> controller,
                        bool allow_pinning);

  // Constructor for the kExtensionsMenuAccessControl feature.
  ExtensionMenuItemView(
      Browser* browser,
      bool is_enterprise,
      std::unique_ptr<ToolbarActionViewController> controller,
      base::RepeatingCallback<void(bool)> site_access_toggle_callback,
      views::Button::PressedCallback site_permissions_button_callback);
  ExtensionMenuItemView(const ExtensionMenuItemView&) = delete;
  ExtensionMenuItemView& operator=(const ExtensionMenuItemView&) = delete;
  ~ExtensionMenuItemView() override;

  // Updates the controller and child views to be on sync with the parent views.
  void Update(SiteAccessToggleState site_access_toggle_state,
              SitePermissionsButtonState site_permissions_button_state,
              SitePermissionsButtonAccess site_permissions_button_access,
              bool is_enterprise);

  // Updates the pin button.
  void UpdatePinButton(bool is_force_pinned, bool is_pinned);

  // Updates the context menu button given `is_action_pinned`.
  void UpdateContextMenuButton(bool is_action_pinned);

  ToolbarActionViewController* view_controller() { return controller_.get(); }
  const ToolbarActionViewController* view_controller() const {
    return controller_.get();
  }

  bool IsContextMenuRunningForTesting() const;
  ExtensionsMenuButton* primary_action_button_for_testing() {
    return primary_action_button_;
  }
  views::ToggleButton* site_access_toggle_for_testing() {
    return site_access_toggle_;
  }
  HoverButton* context_menu_button_for_testing() {
    return context_menu_button_;
  }
  HoverButton* pin_button_for_testing() { return pin_button_; }
  HoverButton* site_permissions_button_for_testing() {
    return site_permissions_button_;
  }

 private:
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

  // Controller responsible for an action that is shown in the toolbar.
  std::unique_ptr<ToolbarActionViewController> controller_;

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
