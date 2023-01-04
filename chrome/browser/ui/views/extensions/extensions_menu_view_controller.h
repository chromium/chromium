// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_VIEW_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_navigation_handler.h"

class Browser;
class PageSwitcherView;

class ExtensionsMenuViewController : public ExtensionsMenuNavigationHandler {
 public:
  ExtensionsMenuViewController(Browser* browser,
                               PageSwitcherView* contents_view);
  ExtensionsMenuViewController(const ExtensionsMenuViewController&) = delete;
  const ExtensionsMenuViewController& operator=(
      const ExtensionsMenuViewController&) = delete;
  ~ExtensionsMenuViewController() override = default;

  // ExtensionsMenuNavigationHandler:
  void OpenMainPage() override;
  void OpenSitePermissionsPage() override;
  void CloseBubble() override;

 private:
  const raw_ptr<Browser> browser_;
  const raw_ptr<PageSwitcherView> contents_view_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_VIEW_CONTROLLER_H_
