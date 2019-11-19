// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_LAYOUT_HELPER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_LAYOUT_HELPER_H_

#include <map>
#include <vector>

#include "base/callback.h"
#include "base/callback_forward.h"
#include "base/optional.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/tabs/tab_types.h"
#include "chrome/browser/ui/views/tabs/tab_animation_state.h"
#include "chrome/browser/ui/views/tabs/tab_strip_layout.h"
#include "chrome/browser/ui/views/tabs/tab_width_constraints.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/view_model.h"

class Tab;
class TabGroupHeader;
class TabGroupId;
class TabStripController;

// Helper class for TabStrip, that is responsible for calculating and assigning
// layouts for tabs and group headers. It tracks animations and changes to the
// model so that it has all necessary information for layout purposes.
class TabStripLayoutHelper {
 public:
  using GetTabsCallback = base::RepeatingCallback<views::ViewModelT<Tab>*()>;
  using GetGroupHeadersCallback =
      base::RepeatingCallback<std::map<TabGroupId, TabGroupHeader*>()>;

  TabStripLayoutHelper(const TabStripController* controller,
                       GetTabsCallback get_tabs_callback,
                       GetGroupHeadersCallback get_group_headers_callback,
                       base::RepeatingClosure on_animation_progressed);
  ~TabStripLayoutHelper();

  // Returns a vector of all tabs in the strip, including both closing tabs
  // and tabs still in the model.
  std::vector<Tab*> GetTabs();

  // Returns whether any animations for tabs or group headers are in progress.
  bool IsAnimating() const;

  int active_tab_width() { return active_tab_width_; }
  int inactive_tab_width() { return inactive_tab_width_; }
  int first_non_pinned_tab_index() { return first_non_pinned_tab_index_; }
  int first_non_pinned_tab_x() { return first_non_pinned_tab_x_; }

  // Returns the number of pinned tabs in the tabstrip.
  int GetPinnedTabCount() const;

  // Inserts a new tab at |index|, without animation. |tab_removed_callback|
  // will be invoked if the tab is removed at the end of a remove animation.
  void InsertTabAtNoAnimation(int model_index,
                              Tab* tab,
                              base::OnceClosure tab_removed_callback,
                              TabPinned pinned);

  // Inserts a new tab at |index|, with animation. |tab_removed_callback| will
  // be invoked if the tab is removed at the end of a remove animation.
  void InsertTabAt(int model_index,
                   Tab* tab,
                   base::OnceClosure tab_removed_callback,
                   TabPinned pinned);

  // Marks the tab at |model_index| as closed without animating it. Use when
  // the tab has been removed from the model but the old animation style owns
  // animating it.
  // TODO(958173): Remove this when the old animation style is removed.
  void RemoveTabNoAnimation(int model_index, Tab* tab);

  // Marks the tab at |model_index| as closing and animates it closed.
  void RemoveTab(int model_index, Tab* tab);

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
  void MoveTab(base::Optional<TabGroupId> moving_tab_group,
               int prev_index,
               int new_index);

  // Sets the tab at |index|'s pinned state to |pinned|.
  void SetTabPinned(int model_index, TabPinned pinned);

  // Inserts a new group header for |group|. |header_removed_callback| will be
  // invoked if the group is removed at the end of a remove animation.
  void InsertGroupHeader(TabGroupId group,
                         TabGroupHeader* header,
                         base::OnceClosure header_removed_callback);

  // Removes the group header for |group|.
  void RemoveGroupHeader(TabGroupId group);

  // Ensures the group header for |group| is at the correct index. Should be
  // called externally when group membership changes but nothing else about the
  // layout does.
  void UpdateGroupHeaderIndex(TabGroupId group);

  // Changes the active tab from |prev_active_index| to |new_active_index|.
  void SetActiveTab(int prev_active_index, int new_active_index);

  // Finishes all in-progress animations.
  void CompleteAnimations();

  // TODO(958173): Temporary method that completes running animations
  // without invoking the callback to destroy removed tabs. Use to hand
  // off animation (and removed tab destruction) responsibilities from
  // this animator to elsewhere without teleporting tabs or destroying
  // the same tab more than once.
  void CompleteAnimationsWithoutDestroyingTabs();

  // Generates and sets the ideal bounds for the views in |tabs| and
  // |group_headers|. Updates the cached widths in |active_tab_width_| and
  // |inactive_tab_width_|.
  // TODO(958173): The notion of ideal bounds is going away. Delete this.
  void UpdateIdealBounds(int available_width);

  // Generates and sets the ideal bounds for |tabs|. Updates
  // the cached values in |first_non_pinned_tab_index_| and
  // |first_non_pinned_tab_x_|.
  // TODO(958173): The notion of ideal bounds is going away. Delete this.
  void UpdateIdealBoundsForPinnedTabs();

  // Lays out tabs and group headers to their current bounds. Returns the
  // x-coordinate of the trailing edge of the trailing-most tab.
  int LayoutTabs(base::Optional<int> available_width);

 private:
  struct TabSlot;

  // Given a tab's |model_index| and |group|, returns the index of its
  // corresponding TabSlot in |slots_|.
  int GetSlotIndexForTabModelIndex(int model_index,
                                   base::Optional<TabGroupId> group) const;

  // Given a group ID, returns the index of its header's corresponding TabSlot
  // in |slots_|.
  int GetSlotIndexForGroupHeader(TabGroupId group) const;

  // Returns the current width constraints for each View.
  std::vector<TabWidthConstraints> GetCurrentTabWidthConstraints() const;

  // Runs an animation for the View at |slot_index| towards |target_state|.
  void AnimateSlot(int slot_index, TabAnimationState target_state);

  // Called when animations progress.
  void TickAnimations();

  // Deletes the data in |slots_| corresponding to fully closed tabs.
  void RemoveClosedTabs();

  // Recalculate |cached_slots_|, called whenever state changes.
  void UpdateCachedTabSlots();

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

  // The owning tabstrip's controller.
  const TabStripController* const controller_;

  // Callbacks to get the necessary View objects from the owning tabstrip.
  GetTabsCallback get_tabs_callback_;
  GetGroupHeadersCallback get_group_headers_callback_;

  // Timer used to run animations on Views..
  base::RepeatingTimer animation_timer_;

  // Called when animations progress.
  base::RepeatingClosure on_animation_progressed_;

  // Current collation of tabs and group headers, along with necessary data to
  // run layout and animations for those Views.
  std::vector<TabSlot> slots_;

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

  DISALLOW_COPY_AND_ASSIGN(TabStripLayoutHelper);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_LAYOUT_HELPER_H_
