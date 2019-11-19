// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/stacked_tab_strip_layout.h"

#include <stdio.h>

#include "base/logging.h"
#include "base/metrics/user_metrics.h"
#include "base/numerics/ranges.h"
#include "base/strings/string_number_conversions.h"

using base::UserMetricsAction;

StackedTabStripLayout::StackedTabStripLayout(const gfx::Size& size,
                                             int overlap,
                                             int stacked_padding,
                                             int max_stacked_count,
                                             views::ViewModelBase* view_model)
    : size_(size),
      overlap_(overlap),
      stacked_padding_(stacked_padding),
      max_stacked_count_(max_stacked_count),
      view_model_(view_model) {}

StackedTabStripLayout::~StackedTabStripLayout() {
}

void StackedTabStripLayout::SetXAndPinnedCount(int x, int pinned_tab_count) {
  first_tab_x_ = x;
  x_ = x;
  pinned_tab_count_ = pinned_tab_count;
  pinned_tab_to_non_pinned_tab_ = 0;
  if (!requires_stacking() || tab_count() == pinned_tab_count) {
    ResetToIdealState();
    return;
  }
  if (pinned_tab_count > 0) {
    pinned_tab_to_non_pinned_tab_ = x - ideal_x(pinned_tab_count - 1);
    first_tab_x_ = ideal_x(0);
  }
  SetIdealBoundsAt(active_index(), ConstrainActiveX(ideal_x(active_index())));
  LayoutByTabOffsetAfter(active_index());
  LayoutByTabOffsetBefore(active_index());
}

void StackedTabStripLayout::SetWidth(int width) {
  if (width_ == width)
    return;

  width_ = width;
  if (!requires_stacking()) {
    ResetToIdealState();
    return;
  }

  // TODO(tdanderson): Audit other places in this class which make a similar
  // pattern of calls to this in order to re-layout the tabs, and refactor
  // into a helper function as appropriate.
  SetIdealBoundsAt(active_index(), ConstrainActiveX(ideal_x(active_index())));
  LayoutByTabOffsetBefore(active_index());
  LayoutByTabOffsetAfter(active_index());
  AdjustStackedTabs();
}

void StackedTabStripLayout::SetActiveIndex(int index) {
  int old = active_index();
  active_index_ = index;
  if (old == active_index() || !requires_stacking())
    return;
  SetIdealBoundsAt(active_index(), ConstrainActiveX(ideal_x(active_index())));
  LayoutByTabOffsetBefore(active_index());
  LayoutByTabOffsetAfter(active_index());
  AdjustStackedTabs();
}

void StackedTabStripLayout::DragActiveTab(int delta) {
  if (delta == 0 || !requires_stacking())
    return;

  base::RecordAction(UserMetricsAction("StackedTab_DragActiveTab"));
  int initial_x = ideal_x(active_index());
  // If we're at a particular edge and start dragging, expose all the tabs after
  // the tab (or before when dragging to the left).
  if (delta > 0 && initial_x == GetMinX(active_index())) {
    LayoutByTabOffsetAfter(active_index());
    AdjustStackedTabs();
  } else if (delta < 0 && initial_x == GetMaxX(active_index())) {
    LayoutByTabOffsetBefore(active_index());
    AdjustStackedTabs();
  }
  int x = delta > 0 ?
      std::min(initial_x + delta, GetMaxDragX(active_index())) :
      std::max(initial_x + delta, GetMinDragX(active_index()));
  if (x != initial_x) {
    SetIdealBoundsAt(active_index(), x);
    if (delta > 0) {
      PushTabsAfter(active_index(), (x - initial_x));
      LayoutForDragBefore(active_index());
    } else {
      PushTabsBefore(active_index(), initial_x - x);
      LayoutForDragAfter(active_index());
    }
    delta -= (x - initial_x);
  }
  if (delta > 0)
    ExpandTabsBefore(active_index(), delta);
  else if (delta < 0)
    ExpandTabsAfter(active_index(), -delta);
  AdjustStackedTabs();
}

