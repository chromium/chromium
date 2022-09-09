// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_ITEM_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_ITEM_VIEW_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/extensions/extension_site_access_combobox_model.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/flex_layout_view.h"

class Browser;
class ExtensionContextMenuController;
class ExtensionsMenuButton;
class HoverButton;
class ToolbarActionViewController;
class ToolbarActionsModel;
class ExtensionSiteAccessComboboxModel;

namespace views {
class Combobox;
}  // namespace views

// SiteAccessMenuItemView is a single row inside the extensions menu for an
// extension with host permissions. Includes information about the extension and
// a dropdown to select host permission options.
class SiteAccessMenuItemView : public views::FlexLayoutView {
 public:
  METADATA_HEADER(SiteAccessMenuItemView);

  SiteAccessMenuItemView(
      Browser* browser,
      std::unique_ptr<ToolbarActionViewController> controller);
  SiteAccessMenuItemView(const SiteAccessMenuItemView&) = delete;
  SiteAccessMenuItemView& operator=(const SiteAccessMenuItemView&) = delete;
  ~SiteAccessMenuItemView() override;

  // Updates the controller and child views to be on sync with the parent views.
  void Update();

  void SetSiteAccessComboboxVisible(bool visibility);

  ToolbarActionViewController* view_controller() { return controller_.get(); }

  ExtensionsMenuButton* primary_action_button_for_testing() {
    return primary_action_button_;
  }
  views::Combobox* site_access_combobox_for_testing() {
    return site_access_combobox_;
  }

 private:
  // Handles the selection of an option in a combobox. This is passed as a
  // callback to `site_access_combobox`.
  void OnComboboxSelectionChanged();

  const raw_ptr<Browser> browser_;

  // Controller responsible for an action that is shown in the toolbar.
  std::unique_ptr<ToolbarActionViewController> controller_;

  raw_ptr<ExtensionsMenuButton> primary_action_button_;

  raw_ptr<views::Combobox> site_access_combobox_ = nullptr;
  raw_ptr<ExtensionSiteAccessComboboxModel> site_access_combobox_model_ =
      nullptr;
};

BEGIN_VIEW_BUILDER(/* no export */,
                   SiteAccessMenuItemView,
                   views::FlexLayoutView)
END_VIEW_BUILDER

DEFINE_VIEW_BUILDER(/* no export */, SiteAccessMenuItemView)

// InstalledExtensionMenuItemView is a single row inside the extensions menu for
// a every installed extension. Includes information about the extension, a
// button to pin the extension to the toolbar and a button for accessing the
// associated context menu.
class InstalledExtensionMenuItemView : public views::FlexLayoutView {
 public:
  METADATA_HEADER(InstalledExtensionMenuItemView);

  InstalledExtensionMenuItemView(
      Browser* browser,
      std::unique_ptr<ToolbarActionViewController> controller,
      bool allow_pinning);
  InstalledExtensionMenuItemView(const InstalledExtensionMenuItemView&) =
      delete;
  InstalledExtensionMenuItemView& operator=(
      const InstalledExtensionMenuItemView&) = delete;
  ~InstalledExtensionMenuItemView() override;

  // views::View:
  void OnThemeChanged() override;

  // Updates the controller and child views to be on sync with the parent views.
  void Update();

  // Updates the pin button.
  void UpdatePinButton();

  ToolbarActionViewController* view_controller() { return controller_.get(); }
  const ToolbarActionViewController* view_controller() const {
    return controller_.get();
  }

  bool IsContextMenuRunningForTesting() const;
  ExtensionsMenuButton* primary_action_button_for_testing() {
    return primary_action_button_;
  }
  HoverButton* context_menu_button_for_testing() {
    return context_menu_button_;
  }
  HoverButton* pin_button_for_testing() { return pin_button_; }

 private:
  // Returns whether the action corresponding to this view is pinned to the
  // toolbar.
  bool IsPinned() const;

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

  raw_ptr<HoverButton> pin_button_ = nullptr;

  raw_ptr<HoverButton> context_menu_button_ = nullptr;
  // Controller responsible for showing the context menu for an extension.
  std::unique_ptr<ExtensionContextMenuController> context_menu_controller_;
};

BEGIN_VIEW_BUILDER(/* no export */,
                   InstalledExtensionMenuItemView,
                   views::FlexLayoutView)
END_VIEW_BUILDER

DEFINE_VIEW_BUILDER(/* no export */, InstalledExtensionMenuItemView)

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_ITEM_VIEW_H_
