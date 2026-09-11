// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_COMMON_TAB_GROUP_STYLE_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_COMMON_TAB_GROUP_STYLE_H_

#include <optional>

#include "chrome/browser/ui/views/tabs/shared/tab_strip_types.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"

// Default styling of tab groups.
class TabGroupStyle {
 public:
  TabGroupStyle() = delete;

  static int GetTabGroupOverlapAdjustment();
  static int GetChipCornerRadius(
      TabStripOrientation orientation = TabStripOrientation::kHorizontal);
  static int GetEmptyChipSize();
  static gfx::Point GetTitleChipOffset(
      std::optional<int> text_height = std::nullopt);
  static gfx::Insets GetInsetsForHeaderChip(
      TabStripOrientation orientation = TabStripOrientation::kHorizontal);
  // Returns the horizontal padding between adjacent tab group headers when
  // collapsed.
  static int GetPaddingBetweenCollapsedHeaders();
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_COMMON_TAB_GROUP_STYLE_H_
