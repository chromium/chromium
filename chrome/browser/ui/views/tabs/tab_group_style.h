// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_GROUP_STYLE_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_GROUP_STYLE_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/tabs/shared/tab_strip_types.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/view.h"

class TabGroupHeader;
class TabGroupViews;

// Default styling of tab groups.
class TabGroupStyle {
 public:
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

  explicit TabGroupStyle(const TabGroupViews& tab_group_views);
  TabGroupStyle(const TabGroupStyle&) = delete;
  TabGroupStyle& operator=(const TabGroupStyle&) = delete;
  virtual ~TabGroupStyle();

  // returns whether the underline for the group should be hidden
  virtual bool TabGroupUnderlineShouldBeHidden() const;
  virtual bool TabGroupUnderlineShouldBeHidden(
      const views::View* leading_view,
      const views::View* trailing_view) const;
  // Returns the path of an underline given the local bounds of the underline.
  virtual SkPath GetUnderlinePath(gfx::Rect local_bounds) const;

  // Returns the bounds of a title chip without any text.
  virtual gfx::Rect GetEmptyTitleChipBounds(const TabGroupHeader* header) const;

  // Returns the background of a title chip without any text.
  virtual std::unique_ptr<views::Background> GetEmptyTitleChipBackground(
      SkColor color) const;

  // Returns the radius for the tab group header's highlight path. This is used
  // when the header is focused.
  virtual int GetHighlightPathGeneratorCornerRadius(
      const views::View* title) const;

  // While calculating desired width of a tab group an adjustment value is added
  // for the distance between the tab group header and the right tab.
  virtual int GetTitleAdjustmentToTabGroupHeaderDesiredWidth(
      std::u16string title) const;

  // Returns the sync icon width.
  virtual float GetSyncIconWidth() const;

  // Returns the attention indicator icon width.
  virtual float GetAttentionIndicatorWidth() const;

  // Overlap between the tab group view and neighbor tab slot
  virtual int GetTabGroupViewOverlap() const;

 protected:
  const raw_ref<const TabGroupViews> tab_group_views_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_GROUP_STYLE_H_