void StackedTabStripLayout::SizeToFit() {
  if (!tab_count())
    return;

  if (!requires_stacking()) {
    ResetToIdealState();
    return;
  }

  if (ideal_x(0) != first_tab_x_) {
    // Tabs have been dragged to the right. Pull in the tabs from left to right
    // to fill in space.
    int delta = ideal_x(0) - first_tab_x_;
    int i = 0;
    for (; i < pinned_tab_count_; ++i) {
      gfx::Rect pinned_bounds(view_model_->ideal_bounds(i));
      pinned_bounds.set_x(ideal_x(i) - delta);
      view_model_->set_ideal_bounds(i, pinned_bounds);
    }
    for (; delta > 0 && i < tab_count() - 1; ++i) {
      const int exposed = tab_offset() - (ideal_x(i + 1) - ideal_x(i));
      SetIdealBoundsAt(i, ideal_x(i) - delta);
      delta -= exposed;
    }
    AdjustStackedTabs();
    return;
  }

  const int max_x = width_ - size_.width();
  if (ideal_x(tab_count() - 1) == max_x)
    return;

  // Tabs have been dragged to the left. Pull in tabs from right to left to fill
  // in space.
  SetIdealBoundsAt(tab_count() - 1, max_x);
  for (int i = tab_count() - 2; i > pinned_tab_count_ &&
           ideal_x(i + 1) - ideal_x(i) > tab_offset(); --i) {
    SetIdealBoundsAt(i, ideal_x(i + 1) - tab_offset());
  }
  AdjustStackedTabs();
}

void StackedTabStripLayout::AddTab(int index, int add_types, int start_x) {
  if (add_types & kAddTypeActive)
    active_index_ = index;
  else if (active_index_ >= index)
    active_index_++;
  if (add_types & kAddTypePinned)
    pinned_tab_count_++;
  x_ = start_x;
  if (!requires_stacking() || normal_tab_count() <= 1) {
    ResetToIdealState();
    return;
  }

  int active_x = ideal_x(active_index());
  if (add_types & kAddTypeActive) {
    active_x = (index + 1 == tab_count()) ?
      width_ - size_.width() : ideal_x(index + 1);
  }

  SetIdealBoundsAt(active_index(), ConstrainActiveX(active_x));
  LayoutByTabOffsetAfter(active_index());
  LayoutByTabOffsetBefore(active_index());
  AdjustStackedTabs();

  if ((add_types & kAddTypeActive) == 0)
    MakeVisible(index);
}

void StackedTabStripLayout::RemoveTab(int index, int start_x, int old_x) {
  if (index == active_index_)
    active_index_ = std::min(active_index_, tab_count() - 1);
  else if (index < active_index_)
    active_index_--;
  bool removed_pinned_tab = index < pinned_tab_count_;
  if (removed_pinned_tab) {
    pinned_tab_count_--;
    DCHECK_GE(pinned_tab_count_, 0);
  }
  int delta = start_x - x_;
  x_ = start_x;
  if (!requires_stacking()) {
    ResetToIdealState();
    return;
  }
  if (removed_pinned_tab) {
    for (int i = pinned_tab_count_; i < tab_count(); ++i)
      SetIdealBoundsAt(i, ideal_x(i) + delta);
  }

  // TODO(tdanderson): Investigate whether the call to
  // SetActiveBoundsAndLayoutFromActiveTab() should be replaced by
  // LayoutByTabOffsetBefore() / LayoutByTabOffsetAfter() similar to the
  // behavior in other stacked tab operations.
  SetActiveBoundsAndLayoutFromActiveTab();
  AdjustStackedTabs();
}

void StackedTabStripLayout::MoveTab(int from,
                                    int to,
                                    int new_active_index,
                                    int start_x,
                                    int pinned_tab_count) {
  x_ = start_x;
  pinned_tab_count_ = pinned_tab_count;
  active_index_ = new_active_index;
  if (!requires_stacking() || tab_count() == pinned_tab_count_) {
    ResetToIdealState();
  } else {
    SetIdealBoundsAt(active_index(),
                     ConstrainActiveX(ideal_x(active_index())));
    LayoutByTabOffsetAfter(active_index());
    LayoutByTabOffsetBefore(active_index());
    AdjustStackedTabs();
  }
  pinned_tab_to_non_pinned_tab_ = pinned_tab_count > 0 ?
      start_x - ideal_x(pinned_tab_count - 1) : 0;
  first_tab_x_ = pinned_tab_count > 0 ? ideal_x(0) : start_x;
}

bool StackedTabStripLayout::IsStacked(int index) const {
  if (index == active_index() || tab_count() == pinned_tab_count_ ||
      index < pinned_tab_count_)
    return false;
  if (index > active_index())
    return ideal_x(index) != ideal_x(index - 1) + tab_offset();
  return ideal_x(index + 1) != ideal_x(index) + tab_offset();
}

