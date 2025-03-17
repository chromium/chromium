// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/saved_tab_groups/most_recent_shared_tab_update_store.h"

#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "ui/views/interaction/element_tracker_views.h"

namespace tab_groups {

MostRecentSharedTabUpdateStore::MostRecentSharedTabUpdateStore(
    BrowserWindowInterface* browser_window)
    : browser_window_(browser_window) {}
MostRecentSharedTabUpdateStore::~MostRecentSharedTabUpdateStore() = default;

void MostRecentSharedTabUpdateStore::SetLastUpdatedTab(
    LocalTabGroupID group_id,
    std::optional<LocalTabID> tab_id) {
  last_updated_tab_ = {group_id, tab_id};

  MaybeShowPromo(feature_engagement::kIPHTabGroupsSharedTabChangedFeature);
}

ui::TrackedElement* MostRecentSharedTabUpdateStore::GetIPHAnchor(
    BrowserView* browser_view) {
  CHECK(last_updated_tab_.has_value());

  TabStripModel* tab_strip_model = browser_view->browser()->tab_strip_model();
  if (last_updated_tab_->second.has_value()) {
    // Last update was an active tab. Anchor to this tab.
    tabs::TabInterface* tab = SavedTabGroupUtils::GetGroupedTab(
        last_updated_tab_->first, last_updated_tab_->second.value());
    int index = tab_strip_model->GetIndexOfTab(tab);
    Tab* tab_view = browser_view->tabstrip()->tab_at(index);
    return tab_view
               ? views::ElementTrackerViews::GetInstance()->GetElementForView(
                     tab_view)
               : nullptr;
  } else {
    // Last update was removing a tab. Anchor to the tab group header.
    TabGroupHeader* tab_group_header =
        browser_view->tabstrip()->group_header(last_updated_tab_->first);
    return tab_group_header
               ? views::ElementTrackerViews::GetInstance()->GetElementForView(
                     tab_group_header)
               : nullptr;
  }
}

void MostRecentSharedTabUpdateStore::MaybeShowPromo(
    const base::Feature& feature) {
  if (auto* user_education_interface =
          browser_window_->GetUserEducationInterface()) {
    user_education_interface->MaybeShowFeaturePromo(feature);
  }
}

}  // namespace tab_groups
