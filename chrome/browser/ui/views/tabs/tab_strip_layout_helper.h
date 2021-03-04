// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_LAYOUT_HELPER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_LAYOUT_HELPER_H_

#include <vector>

#include "base/callback.h"
#include "base/callback_forward.h"
#include "base/optional.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/tabs/tab_types.h"
#include "chrome/browser/ui/views/tabs/tab_animation_state.h"
#include "chrome/browser/ui/views/tabs/tab_slot_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip_layout.h"
#include "chrome/browser/ui/views/tabs/tab_width_constraints.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/view_model.h"

class Tab;
class TabGroupHeader;
class TabStripController;

namespace tab_groups {
class TabGroupId;
}

// Helper class for TabStrip, that is responsible for calculating and assigning
// layouts for tabs and group headers. It tracks animations and changes to the
// model so that it has all necessary information for layout purposes.
class TabStripLayoutHelper {
 public:
  using GetTabsCallback = base::RepeatingCallback<views::ViewModelT<Tab>*()>;

  TabStripLayoutHelper(const TabStripController* controller,
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
  int first_non_pinned_tab_index() { return first_non_pinned_tab_index_; }
  int first_non_pinned_tab_x() { return first_non_pinned_tab_x_; }

  // Returns the number of pinned tabs in the tabstrip.
  int GetPinnedTabCount() const;

  // Returns a map of all tab groups and their bounds.
  const std::map<tab_groups::TabGroupId, gfx::Rect>& group_header_ideal_bounds()
      const {
    return group_header_ideal_bounds_;
  }

  // Inserts a new tab at |index|.
  void InsertTabAt(int model_index, Tab* tab, TabPinned pinned);

  // Marks the tab at |model_index| as closing, but does not remove it from
  // |slots_|.
  void RemoveTabAt(int model_index, Tab* tab);

  // Called when the tabstrip enters tab closing mode, wherein tabs should
  // resize differently to control which tab ends up under the cursor.
  // Assumes that the available width will never be smaller than this value
  // for the duration of this tab closing session, i.e. that resizing the
  // tabstrip will only happen after ExitTabClosingMode().
  void EnterTabClosingMode(int available_width);

  // Called when the tabstrip has left tab closing mode or when falling back
  // to the old animation system while in closing mode. Returns the current
  // available width.
  base::Optional<int> ExitTabClosingMode();

  // Invoked when |tab| has been destroyed by TabStrip (i.e. the remove
  // animation has completed).
  void OnTabDestroyed(Tab* tab);

  // Moves the tab at |prev_index| with group |moving_tab_group| to |new_index|.
  // Also updates the group header's location if necessary.
  void MoveTab(base::Optional<tab_groups::TabGroupId> moving_tab_group,
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
  void SetActiveTab(int prev_active_index, int new_active_index);

  // Calculates the smallest width the tabs can occupy.
  int CalculateMinimumWidth();

  // Calculates the width the tabs would occupy if they have enough space.
  int CalculatePreferredWidth();

  // Generates and sets the ideal bounds for the views in |tabs| and
  // |group_headers|. Updates the cached widths in |active_tab_width_| and
  // |inactive_tab_width_|. Returns the total width occupied by the new ideal
  // bounds.
  int UpdateIdealBounds(int available_width);

  // Generates and sets the ideal bounds for |tabs|. Updates
  // the cached values in |first_non_pinned_tab_index_| and
  // |first_non_pinned_tab_x_|.
  void UpdateIdealBoundsForPinnedTabs();

 private:
  struct TabSlot;

  // Calculates the bounds each tab should occupy, subject to the provided
  // width constraint.
  std::vector<gfx::Rect> CalculateIdealBounds(
      base::Optional<int> available_width);

  // Given |model_index| for a tab already present in |slots_|, return
  // the corresponding index in |slots_|.
  int GetSlotIndexForExistingTab(int model_index) const;

  // For a new tab at |new_model_index|, get the insertion index in
  // |slots_|. |group| is the new tab's group.
  int GetSlotInsertionIndexForNewTab(
      int new_model_index,
      base::Optional<tab_groups::TabGroupId> group) const;

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

  // Returns the current width constraints for each View.
  std::vector<TabWidthConstraints> GetCurrentTabWidthConstraints() const;

  // Compares |cached_slots_| to the TabAnimations in |animator_| and DCHECKs if
  // the TabAnimation::ViewType do not match. Prevents bugs that could cause the
  // wrong callback being run when a tab or group is deleted.
  void VerifyAnimationsMatchTabSlots() const;

  // Updates the value of either |active_tab_width_| or |inactive_tab_width_|,
  // as appropriate.
  void UpdateCachedTabWidth(int tab_index, int tab_width, bool active);

  // The tabstrip may enter 'closing mode' when tabs are closed with the mouse.
  // In closing mode, the ideal widths of tabs are manipulated to control which
  // tab ends up under the cursor after each remove animation completes - the
  // next tab to the right, if it exists, or the next tab to the left otherwise.
  // Returns true if any width constraint is currently being enforced.
  bool WidthsConstrainedForClosingMode();

  // True iff the slot at index |i| is a tab that is in a collapsed group.
  bool SlotIsCollapsedTab(int i) const;

  // The owning tabstrip's controller.
  const TabStripController* const controller_;

  // Callback to get the necessary View objects from the owning tabstrip.
  GetTabsCallback get_tabs_callback_;

  // Current collation of tabs and group headers, along with necessary data to
  // run layout and animations for those Views.
  std::vector<TabSlot> slots_;

  // Contains the ideal bounds of tab group headers.
  std::map<tab_groups::TabGroupId, gfx::Rect> group_header_ideal_bounds_;

  // When in tab closing mode, if we want the next tab to the right to end up
  // under the cursor, each tab needs to stay the same size. When defined,
  // this specifies that size.
  base::Optional<TabWidthOverride> tab_width_override_;

  // When in tab closing mode, if we want the next tab to the left to end up
  // under the cursor, the overall space taken by tabs needs to stay the same.
  // When defined, this specifies that size.
  base::Optional<int> tabstrip_width_override_;

  // The current widths of tabs. If the space for tabs is not evenly divisible
  // into these widths, the initial tabs in the strip will be 1 px larger.
  int active_tab_width_;
  int inactive_tab_width_;

  int first_non_pinned_tab_index_;
  int first_non_pinned_tab_x_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_LAYOUT_HELPER_H_
