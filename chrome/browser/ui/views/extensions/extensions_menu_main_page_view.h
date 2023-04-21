// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_MAIN_PAGE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_MAIN_PAGE_VIEW_H_

#include "base/memory/raw_ptr_exclusion.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_item_view.h"
#include "ui/views/view.h"

#include "ui/base/metadata/metadata_header_macros.h"

namespace content {
class WebContents;
}

namespace views {
class Label;
class ToggleButton;
}  // namespace views

class Browser;
class ExtensionsMenuNavigationHandler;
class ToolbarActionsModel;
class ExtensionMenuItemView;
class ExtensionActionViewController;

// The main view of the extensions menu.
class ExtensionsMenuMainPageView : public views::View {
 public:
  METADATA_HEADER(ExtensionsMenuMainPageView);

  explicit ExtensionsMenuMainPageView(
      Browser* browser,
      ExtensionsMenuNavigationHandler* navigation_handler);
  ~ExtensionsMenuMainPageView() override = default;
  ExtensionsMenuMainPageView(const ExtensionsMenuMainPageView&) = delete;
  const ExtensionsMenuMainPageView& operator=(
      const ExtensionsMenuMainPageView&) = delete;

  // Creates and adds a menu item for `action_controller` at `index` for a
  // newly-added extension.
  void CreateAndInsertMenuItem(
      std::unique_ptr<ExtensionActionViewController> action_controller,
      extensions::ExtensionId extension_id,
      ExtensionMenuItemView::SiteAccessToggleState site_access_toggle_state,
      ExtensionMenuItemView::SitePermissionsButtonState
          site_permissions_button_state,
      int index);

  // Removes the menu item corresponding to `action_id`.
  void RemoveMenuItem(const ToolbarActionsModel::ActionId& action_id);

  // Returns the menu items.
  std::vector<ExtensionMenuItemView*> GetMenuItems() const;

  // Updates the page with the given parameters. Does not update the menu items
  // (menu item updates are handled directly in such view).
  void Update(std::u16string current_site,
              bool is_site_settings_toggle_visible,
              bool is_site_settings_toggle_on);

  void OnToggleButtonPressed();

  // Accessors used by tests:
  // Returns the currently-showing menu items.
  views::ToggleButton* GetSiteSettingsToggleForTesting() {
    return site_settings_toggle_;
  }

 private:
  content::WebContents* GetActiveWebContents() const;

  const raw_ptr<Browser> browser_;
  const raw_ptr<ExtensionsMenuNavigationHandler> navigation_handler_;

  // Subheader section.
  raw_ptr<views::Label> subheader_subtitle_;
  raw_ptr<views::ToggleButton> site_settings_toggle_;

  // Menu items section.
  // The view containing the menu items. This is separated for easy insertion
  // and iteration of menu items. The children are guaranteed to only be
  // ExtensionMenuItemViews.
  // This field is not a raw_ptr<> because it was filtered by the rewriter for:
  // #addr-of
  RAW_PTR_EXCLUSION views::View* menu_items_ = nullptr;
};

BEGIN_VIEW_BUILDER(/* no export */, ExtensionsMenuMainPageView, views::View)
END_VIEW_BUILDER

DEFINE_VIEW_BUILDER(/* no export */, ExtensionsMenuMainPageView)

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_MAIN_PAGE_VIEW_H_
