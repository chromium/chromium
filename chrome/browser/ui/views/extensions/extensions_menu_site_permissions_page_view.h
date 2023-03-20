// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_SITE_PERMISSIONS_PAGE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_SITE_PERMISSIONS_PAGE_VIEW_H_

#include "extensions/common/extension_id.h"
#include "ui/views/view.h"

namespace ui {
class ImageModel;
}  // namespace ui

namespace views {
class ToggleButton;
}  // namespace views

class Browser;
class ExtensionsMenuNavigationHandler;

class ExtensionsMenuSitePermissionsPageView : public views::View {
 public:
  METADATA_HEADER(ExtensionsMenuSitePermissionsPageView);

  explicit ExtensionsMenuSitePermissionsPageView(
      Browser* browser,
      std::u16string extension_name,
      ui::ImageModel extension_icon,
      extensions::ExtensionId extension_id,
      bool is_show_requests_toggle_on,
      ExtensionsMenuNavigationHandler* navigation_handler);
  ExtensionsMenuSitePermissionsPageView(
      const ExtensionsMenuSitePermissionsPageView&) = delete;
  const ExtensionsMenuSitePermissionsPageView& operator=(
      const ExtensionsMenuSitePermissionsPageView&) = delete;
  ~ExtensionsMenuSitePermissionsPageView() override = default;

  // Updates `show_requests_toggle_` state to `is_on`.
  void UpdateShowRequestsToggle(bool is_on);

  // Sets whether the extension pointed by `extension_id_` can request access
  // the toolbar.
  void OnShowRequestsTogglePressed();

  extensions::ExtensionId extension_id() { return extension_id_; }

  // Accessors used by tests:
  views::ToggleButton* GetShowRequestsToggleForTesting() {
    return show_requests_toggle_;
  }

 private:
  const raw_ptr<Browser> browser_;
  extensions::ExtensionId extension_id_;

  raw_ptr<views::ToggleButton> show_requests_toggle_;
};

BEGIN_VIEW_BUILDER(/* no export */,
                   ExtensionsMenuSitePermissionsPageView,
                   views::View)
END_VIEW_BUILDER

DEFINE_VIEW_BUILDER(/* no export */, ExtensionsMenuSitePermissionsPageView)

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_SITE_PERMISSIONS_PAGE_VIEW_H_
