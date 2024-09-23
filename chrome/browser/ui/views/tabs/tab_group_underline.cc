// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_group_underline.h"

#include <memory>
#include <utility>

#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_group_header.h"
#include "chrome/browser/ui/views/tabs/tab_group_style.h"
#include "chrome/browser/ui/views/tabs/tab_group_views.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/background.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"

constexpr int TabGroupUnderline::kStrokeThickness;

TabGroupUnderline::TabGroupUnderline(TabGroupViews* tab_group_views,
                                     const tab_groups::TabGroupId& group,
                                     const TabGroupStyle& style)
    : tab_group_views_(tab_group_views), group_(group), style_(style) {}

void TabGroupUnderline::OnPaint(gfx::Canvas* canvas) {
  SkPath path = style_->GetUnderlinePath(GetLocalBounds());
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(tab_group_views_->GetGroupColor());
  flags.setStyle(cc::PaintFlags::kFill_Style);
  canvas->DrawPath(path, flags);
}

void TabGroupUnderline::UpdateBounds(const views::View* const leading_view,
                                     const views::View* const trailing_view) {
  // If there are no views to underline, don't show the underline.
  if (!leading_view) {
    SetVisible(false);
    return;
  }

  const gfx::Rect tab_group_underline_bounds =
      CalculateTabGroupUnderlineBounds(this, leading_view, trailing_view);

  // The width may be zero if the group underline and header are initialized at
  // the same time, as with tab restore. In this case, don't show the underline.
  if (tab_group_underline_bounds.width() == 0) {
    SetVisible(false);
    return;
  }

  SetVisible(
      !style_->TabGroupUnderlineShouldBeHidden(leading_view, trailing_view));
  SetBoundsRect(tab_group_underline_bounds);
}

gfx::Rect TabGroupUnderline::CalculateTabGroupUnderlineBounds(
    const views::View* const underline_view,
    const views::View* const leading_view,
    const views::View* const trailing_view) const {
  gfx::RectF leading_bounds = views::View::ConvertRectToTarget(
      leading_view->parent(), underline_view->parent(),
      gfx::RectF(leading_view->bounds()));
  leading_bounds.Inset(gfx::InsetsF(GetInsetsForUnderline(leading_view)));

  gfx::RectF trailing_bounds = views::View::ConvertRectToTarget(
      trailing_view->parent(), underline_view->parent(),
      gfx::RectF(trailing_view->bounds()));
  trailing_bounds.Inset(gfx::InsetsF(GetInsetsForUnderline(trailing_view)));

  gfx::Rect group_bounds = ToEnclosingRect(leading_bounds);
  group_bounds.UnionEvenIfEmpty(ToEnclosingRect(trailing_bounds));

  const int y =
      group_bounds.bottom() - GetLayoutConstant(TABSTRIP_TOOLBAR_OVERLAP);

  return gfx::Rect(group_bounds.x(), y - kStrokeThickness, group_bounds.width(),
                   kStrokeThickness);
}

gfx::Insets TabGroupUnderline::GetInsetsForUnderline(
    const views::View* const sibling_view) const {
  // Inset normally from a header - this will always be the left boundary of
  // the group, and may be the right boundary if the group is collapsed.
  const TabGroupHeader* const header =
      views::AsViewClass<TabGroupHeader>(sibling_view);
  if (header) {
    return gfx::Insets::TLBR(0, TabGroupUnderline::GetStrokeInset(), 0,
                             TabGroupUnderline::GetStrokeInset());
  }

  const Tab* const tab = views::AsViewClass<Tab>(sibling_view);
  DCHECK(tab);

  // Active tabs need the rounded bits of the underline poking out the sides.
  if (tab->IsActive()) {
    return gfx::Insets::TLBR(0, -kStrokeThickness, 0, -kStrokeThickness);
  }

  // Inactive tabs are inset like group headers.
  const int left_inset = TabGroupUnderline::GetStrokeInset();
  const int right_inset = TabGroupUnderline::GetStrokeInset();

  return gfx::Insets::TLBR(0, left_inset, 0, right_inset);
}

void TabGroupUnderline::MaybeSetVisible(const bool visible) {
  SetVisible(visible && !style_->TabGroupUnderlineShouldBeHidden());
}

// static
int TabGroupUnderline::GetStrokeInset() {
  return TabStyle::Get()->GetTabOverlap() -
         TabGroupStyle::GetTabGroupOverlapAdjustment() + kStrokeThickness;
}

BEGIN_METADATA(TabGroupUnderline)
END_METADATA
