// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_TOOLBAR_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_TOOLBAR_COORDINATOR_H_

#include "ui/views/view_tracker.h"

class ExtensionsToolbarContainer;
class ExtensionsToolbarContainerViewController;
class Browser;

class ExtensionsToolbarCoordinator final {
 public:
  explicit ExtensionsToolbarCoordinator(
      Browser* browser,
      ExtensionsToolbarContainer* extensions_container);
  ExtensionsToolbarCoordinator(const ExtensionsToolbarCoordinator&) = delete;
  const ExtensionsToolbarCoordinator& operator=(
      const ExtensionsToolbarCoordinator&) = delete;
  ~ExtensionsToolbarCoordinator();

  ExtensionsToolbarContainerViewController*
  GetExtensionsContainerViewController() {
    return extensions_container_controller_.get();
  }

 private:
  void ResetCoordinatorState();

  views::ViewTracker extensions_container_tracker_;
  std::unique_ptr<ExtensionsToolbarContainerViewController>
      extensions_container_controller_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_TOOLBAR_COORDINATOR_H_
