// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_EXTENSIONS_MENU_CORE_CONTROLLER_H_
#define CHROME_BROWSER_UI_EXTENSIONS_EXTENSIONS_MENU_CORE_CONTROLLER_H_

// A platform-independent controller for the extensions menu.
class ExtensionsMenuCoreController {
 public:
  ExtensionsMenuCoreController();
  ExtensionsMenuCoreController(const ExtensionsMenuCoreController&) = delete;
  const ExtensionsMenuCoreController& operator=(
      const ExtensionsMenuCoreController&) = delete;
  ~ExtensionsMenuCoreController();
};

#endif  // CHROME_BROWSER_UI_EXTENSIONS_EXTENSIONS_MENU_CORE_CONTROLLER_H_