void StackedTabStripLayout::SetActiveTabLocation(int x) {
  if (!requires_stacking())
    return;

  const int index = active_index();
  if (index <= pinned_tab_count_)
    return;

  x = ConstrainActiveX(x);
  if (x == ideal_x(index))
    return;

  SetIdealBoundsAt(index, x);
  LayoutByTabOffsetBefore(index);
  LayoutByTabOffsetAfter(index);
}

#if !defined(NDEBUG)
std::string StackedTabStripLayout::BoundsString() const {
  std::string result;
  for (int i = 0; i < view_model_->view_size(); ++i) {
    if (!result.empty())
      result += " ";
    if (i == active_index())
      result += "[";
    result += base::NumberToString(view_model_->ideal_bounds(i).x());
    if (i == active_index())
      result += "]";
  }
  return result;
}
#endif

void StackedTabStripLayout::Reset(int x,
                                  int width,
                                  int pinned_tab_count,
                                  int active_index) {
  x_ = x;
  width_ = width;
  pinned_tab_count_ = pinned_tab_count;
  pinned_tab_to_non_pinned_tab_ = pinned_tab_count > 0 ?
      x - ideal_x(pinned_tab_count - 1) : 0;
  first_tab_x_ = pinned_tab_count > 0 ? ideal_x(0) : x;
  active_index_ = active_index;
  ResetToIdealState();
}

void StackedTabStripLayout::ResetToIdealState() {
  if (tab_count() == pinned_tab_count_)
    return;

  if (!requires_stacking()) {
    SetIdealBoundsAt(pinned_tab_count_, x_);
    LayoutByTabOffsetAfter(pinned_tab_count_);
    return;
  }

  if (normal_tab_count() == 1) {
    // TODO: might want to shrink the tab here.
    SetIdealBoundsAt(pinned_tab_count_, 0);
    return;
  }

  int available_width = width_ - x_;
  int leading_count = active_index() - pinned_tab_count_;
  int trailing_count = tab_count() - active_index();
  if (width_for_count(leading_count + 1) + max_stacked_width() <
      available_width) {
    SetIdealBoundsAt(pinned_tab_count_, x_);
    LayoutByTabOffsetAfter(pinned_tab_count_);
  } else if (width_for_count(trailing_count) + max_stacked_width() <
             available_width) {
    SetIdealBoundsAt(tab_count() - 1, width_ - size_.width());
    LayoutByTabOffsetBefore(tab_count() - 1);
  } else {
    int index = active_index();
    do {
      int stacked_padding =
          stacked_padding_for_count(index - pinned_tab_count_);
      SetIdealBoundsAt(index, x_ + stacked_padding);
      LayoutByTabOffsetAfter(index);
      LayoutByTabOffsetBefore(index);
      index--;
    } while (index >= pinned_tab_count_ && ideal_x(pinned_tab_count_) != x_ &&
             ideal_x(tab_count() - 1) != width_ - size_.width());
  }
  AdjustStackedTabs();
}

void StackedTabStripLayout::MakeVisible(int index) {
  // Currently no need to support tabs opening before |index| visible.
  if (index <= active_index() || !requires_stacking() || !IsStacked(index))
    return;

  const int ideal_delta = width_for_count(index - active_index()) - overlap_;
  if (ideal_x(index) - ideal_x(active_index()) == ideal_delta)
    return;

  // Move the active tab to the left so that all tabs between the active tab
  // and |index| (inclusive) can be made visible.
  const int active_x =
      base::ClampToRange(ideal_x(index) - ideal_delta, GetMinX(active_index()),
                         ideal_x(active_index()));
  SetIdealBoundsAt(active_index(), active_x);
  LayoutByTabOffsetBefore(active_index());
  LayoutByTabOffsetAfter(active_index());
  AdjustStackedTabs();

  if (ideal_x(index) - ideal_x(active_index()) == ideal_delta)
    return;

  // If we get here active_index() is left aligned. Push |index| as far to
  // the right as possible, forming a stack immediately to the right of the
  // active tab if necessary.
  const int x = std::min(GetMaxX(index), active_x + ideal_delta);
  SetIdealBoundsAt(index, x);
  LayoutByTabOffsetAfter(index);
  for (int next_x = x, i = index - 1; i > active_index(); --i) {
    next_x = std::max(GetMinXCompressed(i), next_x - tab_offset());
    SetIdealBoundsAt(i, next_x);
  }
  LayoutUsingCurrentAfter(active_index());
  AdjustStackedTabs();
}

