// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_search/side_search_views_utils.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/views/side_panel/side_panel_ui.h"

namespace side_search {

bool IsSideSearchToggleOpen(Browser* browser) {
  SidePanelUI* side_panel_ui = browser->GetFeatures().side_panel_ui();
  return side_panel_ui && side_panel_ui->IsSidePanelShowing() &&
         side_panel_ui->GetCurrentEntryId() == SidePanelEntryId::kSideSearch;
}

}  // namespace side_search
