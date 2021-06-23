// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_STACKED_TAB_STRIP_LAYOUT_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_STACKED_TAB_STRIP_LAYOUT_H_

#include <algorithm>

#include "ui/gfx/geometry/size.h"
#include "ui/views/view_model.h"

class Tab;

// StackedTabStripLayout is used by TabStrip in touch
// mode. StackedTabStripLayout is responsible for managing the bounds of the
// tabs. StackedTabStripLayout differs from the normal layout in that it stacks
// tabs. Stacked tabs are tabs placed nearly on top of each other, and if enough
// consecutive stacked tabs exist they are placed on top of each other. Normally
// stacked tabs are placed after pinned tabs, or at the end of the tabstrip, but
// during dragging tabs may be stacked before or after the active tab.
class StackedTabStripLayout {
 public:
  static const int kAddTypePinned   = 1 << 0;
  static const int kAddTypeActive = 1 << 1;

  // |size| is the size for tabs, |overlap| the overlap between consecutive
  // tabs, |stacked_padding| the padding between stacked tabs,
  // |max_stacked_count| the maximum number of consecutive tabs that can be
  // stacked before they are placed on top of each other, |view_model| is the
  // ViewModel the bounds of the tabs are placed in.
  StackedTabStripLayout(const gfx::Size& size,
                        int overlap,
                        int stacked_padding,
                        int max_stacked_count,
                        views::ViewModelBase* view_model);
  StackedTabStripLayout(const StackedTabStripLayout&) = delete;
  StackedTabStripLayout& operator=(const StackedTabStripLayout&) = delete;
  ~StackedTabStripLayout();

  // Sets the x-coordinate the normal tabs start at as well as the pinned tab
  // count. This is only useful if the pinned tab count or x-coordinate change.
  void SetXAndPinnedCount(int x, int pinned_tab_count);

  // Sets the width available for sizing the tabs to.
  void SetWidth(int width);

  // Sets the index of the active tab.
  void SetActiveIndex(int index);

  // Drags the active tab.
  void DragActiveTab(int delta);

  // Makes sure the tabs fill the available width. Used after a drag operation
  // completes.
  void SizeToFit();

  // Adds a new tab at the specified index. |add_types| is a bitmask of
  // kAddType*. |start_x| is the new x-coordinate non-pinned tabs start at.
  void AddTab(int index, int add_types, int start_x);

  // Removes the tab at the specified index. |start_x| is the new x-coordinate
  // normal tabs start at, and |old_x| the old x-coordinate of the tab.
  // ViewModel should already have been updated before calling this.
  void RemoveTab(int index, int start_x, int old_x);

  // Moves the tab from |from| to |to|. |new_active_index| is the index of the
  // currently active tab.
  void MoveTab(int from,
               int to,
               int new_active_index,
               int start_x,
               int pinned_tab_count);

  // Returns the active index as used by this class. The active index dictates
  // stacking and what tabs are visible. As pinned tabs are never stacked,
  // StackedTabStripLayout forces the active index to be in the normal tabs.
  int active_index() const {
    return active_index_ < pinned_tab_count_ ? pinned_tab_count_
                                             : active_index_;
  }

  int pinned_tab_count() const { return pinned_tab_count_; }

  // Returns true if the tab at index is stacked.
  bool IsStacked(int index) const;

  // Sets the X coordinate of the active tab as close to |x| as possible.
  void SetActiveTabLocation(int x);

#if !defined(NDEBUG)
  std::string BoundsString() const;
#endif

 private:
  friend class StackedTabStripLayoutTest;

  // Sets the x-coordinate normal tabs start at, width, pinned tab count, and
  // active index.
  void Reset(int x, int width, int pinned_tab_count, int active_index);

  // Resets to an ideal layout state.
  void ResetToIdealState();

  // Makes all tabs between the active tab and |index| (inclusive) visible. If
  // this is not possible, then |index| is made visible and a stack forms
  // immediately to the right of the active tab. This is used when a new tab is
  // added that isn't active.
  void MakeVisible(int index);

  // Returns the x-coordinate for the active tab constrained by the current tab
  // count.
  int ConstrainActiveX(int x) const;

  // Reset the bounds of the active tab (based on ConstrainActiveX()) and resets
  // the bounds of the remaining tabs by way of LayoutUsingCurrent*.
  void SetActiveBoundsAndLayoutFromActiveTab();

