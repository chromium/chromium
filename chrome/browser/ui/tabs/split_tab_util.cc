// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/split_tab_util.h"

#include <algorithm>

#include "base/check_op.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_external.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_source.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/tabs/public/split_tab_data.h"
#include "components/tabs/public/split_tab_id.h"

namespace split_tabs {

int GetIndexOfLastActiveTab(TabStripModel* tab_strip_model,
                            split_tabs::SplitTabId split_id) {
  const std::vector<tabs::TabInterface*> split_tabs =
      tab_strip_model->GetSplitData(split_id)->ListTabs();
  // Check if a tab is activated first before comparing the WebContent's last
  // focused time because the activation happens before focus time updates and
  // wouldn't be accurate for tabs that just had its activation state changed.
  for (int index = tab_strip_model->GetIndexOfTab(split_tabs[0]);
       tabs::TabInterface* tab : split_tabs) {
    if (tab->IsActivated()) {
      return index;
    }
    index++;
  }

  tabs::TabInterface* const recently_active = *std::max_element(
      split_tabs.begin(), split_tabs.end(),
      [](tabs::TabInterface* a, tabs::TabInterface* b) {
        auto get_last_focused_time_for_tab = [](const tabs::TabInterface* tab) {
          return resource_coordinator::TabLifecycleUnitSource::
              GetTabLifecycleUnitExternal(tab->GetContents())
                  ->GetLastFocusedTime();
        };
        return get_last_focused_time_for_tab(b) >
               get_last_focused_time_for_tab(a);
      });

  return tab_strip_model->GetIndexOfTab(recently_active);
}

SplitTabActiveLocation GetLastActiveTabLocation(
    TabStripModel* tab_strip_model,
    split_tabs::SplitTabId split_id) {
  split_tabs::SplitTabData* const split_tab_data =
      tab_strip_model->GetSplitData(split_id);
  std::vector<tabs::TabInterface*> tabs_in_split = split_tab_data->ListTabs();
  CHECK_EQ(tabs_in_split.size(), 2U);

  const int last_active_index =
      GetIndexOfLastActiveTab(tab_strip_model, split_id);
  CHECK_NE(last_active_index, TabStripModel::kNoTab);

  const int first_tab_index = tab_strip_model->GetIndexOfTab(tabs_in_split[0]);
  const bool first_tab_activated = last_active_index == first_tab_index;

  if (split_tab_data->visual_data()->split_layout() ==
      SplitTabLayout::kVertical) {
    return first_tab_activated ? SplitTabActiveLocation::kStart
                               : SplitTabActiveLocation::kEnd;
  } else {
    return first_tab_activated ? SplitTabActiveLocation::kTop
                               : SplitTabActiveLocation::kBottom;
  }
}
}  // namespace split_tabs
