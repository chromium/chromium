// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/testing/toy_tab_strip.h"

#include "base/logging.h"

namespace tabs_api::testing {

void ToyTabStrip::AddTab(ToyTab tab) {
  tabs_.push_back(tab);
}

std::vector<tabs::TabHandle> ToyTabStrip::GetTabs() {
  std::vector<tabs::TabHandle> result;
  for (auto& tab : tabs_) {
    result.push_back(tab.tab_handle);
  }
  return result;
}

void ToyTabStrip::CloseTab(size_t index) {
  if (index < 0 || index >= tabs_.size()) {
    LOG(FATAL) << "invalid idx passed in: " << index
               << ", tab size is: " << tabs_.size();
  }
  tabs_.erase(tabs_.begin() + index);
}

std::optional<int> ToyTabStrip::GetIndexForHandle(tabs::TabHandle tab_handle) {
  for (size_t i = 0; i < tabs_.size(); ++i) {
    if (tabs_.at(i).tab_handle == tab_handle) {
      return i;
    }
  }

  return std::nullopt;
}

tabs::TabHandle ToyTabStrip::AddTabAt(const GURL& url,
                                      std::optional<int> index) {
  // Need to start at 1 because 0 is reserved for null value.
  static int tab_handle_counter = 1;

  auto tab = ToyTab{
      url,
      tabs::TabHandle(tab_handle_counter++),
  };

  if (index.has_value()) {
    tabs_.insert(tabs_.begin() + index.value(), tab);
  } else {
    tabs_.push_back(tab);
  }

  return tab.tab_handle;
}

void ToyTabStrip::ActivateTab(tabs::TabHandle handle) {
  for (auto& tab : tabs_) {
    tab.active = tab.tab_handle == handle;
  }
}

tabs::TabHandle ToyTabStrip::FindActiveTab() {
  for (auto& tab : tabs_) {
    if (tab.active) {
      return tab.tab_handle;
    }
  }
  NOTREACHED() << "toy tab strip does not guarantee one tab is always active, "
                  "did you forget to activate a tab beforehand?";
}

}  // namespace tabs_api::testing
