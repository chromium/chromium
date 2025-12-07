// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TESTING_TOY_TAB_STRIP_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TESTING_TOY_TAB_STRIP_H_

#include <set>

#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/tabs/public/tab_collection.h"
#include "components/tabs/public/tab_interface.h"
#include "components/tabs/public/tab_strip_collection.h"
#include "url/gurl.h"

namespace tabs_api::testing {
struct ToyTab {
  tabs::TabHandle tab_handle;
  GURL gurl;
  bool active = false;
  bool selected = false;
};

struct ToyTabCollection {
  tabs::TabCollection::Handle collection_handle;
  std::vector<ToyTab> tabs;
};

struct ToyTabGroupData {
  tab_groups::TabGroupId id;
  tabs::TabCollectionHandle handle;
  tab_groups::TabGroupVisualData visuals;
};

// A toy tab strip for integration testing. Toy tab strip is a simple
// shallow tree backed by a vector of "tabs."
class ToyTabStrip {
 public:
  ToyTabStrip();
  ToyTabStrip(const ToyTabStrip&) = delete;
  ToyTabStrip& operator=(const ToyTabStrip&) = delete;
  ~ToyTabStrip() = default;

  ToyTab GetToyTabFor(tabs::TabHandle handle) const;

  void AddTab(ToyTab tab);
  std::vector<tabs::TabHandle> GetTabs();
  void CloseTab(size_t index);
  std::optional<int> GetIndexForHandle(tabs::TabHandle tab_handle);
  tabs::TabHandle AddTabAt(const GURL& url, std::optional<int> index);
  void ActivateTab(tabs::TabHandle handle);
  tabs::TabHandle FindActiveTab();
  void MoveTab(tabs::TabHandle handle, size_t to);

  std::optional<tab_groups::TabGroupId> GetGroupIdFor(
      const tabs::TabCollectionHandle& handle) const;
  tabs::TabCollectionHandle AddGroup(
      const tab_groups::TabGroupVisualData& visual_data);
  const tab_groups::TabGroupVisualData* GetGroupVisualData(
      const tabs::TabCollectionHandle& handle) const;
  void UpdateGroupVisuals(const tab_groups::TabGroupId& group_id,
                          const tab_groups::TabGroupVisualData& new_visuals);

  void SetActiveTab(tabs::TabHandle handle);
  void SetTabSelection(std::set<tabs::TabHandle> selection);
  tabs::TabCollectionHandle GetRoot() { return root_.collection_handle; }

 protected:
  // An ever incrementing id.
  int GetNextId();

 private:
  std::vector<ToyTabGroupData> groups_with_visuals_;
  std::unique_ptr<tabs::TabStripCollection> tab_strip_collection_;
  ToyTabCollection root_;
};

}  // namespace tabs_api::testing

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TESTING_TOY_TAB_STRIP_H_
