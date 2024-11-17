// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_MAIN_PAGE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_MAIN_PAGE_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_item_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace content {
class WebContents;
}

namespace ui {
class ImageModel;
}  // namespace ui

namespace views {
class Label;
class ToggleButton;
}  // namespace views

class Browser;
class ExtensionsMenuHandler;
class ToolbarActionsModel;
class ExtensionMenuItemView;
class ExtensionActionViewController;

// The main view of the extensions menu.
class ExtensionsMenuMainPageView : public views::View {
  METADATA_HEADER(ExtensionsMenuMainPageView, views::View)

 public:
  explicit ExtensionsMenuMainPageView(Browser* browser,
                                      ExtensionsMenuHandler* menu_handler);
  ~ExtensionsMenuMainPageView() override;
  ExtensionsMenuMainPageView(const ExtensionsMenuMainPageView&) = delete;
  const ExtensionsMenuMainPageView& operator=(
      const ExtensionsMenuMainPageView&) = delete;

  // Creates and adds a menu item for `action_controller` at `index` for a
  // newly-added extension.
  void CreateAndInsertMenuItem(
      std::unique_ptr<ExtensionActionViewController> action_controller,
      extensions::ExtensionId extension_id,
      bool is_enterprise,
      ExtensionMenuItemView::SiteAccessToggleState site_access_toggle_state,
      ExtensionMenuItemView::SitePermissionsButtonState
          site_permissions_button_state,
      ExtensionMenuItemView::SitePermissionsButtonAccess
          site_permissions_button_access,
      int index);

  // Removes the menu item corresponding to `action_id`.
  void RemoveMenuItem(const ToolbarActionsModel::ActionId& action_id);

  // Returns the menu items.
  std::vector<ExtensionMenuItemView*> GetMenuItems() const;

  // Updates the site settings views with the given parameters.
  void UpdateSiteSettings(const std::u16string& current_site,
                          int label_id,
                          bool is_tooltip_visible,
                          bool is_toggle_visible,
                          bool is_toggle_on);

  // Shows the reload section in the menu. Takes precedence over the requests
  // section.
  void ShowReloadSection();

  // Show the requests section in the menu if there are any items in
  // `requests_entries_` and reload section is not visible.
  void MaybeShowRequestsSection();

  // Adds or updates the extension entry in the `requests_access_section_` with
  // the given information. Doesn't update the requests section view
  // visibility.
  void AddOrUpdateExtensionRequestingAccess(const extensions::ExtensionId& id,
                                            const std::u16string& name,
                                            const ui::ImageModel& icon,
                                            int index);

  // Remove the entry in the `requests_access_section_` corresponding to `id`,
  // if existent. Doesn't update the requests section view visibility.
  void RemoveExtensionRequestingAccess(const extensions::ExtensionId& id);

  // Clears the entries in the `request_access_section_`, if existent. Doesn't
  // update the requests section view visibility.
  void ClearExtensionsRequestingAccess();

  // Accessors used by tests:
  // Returns the currently-showing menu items.
  const std::u16string& GetSiteSettingLabelForTesting() const;
  const views::View* site_settings_tooltip() const;
  views::ToggleButton* GetSiteSettingsToggleForTesting() {
    return site_settings_toggle_;
  }
  const views::View* reload_section() const;
  const views::View* requests_section() const;
  std::vector<extensions::ExtensionId>
  GetExtensionsRequestingAccessForTesting();
  views::View* GetExtensionRequestingAccessEntryForTesting(
      const extensions::ExtensionId& extension_id);

 private:
  content::WebContents* GetActiveWebContents() const;

  // Returns the request entry for `extension_id` if existent.
  views::View* GetExtensionRequestEntry(
      const extensions::ExtensionId& extension_id) const;

  // Returns the site settings section builder, which contains information and
  // access controls for the site.
  [[nodiscard]] views::Builder<views::FlexLayoutView> CreateSiteSettingsBuilder(
      gfx::Insets margins,
      views::FlexSpecification,
      ExtensionsMenuHandler* menu_handler);

  const raw_ptr<Browser> browser_;
  const raw_ptr<ExtensionsMenuHandler> menu_handler_;

  // Site settings section.
  raw_ptr<views::Label> site_settings_label_;
  raw_ptr<views::View> site_settings_tooltip_;
  raw_ptr<views::ToggleButton> site_settings_toggle_;

  // Reload section.
  raw_ptr<views::View> reload_section_;

  // Site access requests section.
  raw_ptr<views::View> requests_section_;
  // View that holds the requests entries in `requests_section_`.
  raw_ptr<views::View> requests_entries_view_;
  // A collection of all the requests entries in `requests_section_`. This is
  // separated for easy insertion and removal of requests entries.
  std::map<extensions::ExtensionId, raw_ptr<views::View, CtnExperimental>>
      requests_entries_;

  // Menu items section. The children are guaranteed to only be
  // ExtensionMenuItemViews.
  raw_ptr<views::View> menu_items_ = nullptr;
};

BEGIN_VIEW_BUILDER(/* no export */, ExtensionsMenuMainPageView, views::View)
END_VIEW_BUILDER

DEFINE_VIEW_BUILDER(/* no export */, ExtensionsMenuMainPageView)

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_MAIN_PAGE_VIEW_H_