int StackedTabStripLayout::ConstrainActiveX(int x) const {
  return base::ClampToRange(x, GetMinX(active_index()),
                            GetMaxX(active_index()));
}

void StackedTabStripLayout::SetActiveBoundsAndLayoutFromActiveTab() {
  int x = ConstrainActiveX(ideal_x(active_index()));
  SetIdealBoundsAt(active_index(), x);
  LayoutUsingCurrentBefore(active_index());
  LayoutUsingCurrentAfter(active_index());
  AdjustStackedTabs();
}

void StackedTabStripLayout::LayoutByTabOffsetAfter(int index) {
  for (int i = index + 1; i < tab_count(); ++i) {
    int max_x = width_ - size_.width() -
        stacked_padding_for_count(tab_count() - i - 1);
    int x = std::min(max_x,
                     view_model_->ideal_bounds(i - 1).x() + tab_offset());
    SetIdealBoundsAt(i, x);
  }
}

void StackedTabStripLayout::LayoutByTabOffsetBefore(int index) {
  for (int i = index - 1; i >= pinned_tab_count_; --i) {
    int min_x = x_ + stacked_padding_for_count(i - pinned_tab_count_);
    int x = std::max(min_x, ideal_x(i + 1) - (tab_offset()));
    SetIdealBoundsAt(i, x);
  }
}

void StackedTabStripLayout::LayoutUsingCurrentAfter(int index) {
  for (int i = index + 1; i < tab_count(); ++i) {
    int x = std::min(ideal_x(i), ideal_x(i - 1) + tab_offset());
    int min_x = width_ - width_for_count(tab_count() - i);
    SetIdealBoundsAt(i, base::ClampToRange(x, min_x, GetMaxX(i)));
  }
}

void StackedTabStripLayout::LayoutUsingCurrentBefore(int index) {
  for (int i = index - 1; i >= pinned_tab_count_; --i) {
    int x = std::max(ideal_x(i), ideal_x(i + 1) - tab_offset());
    int max_x = x_ + width_for_count(i - pinned_tab_count_);
    if (i > pinned_tab_count_)
      max_x -= overlap_;
    SetIdealBoundsAt(i,
                     std::min({x, ideal_x(i + 1) - stacked_padding_, max_x}));
  }
}

void StackedTabStripLayout::PushTabsAfter(int index, int delta) {
  for (int i = index + 1; i < tab_count(); ++i)
    SetIdealBoundsAt(i, std::min(ideal_x(i) + delta, GetMaxDragX(i)));
}

void StackedTabStripLayout::PushTabsBefore(int index, int delta) {
  for (int i = index - 1; i > pinned_tab_count_; --i)
    SetIdealBoundsAt(i, std::max(ideal_x(i) - delta, GetMinDragX(i)));
}

void StackedTabStripLayout::LayoutForDragAfter(int index) {
  for (int i = index + 1; i < tab_count(); ++i) {
    const int min_x = ideal_x(i - 1) + stacked_padding_;
    const int max_x = ideal_x(i - 1) + tab_offset();
    SetIdealBoundsAt(i, base::ClampToRange(ideal_x(i), min_x, max_x));
  }
}

void StackedTabStripLayout::LayoutForDragBefore(int index) {
  for (int i = index - 1; i >= pinned_tab_count_; --i) {
    const int max_x = ideal_x(i + 1) - stacked_padding_;
    const int min_x = ideal_x(i + 1) - tab_offset();
    SetIdealBoundsAt(i, base::ClampToRange(ideal_x(i), min_x, max_x));
  }

  if (pinned_tab_count_ == 0)
    return;

  // Pull in the pinned tabs.
  const int delta = (pinned_tab_count_ > 1) ? ideal_x(1) - ideal_x(0) : 0;
  for (int i = pinned_tab_count_ - 1; i >= 0; --i) {
    gfx::Rect pinned_bounds(view_model_->ideal_bounds(i));
    if (i == pinned_tab_count_ - 1)
      pinned_bounds.set_x(ideal_x(i + 1) - pinned_tab_to_non_pinned_tab_);
    else
      pinned_bounds.set_x(ideal_x(i + 1) - delta);
    view_model_->set_ideal_bounds(i, pinned_bounds);
  }
}

void StackedTabStripLayout::ExpandTabsBefore(int index, int delta) {
  for (int i = index - 1; i >= pinned_tab_count_ && delta > 0; --i) {
    const int max_x = ideal_x(active_index()) -
        stacked_padding_for_count(active_index() - i);
    int to_resize = std::min(delta, max_x - ideal_x(i));

    if (to_resize <= 0)
      continue;
    SetIdealBoundsAt(i, ideal_x(i) + to_resize);
    delta -= to_resize;
    LayoutForDragBefore(i);
  }
}

