// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_group_style_views.h"

#include <memory>
#include <string>

#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/views/tabs/common/tab_group_style.h"
#include "chrome/browser/ui/views/tabs/tab_group_header.h"
#include "chrome/browser/ui/views/tabs/tab_group_underline.h"
#include "chrome/browser/ui/views/tabs/tab_group_views.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/views/background.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"

namespace {

constexpr int kTitleAdjustmentForNonEmptyHeader = -2;
// The width of the sync icon when a tab group is saved.
constexpr int kSyncIconWidth = 16;
// The width of the attention indicator icon for a shared tab group.
constexpr int kAttentionIndicatorWidth = 8;

}  // namespace

TabGroupStyleViews::TabGroupStyleViews(const TabGroupViews& tab_group_views)
    : tab_group_views_(tab_group_views) {}

TabGroupStyleViews::~TabGroupStyleViews() = default;

bool TabGroupStyleViews::TabGroupUnderlineShouldBeHidden() const {
  if (tab_group_views_->IsFocusModeActive()) {
    return true;
  }

  const auto [leading_group_view, trailing_group_view] =
      tab_group_views_->GetLeadingTrailingGroupViews();

  return TabGroupUnderlineShouldBeHidden(leading_group_view,
                                         trailing_group_view);
}

bool TabGroupStyleViews::TabGroupUnderlineShouldBeHidden(
    const views::View* const leading_view,
    const views::View* const trailing_view) const {
  if (tab_group_views_->IsFocusModeActive()) {
    return true;
  }

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
SkPath TabGroupStyleViews::GetUnderlinePath(
    const gfx::Rect local_bounds) const {
  return SkPath::RRect(SkRRect::MakeRectXY(
      gfx::RectToSkRect(local_bounds), TabGroupUnderline::kStrokeThickness / 2,
      TabGroupUnderline::kStrokeThickness / 2));
}

gfx::Rect TabGroupStyleViews::GetEmptyTitleChipBounds(
    const TabGroupHeader* const header) const {
  return gfx::Rect(TabGroupStyle::GetTitleChipOffset(std::nullopt).x(),
                   TabGroupStyle::GetTitleChipOffset(std::nullopt).y(),
                   TabGroupStyle::GetEmptyChipSize(),
                   TabGroupStyle::GetEmptyChipSize());
}

std::unique_ptr<views::Background>
TabGroupStyleViews::GetEmptyTitleChipBackground(const SkColor color) const {
  return views::CreateRoundedRectBackground(
      color, TabGroupStyle::GetChipCornerRadius());
}

int TabGroupStyleViews::GetHighlightPathGeneratorCornerRadius(
    const views::View* const title) const {
  return TabGroupStyle::GetChipCornerRadius();
}

int TabGroupStyleViews::GetTitleAdjustmentToTabGroupHeaderDesiredWidth(
    const std::u16string title) const {
  // Since the shape of the header in ChromeRefresh23 is a rounded rect this
  // value should be `kTitleAdjustmentForNonEmptyHeader`.
  return kTitleAdjustmentForNonEmptyHeader;
}

float TabGroupStyleViews::GetSyncIconWidth() const {
  return kSyncIconWidth;
}

float TabGroupStyleViews::GetAttentionIndicatorWidth() const {
  return kAttentionIndicatorWidth;
}

int TabGroupStyleViews::GetTabGroupViewOverlap() const {
  // For refresh the tab has an overlap value is 18. In order to have a margin
  // of 10 from the neighbor tabs this is required.
  return TabStyle::Get()->GetTabOverlap() -
         TabGroupStyle::GetTabGroupOverlapAdjustment();
}
