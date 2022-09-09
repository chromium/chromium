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
#include "chrome/browser/ui/views/tabs/tab_group_views.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/background.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"

constexpr int TabGroupUnderline::kStrokeThickness;

TabGroupUnderline::TabGroupUnderline(TabGroupViews* tab_group_views,
                                     const tab_groups::TabGroupId& group)
    : tab_group_views_(tab_group_views), group_(group) {}

void TabGroupUnderline::OnPaint(gfx::Canvas* canvas) {
  SkPath path = GetPath();
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(tab_group_views_->GetGroupColor());
  flags.setStyle(cc::PaintFlags::kFill_Style);
  canvas->DrawPath(path, flags);
}

void TabGroupUnderline::UpdateBounds(views::View* leading_view,
                                     views::View* trailing_view) {
  // If there are no views to underline, don't show the underline.
  if (!leading_view) {
    SetVisible(false);
    return;
  }

  gfx::RectF leading_bounds = gfx::RectF(leading_view->bounds());
  ConvertRectToTarget(leading_view->parent(), parent(), &leading_bounds);
  leading_bounds.Inset(gfx::InsetsF(GetInsetsForUnderline(leading_view)));

  gfx::RectF trailing_bounds = gfx::RectF(trailing_view->bounds());
  ConvertRectToTarget(trailing_view->parent(), parent(), &trailing_bounds);
  trailing_bounds.Inset(gfx::InsetsF(GetInsetsForUnderline(trailing_view)));

  gfx::Rect group_bounds = ToEnclosingRect(leading_bounds);
  group_bounds.UnionEvenIfEmpty(ToEnclosingRect(trailing_bounds));

  // The width may be zero if the group underline and header are initialized at
  // the same time, as with tab restore. In this case, don't show the underline.
  if (group_bounds.width() == 0) {
    SetVisible(false);
    return;
  }

  SetVisible(true);
  const int y =
      group_bounds.height() - GetLayoutConstant(TABSTRIP_TOOLBAR_OVERLAP);
  SetBounds(group_bounds.x(), y - kStrokeThickness, group_bounds.width(),
            kStrokeThickness);
}

gfx::Insets TabGroupUnderline::GetInsetsForUnderline(
    views::View* sibling_view) const {
  // Inset normally from a header - this will always be the left boundary of
  // the group, and may be the right boundary if the group is collapsed.
  TabGroupHeader* header = views::AsViewClass<TabGroupHeader>(sibling_view);
  if (header)
    return gfx::Insets::TLBR(0, GetStrokeInset(), 0, GetStrokeInset());

  Tab* tab = views::AsViewClass<Tab>(sibling_view);
  DCHECK(tab);

  // Active tabs need the rounded bits of the underline poking out the sides.
  if (tab->IsActive())
    return gfx::Insets::TLBR(0, -kStrokeThickness, 0, -kStrokeThickness);

  // Inactive tabs are inset like group headers.
  int left_inset = GetStrokeInset();
  int right_inset = GetStrokeInset();

  return gfx::Insets::TLBR(0, left_inset, 0, right_inset);
}

// static
int TabGroupUnderline::GetStrokeInset() {
  return TabStyle::GetTabOverlap() + kStrokeThickness;
}

SkPath TabGroupUnderline::GetPath() const {
  SkPath path;

  path.moveTo(0, kStrokeThickness);
  path.arcTo(kStrokeThickness, kStrokeThickness, 0, SkPath::kSmall_ArcSize,
             SkPathDirection::kCW, kStrokeThickness, 0);
  path.lineTo(width() - kStrokeThickness, 0);
  path.arcTo(kStrokeThickness, kStrokeThickness, 0, SkPath::kSmall_ArcSize,
             SkPathDirection::kCW, width(), kStrokeThickness);
  path.close();

  return path;
}

BEGIN_METADATA(TabGroupUnderline, views::View)
END_METADATA