void StackedTabStripLayout::ExpandTabsAfter(int index, int delta) {
  if (index == tab_count() - 1)
    return;  // Nothing to expand.

  for (int i = index + 1; i < tab_count() && delta > 0; ++i) {
    const int min_compressed =
        ideal_x(active_index()) + stacked_padding_for_count(i - active_index());
    const int to_resize = std::min(ideal_x(i) - min_compressed, delta);
    if (to_resize <= 0)
      continue;
    SetIdealBoundsAt(i, ideal_x(i) - to_resize);
    delta -= to_resize;
    LayoutForDragAfter(i);
  }
}

void StackedTabStripLayout::AdjustStackedTabs() {
  if (!requires_stacking() || tab_count() <= pinned_tab_count_ + 1)
    return;

  AdjustLeadingStackedTabs();
  AdjustTrailingStackedTabs();
}

void StackedTabStripLayout::AdjustLeadingStackedTabs() {
  int index = pinned_tab_count_ + 1;
  while (index < active_index() &&
         ideal_x(index) - ideal_x(index - 1) <= stacked_padding_ &&
         ideal_x(index) <= x_ + max_stacked_width()) {
    index++;
  }
  if (ideal_x(index) - ideal_x(index - 1) <= stacked_padding_ &&
      ideal_x(index) <= x_ + max_stacked_width()) {
    index++;
  }
  if (index <= pinned_tab_count_ + max_stacked_count_ - 1)
    return;
  int max_stacked = index;
  int x = x_;
  index = pinned_tab_count_;
  for (; index < max_stacked - max_stacked_count_ - 1; ++index)
    SetIdealBoundsAt(index, x);
  for (; index < max_stacked; ++index, x += stacked_padding_)
    SetIdealBoundsAt(index, x);
}

void StackedTabStripLayout::AdjustTrailingStackedTabs() {
  int index = tab_count() - 1;
  int max_stacked_x = width_ - size_.width() - max_stacked_width();
  while (index > active_index() &&
         ideal_x(index) - ideal_x(index - 1) <= stacked_padding_ &&
         ideal_x(index - 1) >= max_stacked_x) {
    index--;
  }
  if (index > active_index() &&
      ideal_x(index) - ideal_x(index - 1) <= stacked_padding_ &&
      ideal_x(index - 1) >= max_stacked_x) {
    index--;
  }
  if (index >= tab_count() - max_stacked_count_)
    return;
  int first_stacked = index;
  int x = width_ - size_.width() -
      std::min(tab_count() - first_stacked, max_stacked_count_) *
      stacked_padding_;
  for (; index < first_stacked + max_stacked_count_;
       ++index, x += stacked_padding_) {
    SetIdealBoundsAt(index, x);
  }
  for (; index < tab_count(); ++index)
    SetIdealBoundsAt(index, x);
}

void StackedTabStripLayout::SetIdealBoundsAt(int index, int x) {
  view_model_->set_ideal_bounds(index, gfx::Rect(gfx::Point(x, 0), size_));
}

int StackedTabStripLayout::GetMinX(int index) const {
  int leading_count = index - pinned_tab_count_;
  int trailing_count = tab_count() - index;
  return std::max(x_ + stacked_padding_for_count(leading_count),
                  width_ - width_for_count(trailing_count));
}

int StackedTabStripLayout::GetMaxX(int index) const {
  int trailing_offset = stacked_padding_for_count(tab_count() - index - 1);
  int leading_size = width_for_count(index - pinned_tab_count_) + x_;
  if (index > pinned_tab_count_)
    leading_size -= overlap_;
  return std::min(width_ - trailing_offset - size_.width(), leading_size);
}

int StackedTabStripLayout::GetMinDragX(int index) const {
  return x_ + stacked_padding_for_count(index - pinned_tab_count_);
}

int StackedTabStripLayout::GetMaxDragX(int index) const {
  const int trailing_offset =
      stacked_padding_for_count(tab_count() - index - 1);
  return width_ - trailing_offset - size_.width();
}

int StackedTabStripLayout::GetMinXCompressed(int index) const {
  DCHECK_GT(index, active_index());
  return std::max(
      width_ - width_for_count(tab_count() - index),
      ideal_x(active_index()) +
          stacked_padding_for_count(index - active_index()));
}
