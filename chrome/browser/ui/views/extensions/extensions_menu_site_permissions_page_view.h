// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_SITE_PERMISSIONS_PAGE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_SITE_PERMISSIONS_PAGE_VIEW_H_

#include "chrome/browser/ui/views/extensions/extensions_menu_page_view.h"
#include "extensions/common/extension_id.h"

namespace ui {
class ImageModel;
}  // namespace ui

class ExtensionsMenuNavigationHandler;

class ExtensionsMenuSitePermissionsPage : public ExtensionsMenuPageView {
 public:
  explicit ExtensionsMenuSitePermissionsPage(
      std::u16string extension_name,
      ui::ImageModel extension_icon,
      extensions::ExtensionId extension_id,
      ExtensionsMenuNavigationHandler* navigation_handler);
  ExtensionsMenuSitePermissionsPage(const ExtensionsMenuSitePermissionsPage&) =
      delete;
  const ExtensionsMenuSitePermissionsPage& operator=(
      const ExtensionsMenuSitePermissionsPage&) = delete;
  ~ExtensionsMenuSitePermissionsPage() override = default;

  // ExtensionsMenuPageView:
  void Update(content::WebContents* web_contents) override;

  // Accessors used by tests:
  extensions::ExtensionId GetExtensionIdForTesting() { return extension_id_; }

 private:
  extensions::ExtensionId extension_id_;
};

BEGIN_VIEW_BUILDER(/* no export */,
                   ExtensionsMenuSitePermissionsPage,
                   ExtensionsMenuPageView)
END_VIEW_BUILDER

DEFINE_VIEW_BUILDER(/* no export */, ExtensionsMenuSitePermissionsPage)

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_SITE_PERMISSIONS_PAGE_VIEW_H_
