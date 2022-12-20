// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_SITE_PERMISSIONS_PAGE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_SITE_PERMISSIONS_PAGE_VIEW_H_

#include "ui/views/view.h"

class ExtensionsMenuNavigationHandler;

class ExtensionsMenuSitePermissionsPage : public views::View {
 public:
  explicit ExtensionsMenuSitePermissionsPage(
      ExtensionsMenuNavigationHandler* navigation_handler);
  ExtensionsMenuSitePermissionsPage(const ExtensionsMenuSitePermissionsPage&) =
      delete;
  const ExtensionsMenuSitePermissionsPage& operator=(
      const ExtensionsMenuSitePermissionsPage&) = delete;
  ~ExtensionsMenuSitePermissionsPage() override = default;
};

BEGIN_VIEW_BUILDER(/* no export */,
                   ExtensionsMenuSitePermissionsPage,
                   views::View)
END_VIEW_BUILDER

DEFINE_VIEW_BUILDER(/* no export */, ExtensionsMenuSitePermissionsPage)

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_SITE_PERMISSIONS_PAGE_VIEW_H_
