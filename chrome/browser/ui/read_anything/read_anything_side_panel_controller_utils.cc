// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/read_anything/read_anything_side_panel_controller_utils.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/browser/ui/views/side_panel/side_panel_ui.h"

void ShowReadAnythingSidePanel(Browser* browser,
                               SidePanelOpenTrigger open_trigger) {
  SidePanelUI* side_panel_ui = browser->GetFeatures().side_panel_ui();
  if (!side_panel_ui) {
    return;
  }
  side_panel_ui->Show(SidePanelEntryId::kReadAnything, open_trigger);
}

bool IsReadAnythingEntryShowing(BrowserWindowInterface* browser) {
  // The side panel is not immediately hidden, and IsSidePanelEntryShowing
  // may return true for a few seconds after the panel is visually closed. This
  // can lead to a race condition where is ReadAnythingEntryShowing incorrectly
  // returns true, which means that read anything might not be added to the
  // context menu. To fix this, IsReadAnythingEntryShowing should also return
  // false if the side panel is in the process of closing.
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  if (browser_view && browser_view->contents_height_side_panel()->state() ==
                          SidePanel::State::kClosing) {
    return false;
  }

  SidePanelUI* side_panel_ui = browser->GetFeatures().side_panel_ui();
  if (!side_panel_ui) {
    return false;
  }
  return side_panel_ui->IsSidePanelEntryShowing(
      SidePanelEntryKey(SidePanelEntryId::kReadAnything));
}
