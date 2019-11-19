// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_width_constraints.h"

#include "chrome/browser/ui/views/tabs/tab_strip_layout.h"
#include "ui/gfx/animation/tween.h"

TabWidthConstraints::TabWidthConstraints(
    const TabAnimationState& state,
    const TabLayoutConstants& layout_constants,
    const TabSizeInfo& size_info)
    : state_(state),
      layout_constants_(layout_constants),
      size_info_(size_info) {}

float TabWidthConstraints::GetMinimumWidth() const {
  const float min_width = gfx::Tween::FloatValueBetween(
      state_.activeness(), size_info_.min_inactive_width,
      size_info_.min_active_width);
  return TransformForPinnednessAndOpenness(min_width);
}

float TabWidthConstraints::GetLayoutCrossoverWidth() const {
  return TransformForPinnednessAndOpenness(size_info_.min_active_width);
}

float TabWidthConstraints::GetPreferredWidth() const {
  return TransformForPinnednessAndOpenness(size_info_.standard_width);
}

float TabWidthConstraints::TransformForPinnednessAndOpenness(
    float width) const {
  const float pinned_width = gfx::Tween::FloatValueBetween(
      state_.pinnedness(), width, size_info_.pinned_tab_width);
  return gfx::Tween::FloatValueBetween(
      state_.openness(), layout_constants_.tab_overlap, pinned_width);
}
