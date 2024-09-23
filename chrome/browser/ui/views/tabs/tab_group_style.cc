// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_group_style.h"

#include "base/feature_list.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_group_header.h"
#include "chrome/browser/ui/views/tabs/tab_group_underline.h"
#include "chrome/browser/ui/views/tabs/tab_group_views.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"

namespace {

constexpr int kHeaderChipVerticalInset = 2;
constexpr int kTitleAdjustmentForNonEmptyHeader = -2;
// The width of the sync icon when a tab group is saved.
constexpr int kSyncIconWidth = 16;
// The size of the empty chip.
constexpr int kEmptyChipSize = 20;
constexpr int kSyncIconLeftMargin = 2;
constexpr int kCornerRadius = 6;
constexpr int kTabGroupOverlapAdjustment = 2;

}  // namespace

// static
int TabGroupStyle::GetTabGroupOverlapAdjustment() {
  return kTabGroupOverlapAdjustment;
}

TabGroupStyle::TabGroupStyle(const TabGroupViews& tab_group_views)
    : tab_group_views_(tab_group_views) {}

TabGroupStyle::~TabGroupStyle() = default;

bool TabGroupStyle::TabGroupUnderlineShouldBeHidden() const {
  const auto [leading_group_view, trailing_group_view] =
      tab_group_views_->GetLeadingTrailingGroupViews();

  return TabGroupUnderlineShouldBeHidden(leading_group_view,
                                         trailing_group_view);
}

bool TabGroupStyle::TabGroupUnderlineShouldBeHidden(
    const views::View* const leading_view,
    const views::View* const trailing_view) const {
  const TabGroupHeader* const leading_view_group_header =
      views::AsViewClass<TabGroupHeader>(leading_view);
  const TabGroupHeader* const trailing_view_group_header =
      views::AsViewClass<TabGroupHeader>(trailing_view);

  if (leading_view_group_header && trailing_view_group_header &&
      leading_view_group_header == trailing_view_group_header) {
    return true;
  }

  return false;
}

// The path is a rounded rect.
SkPath TabGroupStyle::GetUnderlinePath(const gfx::Rect local_bounds) const {
  SkPath path;
  path.addRoundRect(gfx::RectToSkRect(local_bounds),
                    TabGroupUnderline::kStrokeThickness / 2,
                    TabGroupUnderline::kStrokeThickness / 2);
  return path;
}

gfx::Rect TabGroupStyle::GetEmptyTitleChipBounds(
    const TabGroupHeader* const header) const {
  return gfx::Rect(GetTitleChipOffset(std::nullopt).x(),
                   GetTitleChipOffset(std::nullopt).y(), GetEmptyChipSize(),
                   GetEmptyChipSize());
}

gfx::Point TabGroupStyle::GetTitleChipOffset(
    std::optional<int> text_height) const {
  const int total_space = GetLayoutConstant(TAB_STRIP_HEIGHT) -
                          GetEmptyChipSize() -
                          GetLayoutConstant(TABSTRIP_TOOLBAR_OVERLAP);
  return gfx::Point(TabStyle::Get()->GetTabOverlap() - 2, total_space / 2);
}

std::unique_ptr<views::Background> TabGroupStyle::GetEmptyTitleChipBackground(
    const SkColor color) const {
  return views::CreateRoundedRectBackground(color, GetChipCornerRadius());
}

gfx::Insets TabGroupStyle::GetInsetsForHeaderChip(
    bool should_show_sync_icon) const {
  return gfx::Insets::TLBR(
      kHeaderChipVerticalInset,
      should_show_sync_icon ? kSyncIconLeftMargin : GetChipCornerRadius(),
      kHeaderChipVerticalInset, GetChipCornerRadius());
}

int TabGroupStyle::GetHighlightPathGeneratorCornerRadius(
    const views::View* const title) const {
  return GetChipCornerRadius();
}

int TabGroupStyle::GetTitleAdjustmentToTabGroupHeaderDesiredWidth(
    const std::u16string title) const {
  // Since the shape of the header in ChromeRefresh23 is a rounded rect this
  // value should be `kTitleAdjustmentForNonEmptyHeader`.
  return kTitleAdjustmentForNonEmptyHeader;
}

float TabGroupStyle::GetEmptyChipSize() const {
  return kEmptyChipSize;
}

float TabGroupStyle::GetSyncIconWidth() const {
  return kSyncIconWidth;
}

int TabGroupStyle::GetChipCornerRadius() const {
  return kCornerRadius;
}

int TabGroupStyle::GetTabGroupViewOverlap() const {
  // For refresh the tab has an overlap value is 18. In order to have a margin
  // of 10 from the neighbor tabs this is required.
  return TabStyle::Get()->GetTabOverlap() - GetTabGroupOverlapAdjustment();
}
