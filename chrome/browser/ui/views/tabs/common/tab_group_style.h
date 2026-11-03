// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_COMMON_TAB_GROUP_STYLE_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_COMMON_TAB_GROUP_STYLE_H_

#include <optional>

#include "chrome/browser/ui/views/tabs/shared/tab_strip_types.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rounded_corners_f.h"

// Default styling of tab groups.
class TabGroupStyle {
 public:
  TabGroupStyle() = delete;

  static int GetTabGroupOverlapAdjustment();
  static int GetChipCornerRadius(
      TabStripOrientation orientation = TabStripOrientation::kHorizontal);
  static int GetFocusedChipCornerRadius(
      TabStripOrientation orientation = TabStripOrientation::kHorizontal);

  // Corner radii for the group header and the Unfocus button, which sit
  // against each other when a group is focused (the header follows the Unfocus
  // button: below it in a vertical strip, after it in a horizontal one). The
  // corners where they meet flatten to a small radius when focused and
  // interpolate according to `animation_progress` (0.0 unfocused to 1.0
  // focused) during a focus transition.
  static gfx::RoundedCornersF GetHeaderCornerRadii(
      TabStripOrientation orientation,
      bool in_focused_state,
      std::optional<float> animation_progress = std::nullopt);
  static gfx::RoundedCornersF GetUnfocusButtonCornerRadii(
      TabStripOrientation orientation,
      std::optional<float> animation_progress = std::nullopt);

  static int GetEmptyChipSize();
  static int GetFocusedChipHeight();
  static gfx::Point GetTitleChipOffset(
      std::optional<int> chip_height = std::nullopt);
  static gfx::Insets GetInsetsForHeaderChip(
      TabStripOrientation orientation = TabStripOrientation::kHorizontal);
  static gfx::Insets GetInsetsForFocusedHeaderChip();
  static gfx::Insets GetInsetsForFocusedUnfocusChip();
  // Returns the horizontal padding between adjacent tab group headers when
  // collapsed.
  static int GetPaddingBetweenCollapsedHeaders();
  // Returns the leading padding for a tab group header when it is the first
  // item in the tab strip.
  static int GetLeadingGroupHeaderPadding();
  // Returns the trailing padding for a collapsed tab group header when it is
  // the last item in the tab strip.
  static int GetTrailingGroupHeaderPadding();
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_COMMON_TAB_GROUP_STYLE_H_
