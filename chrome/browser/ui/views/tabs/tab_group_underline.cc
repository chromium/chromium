// Copyright 2019 The Chromium Authors. All rights reserved.
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
#include "ui/gfx/canvas.h"
#include "ui/views/background.h"
#include "ui/views/metadata/metadata_impl_macros.h"
#include "ui/views/view.h"

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

void TabGroupUnderline::UpdateBounds(const gfx::Rect& group_bounds) {
  const int start_x = GetStart(group_bounds);
  const int end_x = GetEnd(group_bounds);

  // The width may be zero if the group underline and header are initialized at
  // the same time, as with tab restore. In this case, don't update the bounds
  // and defer to the next paint cycle.
  if (end_x <= start_x)
    return;

  const int y =
      group_bounds.height() - GetLayoutConstant(TABSTRIP_TOOLBAR_OVERLAP);
  SetBounds(start_x, y - kStrokeThickness, end_x - start_x, kStrokeThickness);
}

// static
int TabGroupUnderline::GetStrokeInset() {
  return TabStyle::GetTabOverlap() + kStrokeThickness;
}

int TabGroupUnderline::GetStart(const gfx::Rect& group_bounds) const {
  return group_bounds.x() + GetStrokeInset();
}

int TabGroupUnderline::GetEnd(const gfx::Rect& group_bounds) const {
  const Tab* last_grouped_tab = tab_group_views_->GetLastTabInGroup();
  if (!last_grouped_tab)
    return group_bounds.right() - GetStrokeInset();

  return group_bounds.right() +
         (last_grouped_tab->IsActive() ? kStrokeThickness : -GetStrokeInset());
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
