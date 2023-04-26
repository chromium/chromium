// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/side_panel/read_anything/read_anything_side_panel_controller_utils.h"

#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"

void ShowReadAnythingSidePanel(Browser* browser) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  if (!browser_view) {
    return;
  }
  browser_view->side_panel_coordinator()->Show(
      SidePanelEntry::Id::kReadAnything,
      SidePanelUtil::SidePanelOpenTrigger::kReadAnythingContextMenu);
}

bool IsReadAnythingEntryShowing(Browser* browser) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  if (!browser_view) {
    return false;
  }
  auto* side_panel_coordinator = browser_view->side_panel_coordinator();
  return side_panel_coordinator->IsSidePanelShowing() &&
         (side_panel_coordinator->GetCurrentEntryId() ==
          SidePanelEntry::Id::kReadAnything);
}
