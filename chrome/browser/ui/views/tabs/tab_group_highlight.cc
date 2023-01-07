// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_group_highlight.h"

#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/views/tabs/tab_group_views.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/canvas.h"
#include "ui/views/background.h"
#include "ui/views/view.h"

TabGroupHighlight::TabGroupHighlight(TabGroupViews* tab_group_views,
                                     const tab_groups::TabGroupId& group)
    : tab_group_views_(tab_group_views), group_(group) {}

void TabGroupHighlight::UpdateBounds(views::View* leading_view,
                                     views::View* trailing_view) {
  // If there are no views to highlight, do nothing. Our visibility is
  // controlled by our parent TabDragContext.
  if (!leading_view)
    return;
  gfx::Rect bounds = leading_view->bounds();
  bounds.UnionEvenIfEmpty(trailing_view->bounds());
  SetBoundsRect(bounds);
}

void TabGroupHighlight::OnPaint(gfx::Canvas* canvas) {
  SkPath path = GetPath();
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kFill_Style);

  // Draw two layers to simulate the color of other non-active selected tabs,
  // which use a similar drawing strategy (see GM2TabStyle::PaintTab()).
  // This is needed because the group background color alone would be slightly
  // transparent, so instead it's drawn over the inactive background color.
  flags.setColor(tab_group_views_->GetTabBackgroundColor());
  canvas->DrawPath(path, flags);

  flags.setColor(tab_group_views_->GetGroupBackgroundColor());
  canvas->DrawPath(path, flags);
}

bool TabGroupHighlight::GetCanProcessEventsWithinSubtree() const {
  // Don't accept any mouse events, otherwise this will prevent tabs and group
  // headers from getting clicked.
  return false;
}

SkPath TabGroupHighlight::GetPath() const {
  // This path imitates the shape of a tab (see GM2TabStyle::GetPath()). It
  // doesn't reuse the exact same GetPath() function because it doesn't need
  // much of the complexity there. Group highlights only appear on group drag,
  // which is a well-scoped interaction. A dragging group doesn't nestle in with
  // the tabs around it, so there are no special cases needed when determining
  // its shape.
  const int corner_radius = TabStyle::GetCornerRadius();

  SkPath path;
  path.moveTo(0, bounds().height());
  path.arcTo(corner_radius, corner_radius, 0, SkPath::kSmall_ArcSize,
             SkPathDirection::kCCW, corner_radius,
             bounds().height() - corner_radius);
  path.lineTo(corner_radius, corner_radius);
  path.arcTo(corner_radius, corner_radius, 0, SkPath::kSmall_ArcSize,
             SkPathDirection::kCW, 2 * corner_radius, 0);
  path.lineTo(bounds().width() - 2 * corner_radius, 0);
  path.arcTo(corner_radius, corner_radius, 0, SkPath::kSmall_ArcSize,
             SkPathDirection::kCW, bounds().width() - corner_radius,
             corner_radius);
  path.lineTo(bounds().width() - corner_radius,
              bounds().height() - corner_radius);
  path.arcTo(corner_radius, corner_radius, 0, SkPath::kSmall_ArcSize,
             SkPathDirection::kCCW, bounds().width(), bounds().height());
  path.close();

  return path;
}

BEGIN_METADATA(TabGroupHighlight, views::View)
END_METADATA
