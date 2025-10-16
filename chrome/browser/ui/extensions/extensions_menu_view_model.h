// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_EXTENSIONS_MENU_VIEW_MODEL_H_
#define CHROME_BROWSER_UI_EXTENSIONS_EXTENSIONS_MENU_VIEW_MODEL_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/extensions/extensions_menu_view_platform_delegate.h"

// The platform agnostic controller for the extensions menu.
// TODO(crbug.com/449814184): Move the observers from
// ExtensionsMenuViewController here.
class ExtensionsMenuViewModel {
 public:
  explicit ExtensionsMenuViewModel(
      std::unique_ptr<ExtensionsMenuViewPlatformDelegate> platform_delegate);
  ExtensionsMenuViewModel(const ExtensionsMenuViewModel&) = delete;
  const ExtensionsMenuViewModel& operator=(const ExtensionsMenuViewModel&) =
      delete;
  ~ExtensionsMenuViewModel();

 private:
  std::unique_ptr<ExtensionsMenuViewPlatformDelegate> platform_delegate_;
};

#endif  // CHROME_BROWSER_UI_EXTENSIONS_EXTENSIONS_MENU_VIEW_MODEL_H_
