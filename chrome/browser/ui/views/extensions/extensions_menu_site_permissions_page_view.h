// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_SITE_PERMISSIONS_PAGE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_SITE_PERMISSIONS_PAGE_VIEW_H_

#include "extensions/browser/permissions_manager.h"
#include "extensions/common/extension_id.h"
#include "ui/views/view.h"

namespace ui {
class ImageModel;
}  // namespace ui

namespace views {
class ImageView;
class Label;
class RadioButton;
class ToggleButton;
class RadioButton;
}  // namespace views

class Browser;
class ExtensionsMenuHandler;

class ExtensionsMenuSitePermissionsPageView : public views::View {
  METADATA_HEADER(ExtensionsMenuSitePermissionsPageView, views::View)

 public:
  explicit ExtensionsMenuSitePermissionsPageView(
      Browser* browser,
      extensions::ExtensionId extension_id,
      ExtensionsMenuHandler* menu_handler);
  ExtensionsMenuSitePermissionsPageView(
      const ExtensionsMenuSitePermissionsPageView&) = delete;
  const ExtensionsMenuSitePermissionsPageView& operator=(
      const ExtensionsMenuSitePermissionsPageView&) = delete;
  ~ExtensionsMenuSitePermissionsPageView() override = default;

  // Updates the page contents with the given parameters.
  void Update(const std::u16string& extension_name,
              const ui::ImageModel& extension_icon,
              const std::u16string& current_site,
              extensions::PermissionsManager::UserSiteAccess user_site_access,
              bool is_show_requests_toggle_on,
              bool is_on_site_enabled,
              bool is_on_all_sites_enabled);

  // Updates `show_requests_toggle_` state to `is_on`.
  void UpdateShowRequestsToggle(bool is_on);

  extensions::ExtensionId extension_id() { return extension_id_; }

  // Accessors used by tests:
  views::ToggleButton* GetShowRequestsToggleForTesting() {
    return show_requests_toggle_;
  }
  views::RadioButton* GetSiteAccessButtonForTesting(
      extensions::PermissionsManager::UserSiteAccess site_access);

 private:
  const raw_ptr<Browser> browser_;
  extensions::ExtensionId extension_id_;

  raw_ptr<views::ImageView> extension_icon_;
  raw_ptr<views::Label> extension_name_;
  raw_ptr<views::ToggleButton> show_requests_toggle_;
};

BEGIN_VIEW_BUILDER(/* no export */,
                   ExtensionsMenuSitePermissionsPageView,
                   views::View)
END_VIEW_BUILDER

DEFINE_VIEW_BUILDER(/* no export */, ExtensionsMenuSitePermissionsPageView)

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_SITE_PERMISSIONS_PAGE_VIEW_H_
