// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_COORDINATOR_H_

#include "ui/views/view_tracker.h"

class Browser;

// Handles the lifetime and showing/hidden state of the extensions menu bubble.
class ExtensionsMenuCoordinator {
 public:
  explicit ExtensionsMenuCoordinator(Browser* browser);
  ExtensionsMenuCoordinator(const ExtensionsMenuCoordinator&) = delete;
  const ExtensionsMenuCoordinator& operator=(const ExtensionsMenuCoordinator&) =
      delete;
  ~ExtensionsMenuCoordinator();

  // Displays the extensions menu under `anchor_view`.
  void Show(views::View* anchor_view);

  // Hides the currently-showing extensions menu, if it exists.
  void Hide();

  // Returns true if the extensions menu is showing.
  bool IsShowing() const;

  // Returns the currently-showing extensions menu widget, if it exists.
  views::Widget* GetExtensionsMenuWidget();

 private:
  raw_ptr<Browser> browser_;
  views::ViewTracker bubble_tracker_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_COORDINATOR_H_
