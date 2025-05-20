// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TABS_PUBLIC_TAB_GROUP_H_
#define COMPONENTS_TABS_PUBLIC_TAB_GROUP_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "ui/gfx/range/range.h"

namespace tabs {

class TabGroupTabCollection;
class TabInterface;

}  // namespace tabs

// The metadata and state of a tab group. This handles state changes that are
// specific to tab groups and not grouped tabs. The latter (i.e. the groupness
// state of a tab) is handled by TabStripModel, which also notifies TabStrip of
// any grouped tab state changes that need to be reflected in the view. TabGroup
// handles similar notifications for tab group state changes.
class TabGroup {
 public:
  TabGroup(tabs::TabGroupTabCollection* collection,
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
  void SetGroupIsClosing(bool is_closing);
  bool IsGroupClosing() { return is_closing_; }

  // Updates internal bookkeeping for group contents.
  void AddTab();
  void RemoveTab();

  // The number of tabs in this group, determined by AddTab() and
  // RemoveTab() calls.
  int tab_count() const { return tab_count_; }

  // Returns whether the group has no tabs.
  bool IsEmpty() const;

  // Returns whether the user has explicitly set the visual data themselves.
  bool IsCustomized() const;

  // Get the first tab in the Tab Group or nullptr if the group is currently
  // empty.
  tabs::TabInterface* GetFirstTab() const;

  // Get the last tab in the Tab Group or nullptr if the group is currently
  // empty.
  tabs::TabInterface* GetLastTab() const;

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
  // The collection that owns the TabGroup.
  raw_ptr<tabs::TabGroupTabCollection> collection_;

  tab_groups::TabGroupId id_;
  std::unique_ptr<tab_groups::TabGroupVisualData> visual_data_;

  int tab_count_ = 0;

  bool is_closing_ = false;
  bool is_customized_ = false;
};

#endif  // COMPONENTS_TABS_PUBLIC_TAB_GROUP_H_
