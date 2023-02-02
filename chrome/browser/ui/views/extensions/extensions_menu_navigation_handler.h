// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_NAVIGATION_HANDLER_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_NAVIGATION_HANDLER_H_

#include "extensions/common/extension_id.h"

// An interface that provides methods to navigate between pages of the
// extensions menu.
class ExtensionsMenuNavigationHandler {
 public:
  virtual ~ExtensionsMenuNavigationHandler() = default;

  // Creates and opens the main page in the menu, if it exists.
  virtual void OpenMainPage() = 0;

  // Creates and opens the site permissions page for `extension_id` in the menu,
  // if it exists.
  virtual void OpenSitePermissionsPage(
      extensions::ExtensionId extension_id) = 0;

  // Closes the currently-showing extensions menu, if it exists.
  virtual void CloseBubble() = 0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_NAVIGATION_HANDLER_H_
