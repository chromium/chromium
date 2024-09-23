// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_GROUP_H_
#define CHROME_BROWSER_UI_TABS_TAB_GROUP_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "ui/gfx/range/range.h"

class TabGroupController;

// The metadata and state of a tab group. This handles state changes that are
// specific to tab groups and not grouped tabs. The latter (i.e. the groupness
// state of a tab) is handled by TabStripModel, which also notifies TabStrip of
// any grouped tab state changes that need to be reflected in the view. TabGroup
// handles similar notifications for tab group state changes.
class TabGroup {
 public:
  TabGroup(TabGroupController* controller,
           const tab_groups::TabGroupId& id,
           const tab_groups::TabGroupVisualData& visual_data);
  ~TabGroup();

  const tab_groups::TabGroupId& id() const { return id_; }
  const tab_groups::TabGroupVisualData* visual_data() const {
    return visual_data_.get();
  }

  // Sets the visual data of the tab group. |is_customized| is true when this
  // method is called from the user explicitly setting the data and defaults to
  // false for callsites that may set the data such as tab restore. Once set to
  // true, |is_customized| cannot be reset to false.
  void SetVisualData(tab_groups::TabGroupVisualData visual_data,
                     bool is_customized = false);

  // Returns a user-visible string describing the contents of the group, such as
  // "Google Search and 3 other tabs". Used for accessibly describing the group,
  // as well as for displaying in context menu items and tooltips when the group
  // is unnamed.
  std::u16string GetContentString() const;

  // Updates internal bookkeeping for group contents, and notifies the
  // controller that contents changed when a tab is added.
  void AddTab();

  // Updates internal bookkeeping for group contents, and notifies the
  // controller that contents changed when a tab is removed.
  void RemoveTab();

  // The number of tabs in this group, determined by AddTab() and
  // RemoveTab() calls.
  int tab_count() const { return tab_count_; }

  // Returns whether the group has no tabs.
  bool IsEmpty() const;

  // Returns whether the user has explicitly set the visual data themselves.
  bool IsCustomized() const;

  // Gets the model index of this group's first tab, or nullopt if it is
  // empty. Similar to ListTabs() it traverses through TabStripModel's
  // tabs. Unlike ListTabs() this is always safe to call.
  std::optional<int> GetFirstTab() const;

  // Gets the model index of this group's last tab, or nullopt if it is
  // empty. Similar to ListTabs() it traverses through TabStripModel's
  // tabs. Unlike ListTabs() this is always safe to call.
  std::optional<int> GetLastTab() const;

  // Returns the range of tab model indices this group contains. Notably
  // does not rely on the TabGroup's internal metadata, but rather
  // traverses directly through the tabs in TabStripModel.
  //
  // The returned range will never be a reverse range. It will always be
  // a forward range, or the empty range {0,0}.
  //
  // This method can only be called when a group is contiguous. A group
  // may not be contiguous in some TabStripModel intermediate states.
  // Notably, any operation that groups tabs then moves them into
  // position exposes these states.
  //
  // This is only a concern for TabStripModelObservers who query
  // TabStripModel after observer notifications. While each call to
  // TabStripModel's public API leaves groups in a contiguous state,
  // observers are notified of some changes in during intermediate
  // steps.
  gfx::Range ListTabs() const;

 private:
  raw_ptr<TabGroupController> controller_;

  tab_groups::TabGroupId id_;
  std::unique_ptr<tab_groups::TabGroupVisualData> visual_data_;

  int tab_count_ = 0;

  bool is_customized_ = false;
};

#endif  // CHROME_BROWSER_UI_TABS_TAB_GROUP_H_
