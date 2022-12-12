// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_BASE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_BASE_VIEW_H_

#include "chrome/browser/ui/views/controls/page_switcher_view.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_navigation_handler.h"
#include "ui/views/view.h"

class Browser;

// The views implementation of the extensions menu UI.
class ExtensionsMenuBaseView : public views::View,
                               public ExtensionsMenuNavigationHandler {
 public:
  explicit ExtensionsMenuBaseView(Browser* browser);
  ~ExtensionsMenuBaseView() override = default;
  ExtensionsMenuBaseView(const ExtensionsMenuBaseView&) = delete;
  const ExtensionsMenuBaseView& operator=(const ExtensionsMenuBaseView&) =
      delete;

  // ExtensionsMenuNavigationHandler:
  void CloseBubble() override;

 private:
  raw_ptr<PageSwitcherView> page_container_;
};

BEGIN_VIEW_BUILDER(/* no export */, ExtensionsMenuBaseView, views::View)
END_VIEW_BUILDER

DEFINE_VIEW_BUILDER(/* no export */, ExtensionsMenuBaseView)

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_BASE_VIEW_H_
