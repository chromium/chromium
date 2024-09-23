// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_LAYOUT_HELPER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_LAYOUT_HELPER_H_

#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/ui/tabs/tab_types.h"
#include "chrome/browser/ui/views/tabs/tab_layout_state.h"
#include "chrome/browser/ui/views/tabs/tab_slot_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip_layout.h"
#include "chrome/browser/ui/views/tabs/tab_width_constraints.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/view_model.h"

class Tab;
class TabGroupHeader;
class TabContainerController;

namespace tab_groups {
class TabGroupId;
}

// Helper class for TabStrip, that is responsible for calculating and assigning
// layouts for tabs and group headers. It tracks animations and changes to the
// model so that it has all necessary information for layout purposes.
class TabStripLayoutHelper {
 public:
  using GetTabsCallback = base::RepeatingCallback<views::ViewModelT<Tab>*()>;

  TabStripLayoutHelper(const TabContainerController& controller,
                       GetTabsCallback get_tabs_callback);
  TabStripLayoutHelper(const TabStripLayoutHelper&) = delete;
  TabStripLayoutHelper& operator=(const TabStripLayoutHelper&) = delete;
  ~TabStripLayoutHelper();

  // Returns a vector of all tabs in the strip, including both closing tabs
  // and tabs still in the model.
  std::vector<Tab*> GetTabs() const;

  // Get all tab slot views in visual order, including all tabs from
  // GetTabs() and all tab group headers.
  std::vector<TabSlotView*> GetTabSlotViews() const;

  int active_tab_width() { return active_tab_width_; }
  int inactive_tab_width() { return inactive_tab_width_; }

  // Returns the number of pinned tabs in the tabstrip.
  size_t GetPinnedTabCount() const;

  // Returns a map of all tab groups and their bounds.
  const std::map<tab_groups::TabGroupId, gfx::Rect>& group_header_ideal_bounds()
      const {
    return group_header_ideal_bounds_;
  }

  // Inserts a new tab at |index|.
  void InsertTabAt(int model_index, Tab* tab, TabPinned pinned);

  // Marks the tab at |model_index| as closing, but does not remove it from
  // |slots_|.
  void MarkTabAsClosing(int model_index, Tab* tab);

  // Removes `tab` from `slots_`.
  void RemoveTab(Tab* tab);

  // Moves the tab at |prev_index| with group |moving_tab_group| to |new_index|.
  // Also updates the group header's location if necessary.
  void MoveTab(std::optional<tab_groups::TabGroupId> moving_tab_group,
               int prev_index,
               int new_index);

  // Sets the tab at |index|'s pinned state to |pinned|.
  void SetTabPinned(int model_index, TabPinned pinned);

  // Inserts a new group header for |group|.
  void InsertGroupHeader(tab_groups::TabGroupId group, TabGroupHeader* header);

  // Removes the group header for |group|.
  void RemoveGroupHeader(tab_groups::TabGroupId group);

  // Ensures the group header for |group| is at the correct index. Should be
  // called externally when group membership changes but nothing else about the
  // layout does.
  void UpdateGroupHeaderIndex(tab_groups::TabGroupId group);

  // Changes the active tab from |prev_active_index| to |new_active_index|.
  void SetActiveTab(std::optional<size_t> prev_active_index,
                    std::optional<size_t> new_active_index);

  // Calculates the smallest width the tabs can occupy.
  int CalculateMinimumWidth();

  // Calculates the width the tabs would occupy if they have enough space.
  int CalculatePreferredWidth();

  // Generates and sets the ideal bounds for the views in |tabs| and
  // |group_headers|. Updates the cached widths in |active_tab_width_| and
  // |inactive_tab_width_|. Returns the total width occupied by the new ideal
  // bounds.
  int UpdateIdealBounds(int available_width);

 private:
  struct TabSlot;

  // Calculates the bounds each tab should occupy, subject to the provided
  // width constraint.
  std::vector<gfx::Rect> CalculateIdealBounds(
      std::optional<int> available_width);

  // Given |model_index| for a tab already present in |slots_|, return
  // the corresponding index in |slots_|.
  int GetSlotIndexForExistingTab(int model_index) const;

  // For a new tab at |new_model_index|, get the insertion index in
  // |slots_|. |group| is the new tab's group.
  int GetSlotInsertionIndexForNewTab(
      int new_model_index,
      std::optional<tab_groups::TabGroupId> group) const;

  // Returns the slot corresponding to the first tab in a group
  // in the view.
  std::optional<int> GetFirstTabSlotForGroup(
      tab_groups::TabGroupId group) const;

  // Used internally in the above two functions. For a tabstrip with N
  // tabs, this takes 0 <= |model_index| <= N and returns the first
  // possible slot corresponding to this model index.
  //
  // This means that if |model_index| is the first tab in a group, the
  // returned slot index will point to the group header. For other tabs,
  // the slot index corresponding to that tab will be returned. Finally,
  // if |model_index| = N, slots_.size() will be returned.
  int GetFirstSlotIndexForTabModelIndex(int model_index) const;

  // Given a group ID, returns the index of its header's corresponding TabSlot
  // in |slots_|.
  int GetSlotIndexForGroupHeader(tab_groups::TabGroupId group) const;

  // Updates the value of either |active_tab_width_| or |inactive_tab_width_|,
  // as appropriate.
  void UpdateCachedTabWidth(int tab_index, int tab_width, bool active);

  // True iff the slot at index |i| is a tab that is in a collapsed group.
  bool SlotIsCollapsedTab(int i) const;

  // The owning TabContainer's controller.
  const raw_ref<const TabContainerController, DanglingUntriaged> controller_;

  // Callback to get the necessary View objects from the owning tabstrip.
  GetTabsCallback get_tabs_callback_;

  // Current collation of tabs and group headers, along with necessary data to
  // run layout and animations for those Views.
  std::vector<TabSlot> slots_;

  // Contains the ideal bounds of tab group headers.
  std::map<tab_groups::TabGroupId, gfx::Rect> group_header_ideal_bounds_;

  // The current widths of tabs. If the space for tabs is not evenly divisible
  // into these widths, the initial tabs in the strip will be 1 px larger.
  int active_tab_width_;
  int inactive_tab_width_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_LAYOUT_HELPER_H_
