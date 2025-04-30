// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_width_constraints.h"

#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/tabs/tab_types.h"
#include "chrome/browser/ui/views/tabs/tab_strip_layout.h"
#include "ui/gfx/animation/tween.h"

namespace {
// We try to render split tabs such that the whole split is the same size as a
// regular tab but as tabs shrink, we end up with very condensed split tabs
// while other tabs still have lots of padding. This constant was decided in
// collaboration with UX to make the layout of split tabs look smooth as more
// tabs are added to the tab strip. In practice, a split tab will appear the
// same size as a regular tab until half of the split hits this constant at
// which point a split tab will appear larger than a regular tab.
constexpr int kSplitTabLayoutCrossoverWidth = 53;
}  // namespace

TabWidthConstraints::TabWidthConstraints(const TabLayoutState& state,
                                         const TabSizeInfo& size_info)
    : state_(state), size_info_(size_info) {}

float TabWidthConstraints::GetMinimumWidth(bool compensate_for_splits) const {
  const bool use_active_width =
      state_.active() == TabActive::kActive ||
      (compensate_for_splits && state_.split().has_value());
  const float min_width = use_active_width ? size_info_.min_active_width
                                           : size_info_.min_inactive_width;
  return TransformForPinnednessAndOpenness(min_width);
}

float TabWidthConstraints::GetLayoutCrossoverWidth(
    bool compensate_for_splits) const {
  return TransformForPinnednessAndOpenness(compensate_for_splits &&
                                                   state_.split().has_value()
                                               ? kSplitTabLayoutCrossoverWidth
                                               : size_info_.min_active_width);
}

float TabWidthConstraints::GetPreferredWidth() const {
  return TransformForPinnednessAndOpenness(size_info_.standard_width);
}

float TabWidthConstraints::TransformForPinnednessAndOpenness(
    float width) const {
  if (state_.IsClosed()) {
    return TabStyle::Get()->GetTabOverlap();
  } else if (state_.pinned() == TabPinned::kPinned) {
    return size_info_.pinned_tab_width;
  } else {
    return width;
  }
}
