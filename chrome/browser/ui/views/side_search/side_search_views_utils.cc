// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_search/side_search_views_utils.h"

#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_search/side_search_browser_controller.h"

namespace side_search {

bool IsSideSearchToggleOpen(BrowserView* browser_view) {
  if (base::FeatureList::IsEnabled(features::kUnifiedSidePanel)) {
    auto* coordinator = browser_view->side_panel_coordinator();
    return coordinator->IsSidePanelShowing() &&
           coordinator->GetCurrentEntryId() == SidePanelEntry::Id::kSideSearch;
  }
  return browser_view->side_search_controller()->GetSidePanelToggledOpen();
}

}  // namespace side_search
