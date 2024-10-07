// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_CONTENTS_DATA_H_
#define CHROME_BROWSER_UI_TABS_TAB_CONTENTS_DATA_H_

#include <memory>
#include <optional>
#include <vector>

namespace tabs {
class TabModel;
}  // namespace tabs

namespace tab_groups {
class TabGroupId;
}  // namespace tab_groups

class TabGroupModel;

// This is an interface between TabStripModel and the underlying implementation
// of the storage. This provides different APIs for the model to manipulate tabs
// and the state of the tabstrip.
class TabContentsData {
 public:
  TabContentsData();
  virtual ~TabContentsData();
  TabContentsData(const TabContentsData&) = delete;
  TabContentsData& operator=(const TabContentsData&) = delete;

  virtual size_t TotalTabCount() const = 0;
  virtual size_t IndexOfFirstNonPinnedTab() const = 0;

  virtual tabs::TabModel* GetTabAtIndexRecursive(size_t index) const = 0;

  virtual std::optional<size_t> GetIndexOfTabRecursive(
      const tabs::TabModel* tab_model) const = 0;

  virtual void AddTabRecursive(
      std::unique_ptr<tabs::TabModel> tab_model,
      size_t index,
      std::optional<tab_groups::TabGroupId> new_group_id,
      bool new_pinned_state) = 0;

  virtual std::unique_ptr<tabs::TabModel> RemoveTabAtIndexRecursive(
      size_t index) = 0;

  virtual void MoveTabRecursive(
      size_t initial_index,
      size_t final_index,
      std::optional<tab_groups::TabGroupId> new_group_id,
      bool new_pinned_state) = 0;

  virtual void MoveTabsRecursive(
      const std::vector<int>& tab_indices,
      size_t destination_index,
      std::optional<tab_groups::TabGroupId> new_group_id,
      bool new_pinned_state) = 0;

  virtual void MoveGroupTo(const TabGroupModel* group_model,
                           const tab_groups::TabGroupId& group,
                           int to_index) = 0;

  virtual void ValidateData(const TabGroupModel* group_model) = 0;
};

std::unique_ptr<TabContentsData> CreateTabContentsDataImpl();

#endif  // CHROME_BROWSER_UI_TABS_TAB_CONTENTS_DATA_H_
