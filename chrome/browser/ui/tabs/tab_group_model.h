// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_GROUP_MODEL_H_
#define CHROME_BROWSER_UI_TABS_TAB_GROUP_MODEL_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <optional>
#include <vector>

#include "base/memory/raw_ptr.h"

class TabGroup;
class TabGroupController;

namespace tab_groups {
enum class TabGroupColorId;
class TabGroupId;
class TabGroupVisualData;
}  // namespace tab_groups

// A model for all tab groups with at least one tab in the tabstrip. Keeps a map
// of tab_groups::TabGroupIds and TabGroups and provides an API for maintaining
// it. It is owned and used primarily by TabStripModel, which handles
// tab-to-group correspondences and any groupedness state changes on tabs.
// TabStipModel then notifies TabStrip of any groupedness state changes that
// need to be reflected in the view.
class TabGroupModel {
 public:
  explicit TabGroupModel(TabGroupController* controller);
  ~TabGroupModel();

  // Registers a tab group and returns the newly registered group. It will
  // initially be empty, but the expectation is that at least one tab will be
  // added to it immediately.
  TabGroup* AddTabGroup(
      const tab_groups::TabGroupId& id,
      std::optional<tab_groups::TabGroupVisualData> visual_data);

  // Returns whether a tab group with the given |id| exists.
  bool ContainsTabGroup(const tab_groups::TabGroupId& id) const;

  // Returns the tab group with the given |id|. The group must exist.
  TabGroup* GetTabGroup(const tab_groups::TabGroupId& id) const;

  // Removes the tab group with the given |id| from the registry. Should be
  // called whenever the group becomes empty.
  void RemoveTabGroup(const tab_groups::TabGroupId& id);

  std::vector<tab_groups::TabGroupId> ListTabGroups() const;

 private:
  std::map<tab_groups::TabGroupId, std::unique_ptr<TabGroup>> groups_;

  // Used to maintain insertion order of TabGroupsIds added to the
  // TabGroupModel.
  std::vector<tab_groups::TabGroupId> group_ids_;

  raw_ptr<TabGroupController> controller_;

  // Returns the least-used color in the color set, breaking ties toward the
  // first color in the set. Used to initialize a new group's color, which
  // should be as distinct from the other groups as possible.
  tab_groups::TabGroupColorId GetNextColor() const;
};

#endif  // CHROME_BROWSER_UI_TABS_TAB_GROUP_MODEL_H_
