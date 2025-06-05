// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/testing/toy_tab_strip.h"

#include "base/logging.h"

namespace tabs_api::testing {

void ToyTabStrip::AddTab(ToyTab tab) {
  root_.tabs.push_back(tab);
}

std::vector<tabs::TabHandle> ToyTabStrip::GetTabs() {
  std::vector<tabs::TabHandle> result;
  for (auto& tab : root_.tabs) {
    result.push_back(tab.tab_handle);
  }
  return result;
}

void ToyTabStrip::CloseTab(size_t index) {
  if (index < 0 || index >= root_.tabs.size()) {
    LOG(FATAL) << "invalid idx passed in: " << index
               << ", tab size is: " << root_.tabs.size();
  }
  root_.tabs.erase(root_.tabs.begin() + index);
}

std::optional<int> ToyTabStrip::GetIndexForHandle(tabs::TabHandle tab_handle) {
  for (size_t i = 0; i < root_.tabs.size(); ++i) {
    if (root_.tabs.at(i).tab_handle == tab_handle) {
      return i;
    }
  }

  return std::nullopt;
}

tabs::TabHandle ToyTabStrip::AddTabAt(const GURL& url,
                                      std::optional<int> index) {
  auto tab = ToyTab{
      tabs::TabHandle(GetNextId()),
      url,
  };

  if (index.has_value()) {
    root_.tabs.insert(root_.tabs.begin() + index.value(), tab);
  } else {
    root_.tabs.push_back(tab);
  }

  return tab.tab_handle;
}

void ToyTabStrip::ActivateTab(tabs::TabHandle handle) {
  for (auto& tab : root_.tabs) {
    tab.active = tab.tab_handle == handle;
  }
}

tabs::TabHandle ToyTabStrip::FindActiveTab() {
  for (auto& tab : root_.tabs) {
    if (tab.active) {
      return tab.tab_handle;
    }
  }
  NOTREACHED() << "toy tab strip does not guarantee one tab is always active, "
                  "did you forget to activate a tab beforehand?";
}

void ToyTabStrip::MoveTab(tabs::TabHandle handle, size_t to) {
  auto idx = GetIndexForHandle(handle).value();
  auto tab = root_.tabs.at(idx);

  root_.tabs.erase(root_.tabs.begin() + idx);
  root_.tabs.insert(root_.tabs.begin() + to, tab);
}

int ToyTabStrip::GetNextId() {
  static int id = 0;
  return id++;
}

}  // namespace tabs_api::testing
