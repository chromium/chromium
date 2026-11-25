// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_COMMON_TAB_GROUP_STYLE_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_COMMON_TAB_GROUP_STYLE_H_

#include <optional>

#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/views/tabs/shared/tab_strip_types.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rounded_corners_f.h"

class TabStyleViewDelegate;

// Default styling of tab groups.
class TabGroupStyle {
 public:
  TabGroupStyle() = delete;

  // Group header styling.
  // Returns the height of a horizontal group header. An untitled unfocused
  // header is square, so its width equals the unfocused height.
  static int GetHeaderHeight(bool in_focused_state = false);
  static gfx::Point GetHeaderOffset(
      std::optional<int> header_height = std::nullopt);
  static gfx::Insets GetHeaderInsets(
      TabStripOrientation orientation = TabStripOrientation::kHorizontal,
      bool in_focused_state = false);
  // Corner radii for the group header, which sits adjacent to the Unfocus
  // button when a group is focused (below it in a vertical strip, after it in a
  // horizontal one). The corners facing the Unfocus button flatten to a small
  // radius as `focus_progress` interpolates from 0.0 (unfocused) to 1.0
  // (focused).
  static gfx::RoundedCornersF GetHeaderCornerRadii(
      TabStripOrientation orientation,
      float focus_progress = 0.0f);

  // Unfocus button styling.
  // Returns the height of the horizontal Unfocus button.
  static int GetUnfocusButtonHeight();
  static gfx::Insets GetUnfocusButtonInsets(
      TabStripOrientation orientation = TabStripOrientation::kHorizontal);
  // Corner radii for the Unfocus button. The corners facing the group header
  // flatten to a small radius as `focus_progress` interpolates from 0.0
  // (unfocused) to 1.0 (focused).
  static gfx::RoundedCornersF GetUnfocusButtonCornerRadii(
      TabStripOrientation orientation,
      float focus_progress = 1.0f);

  // Tab strip spacing and overlap.
  static int GetOverlapAdjustment();
  // Returns the horizontal padding between adjacent tab group headers when
  // collapsed.
  static int GetPaddingBetweenCollapsedHeaders();
  // Returns the leading padding for a tab group header when it is the first
  // item in the tab strip.
  static int GetLeadingGroupHeaderPadding();
  // Returns the trailing padding for a collapsed tab group header when it is
  // the last item in the tab strip.
  static int GetTrailingGroupHeaderPadding();

  // Group underline.
  // Thickness of the line drawn under (or beside) the tabs in a group.
  static int GetUnderlineStrokeThickness();
  // Returns how far the group underline is inset from the leading/trailing
  // edges of the group in a horizontal tab strip.
  static int GetUnderlineStrokeInset();

  // Separators.
  // Returns the opacities of the separators, in physical order, for a group
  // header. Group headers never paint a leading separator: the preceding slot
  // paints it. A trailing separator is only painted while the group is
  // collapsed, as there is no visible tab in the group that could paint it.
  // A group in focus mode is always shown expanded, so callers should pass
  // `is_collapsed` as false for it. `adjacent_tab` is the tab following the
  // group and is only used when `is_collapsed` is true, so callers may skip
  // looking it up otherwise.
  // These rules mirror HorizontalTabStyleViews::GetSeparatorOpacity(), where
  // tabs paint trailing separators by default; keep both in sync.
  static TabStyle::SeparatorOpacities GetSeparatorOpacities(
      bool is_collapsed,
      bool is_dragging,
      int tab_count,
      const TabStyleViewDelegate* adjacent_tab);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_COMMON_TAB_GROUP_STYLE_H_