  // Sets the bounds of the tabs after |index| relative to the position of the
  // tab at |index|. Each tab is placed tab_offset() pixels after the previous
  // tab, stacking as necessary.
  void LayoutByTabOffsetAfter(int index);

  // Same as LayoutByTabOffsetAfter(), but iterates toward
  // |pinned_tab_count_|.
  void LayoutByTabOffsetBefore(int index);

  // Similar to LayoutByTabOffsetAfter(), but uses the current x-coordinate
  // if possible.
  void LayoutUsingCurrentAfter(int index);
  void LayoutUsingCurrentBefore(int index);

  void PushTabsAfter(int index, int delta);
  void PushTabsBefore(int index, int delta);

  // Does a layout for drag. Similar to LayoutUsingCurrentXXX() but does not
  // contrain. Used when dragging the active tab.
  void LayoutForDragAfter(int index);
  void LayoutForDragBefore(int index);

  // Used when the tabs are stacked at one side. The remaining tabs are stacked
  // against the |active_index()|. |delta| is the amount of space to resize the
  // the tabs by.
  void ExpandTabsBefore(int index, int delta);
  void ExpandTabsAfter(int index, int delta);

  // Adjusts the stacked tabs so that if there are more than
  // |max_stacked_count_| tabs, the set > max_stacked_count_ have an
  // x-coordinate of |x_|. Similarly those at the end have the same x-coordinate
  // and are pushed all the way to the right.
  void AdjustStackedTabs();
  void AdjustLeadingStackedTabs();
  void AdjustTrailingStackedTabs();

  // Sets the bounds of the tab at |index|.
  void SetIdealBoundsAt(int index, int x);

  // Returns the min x-coordinate for the sepcified index. This is calculated
  // assuming all the tabs before |index| are stacked.
  int GetMinX(int index) const;

  // Returns the max x-coordinate for the speficifed index. This is calculated
  // assuming all the tabs after |index| are stacked.
  int GetMaxX(int index) const;

  // Used when dragging to get the min/max coodinate.
  int GetMinDragX(int index) const;
  int GetMaxDragX(int index) const;

  // Returns the min x-coordinate for the tab at |index|. This is relative
  // to the |active_index()| and is only useful when the active tab is pushed
  // against the left side.
  int GetMinXCompressed(int index) const;

  // Width needed to display |count| tabs.
  int width_for_count(int count) const {
    return (count * size_.width()) - (std::max(count - 1, 0) * overlap_);
  }

  // Padding needed for |count| stacked tabs.
  int stacked_padding_for_count(int count) const {
    return std::min(count, max_stacked_count_) * stacked_padding_;
  }

  // Max stacked padding.
  int max_stacked_width() const {
    return stacked_padding_ * max_stacked_count_;
  }

  int ideal_x(int index) const { return view_model_->ideal_bounds(index).x(); }

  // Returns true if some of the tabs need to be stacked.
  bool requires_stacking() const {
    return tab_count() != pinned_tab_count_ &&
        x_ + width_for_count(tab_count() - pinned_tab_count_) > width_;
  }

  // Number of tabs.
  int tab_count() const { return view_model_->view_size(); }

  // Number of normal (non-pinned) tabs.
  int normal_tab_count() const { return tab_count() - pinned_tab_count_; }

  // Distance from the start of one tab to the next.
  int tab_offset() const { return size_.width() - overlap_; }

  // Size of tabs.
  const gfx::Size size_;

  // Overlap between tabs.
  const int overlap_;

  // Padding between stacked tabs.
  const int stacked_padding_;

  // Max number of stacked tabs.
  const int max_stacked_count_;

  // Where bounds are placed. This is owned by TabStrip.
  // (Note: This is a ViewModelBase, not a ViewModelT<Tab>, because the tests do
  // not populate the model with Tab views.)
  views::ViewModelBase* view_model_;

  // X coordinate of the first tab.
  int first_tab_x_ = 0;

  // X coordinate normal tabs start at.  If there are no pinned tabs, this is
  // equal to |first_tab_x_|.
  int x_ = 0;

  // Available width.
  int width_ = 0;

  // Number of pinned tabs.
  int pinned_tab_count_ = 0;

  // Distance from the last pinned tab to the first non-pinned tab.
  int pinned_tab_to_non_pinned_tab_ = 0;

  // Index of the active tab.
  int active_index_ = -1;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_STACKED_TAB_STRIP_LAYOUT_H_
