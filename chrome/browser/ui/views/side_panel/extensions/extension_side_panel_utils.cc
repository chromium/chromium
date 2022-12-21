// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/extensions/extension_side_panel_utils.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"

namespace extensions {

SidePanelRegistry* GetGlobalSidePanelRegistry(Browser* browser) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  if (!browser_view) {
    return nullptr;
  }

  SidePanelCoordinator* coordinator = browser_view->side_panel_coordinator();
  return coordinator ? coordinator->GetGlobalSidePanelRegistry() : nullptr;
}

}  // namespace extensions
