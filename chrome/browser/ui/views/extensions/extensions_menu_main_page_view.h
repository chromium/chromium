// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_MAIN_PAGE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_MAIN_PAGE_VIEW_H_

#include "ui/views/view.h"

class ExtensionsMenuNavigationHandler;

// The main view of the extensions menu.
class ExtensionsMenuMainPageView : public views::View {
 public:
  explicit ExtensionsMenuMainPageView(
      ExtensionsMenuNavigationHandler* navigation_handler);
  ~ExtensionsMenuMainPageView() override = default;
  ExtensionsMenuMainPageView(const ExtensionsMenuMainPageView&) = delete;
  const ExtensionsMenuMainPageView& operator=(
      const ExtensionsMenuMainPageView&) = delete;

 private:
  raw_ptr<ExtensionsMenuNavigationHandler> navigation_handler_;
};

BEGIN_VIEW_BUILDER(/* no export */, ExtensionsMenuMainPageView, views::View)
END_VIEW_BUILDER

DEFINE_VIEW_BUILDER(/* no export */, ExtensionsMenuMainPageView)

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_MAIN_PAGE_VIEW_H_
