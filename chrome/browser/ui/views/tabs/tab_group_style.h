// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_GROUP_STYLE_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_GROUP_STYLE_H_

#include <string>
#include "base/memory/raw_ptr.h"
#include "tab_group_header.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/view.h"

class TabGroupViews;

// Default styling of tab groups.
class TabGroupStyle {
 public:
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

  // Returns the starting y coordinate of the title chip from the `tabstrip`.
  virtual gfx::Point GetTitleChipOffset(absl::optional<int> text_height) const;

  // Returns the background of a title chip without any text.
  virtual std::unique_ptr<views::Background> GetEmptyTitleChipBackground(
      SkColor color) const;

  // Returns the radius for the tab group header's highlight path. This is used
  // when the header is focused.
  virtual int GetHighlightPathGeneratorCornerRadius(
      const views::View* title) const;

  // Returns the insets for a header chip that has text.
  virtual gfx::Insets GetInsetsForHeaderChip(bool should_show_sync_icon) const;

  // While calculating desired width of a tab group an adjustment value is added
  // for the distance between the tab group header and the right tab.
  virtual int GetTitleAdjustmentToTabGroupHeaderDesiredWidth(
      std::u16string title) const;

  // Returns the size of an empty chip without any text.
  virtual float GetEmptyChipSize() const;

  // Returns the sync icon width.
  virtual float GetSyncIconWidth() const;

  // The radius of the tab group header chip
  virtual int GetChipCornerRadius() const;

  // Overlap between the tab group view and neighbor tab slot
  virtual int GetTabGroupViewOverlap() const;

 protected:
  const raw_ref<const TabGroupViews> tab_group_views_;
};

// Styling of tab groups when the #chrome-refresh-2023 flag is on.
class ChromeRefresh2023TabGroupStyle : public TabGroupStyle {
 public:
  static int GetTabGroupOverlapAdjustment();
  explicit ChromeRefresh2023TabGroupStyle(const TabGroupViews& tab_group_views);
  ChromeRefresh2023TabGroupStyle(const ChromeRefresh2023TabGroupStyle&) =
      delete;
  ChromeRefresh2023TabGroupStyle& operator=(
      const ChromeRefresh2023TabGroupStyle&) = delete;
  ~ChromeRefresh2023TabGroupStyle() override;

  bool TabGroupUnderlineShouldBeHidden() const override;
  bool TabGroupUnderlineShouldBeHidden(
      const views::View* leading_view,
      const views::View* trailing_view) const override;
  SkPath GetUnderlinePath(gfx::Rect local_bounds) const override;
  gfx::Rect GetEmptyTitleChipBounds(
      const TabGroupHeader* header) const override;
  std::unique_ptr<views::Background> GetEmptyTitleChipBackground(
      SkColor color) const override;
  int GetHighlightPathGeneratorCornerRadius(
      const views::View* title) const override;
  gfx::Insets GetInsetsForHeaderChip(bool should_show_sync_icon) const override;
  int GetTitleAdjustmentToTabGroupHeaderDesiredWidth(
      std::u16string title) const override;
  float GetEmptyChipSize() const override;
  gfx::Point GetTitleChipOffset(absl::optional<int> text_height) const override;
  float GetSyncIconWidth() const override;
  int GetChipCornerRadius() const override;
  int GetTabGroupViewOverlap() const override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_GROUP_STYLE_H_
