// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_side_panel_controller_utils.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_side_panel_controller.h"
#include "chrome/browser/ui/views/side_panel/side_panel_ui.h"

void ShowReadAnythingSidePanel(Browser* browser,
                               SidePanelOpenTrigger open_trigger) {
  SidePanelUI* side_panel_ui = browser->GetFeatures().side_panel_ui();
  if (!side_panel_ui) {
    return;
  }
  side_panel_ui->Show(SidePanelEntryId::kReadAnything, open_trigger);
}

bool IsReadAnythingEntryShowing(Browser* browser) {
  SidePanelUI* side_panel_ui = browser->GetFeatures().side_panel_ui();
  if (!side_panel_ui) {
    return false;
  }
  return side_panel_ui->IsSidePanelShowing() &&
         (side_panel_ui->GetCurrentEntryId() ==
          SidePanelEntryId::kReadAnything);
}
