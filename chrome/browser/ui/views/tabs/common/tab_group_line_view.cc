// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/common/tab_group_line_view.h"

#include <optional>

#include "base/check.h"
#include "base/i18n/rtl.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/tab_group_data.h"
#include "chrome/browser/ui/tabs/tab_group_theme.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/views/tabs/common/tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/common/tab_collection_z_order_manager.h"
#include "chrome/browser/ui/views/tabs/common/tab_group_view.h"
#include "chrome/browser/ui/views/tabs/common/tab_view.h"
#include "chrome/browser/ui/views/tabs/shared/tab_strip_types.h"
#include "chrome/browser/ui/views/tabs/tab_group_underline.h"
#include "chrome/browser/ui/views/tabs/tab_style_views.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkPathBuilder.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace {
constexpr int kGroupLineCornerRadius = 4;
}  // namespace

TabGroupLineView::TabGroupLineView(TabGroupView& tab_group_view)
    : tab_group_view_(tab_group_view) {
  SetProperty(kTabZOrderKey,
              TabCollectionZOrderManager::ZOrderLevel::kGroupUnderline);
  SetCanProcessEventsWithinSubtree(false);
  SetFocusBehavior(views::View::FocusBehavior::NEVER);
}

TabGroupLineView::~TabGroupLineView() = default;

void TabGroupLineView::OnPaint(gfx::Canvas* canvas) {
  if (!tab_group_view_->GetColorProvider() || !tab_group_view_->GetWidget()) {
    return;
  }

  SkColor color =
      tab_group_view_->GetColorProvider()->GetColor(GetTabGroupTabStripColorId(
          tab_group_view_->tab_group_visual_data().color(),
          tab_group_view_->GetWidget()->ShouldPaintAsActive()));

  if (tab_group_view_->orientation() == TabStripOrientation::kVertical) {
    PaintVertical(canvas, color);
  } else {
    PaintHorizontal(canvas, color);
  }
}

void TabGroupLineView::PaintVertical(gfx::Canvas* canvas, SkColor color) {
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(color);
  flags.setStyle(cc::PaintFlags::kFill_Style);

  SkRRect rrect;
  SkVector radii[4] = {
      SkVector(0, 0), SkVector(kGroupLineCornerRadius, kGroupLineCornerRadius),
      SkVector(kGroupLineCornerRadius, kGroupLineCornerRadius), SkVector(0, 0)};
  gfx::Rect local_bounds = GetLocalBounds();
  rrect.setRectRadii(
      SkRect::MakeXYWH(local_bounds.x(), local_bounds.y(), local_bounds.width(),
                       local_bounds.height()),
      radii);
  SkPathBuilder path_builder;
  path_builder.addRRect(rrect);
  canvas->DrawPath(path_builder.detach(), flags);
}

void TabGroupLineView::PaintHorizontal(gfx::Canvas* canvas, SkColor color) {
  gfx::ScopedCanvas scoped_canvas(canvas);
  const float scale = canvas->UndoDeviceScaleFactor();
  const float stroke_thickness = TabGroupUnderline::kStrokeThickness * scale;

  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(color);
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setStrokeWidth(stroke_thickness);

  std::optional<SkPath> active_tab_path = GetActiveOverlinePath(scale);
  SkPath path = GetHorizontalLinePath(active_tab_path, scale);
  canvas->DrawPath(path, flags);
}

SkPath TabGroupLineView::GetHorizontalLinePath(
    const std::optional<SkPath>& active_tab_path,
    float scale) const {
  const float stroke_inset = TabGroupUnderline::GetStrokeInset() * scale;
  const float stroke_thickness = TabGroupUnderline::kStrokeThickness * scale;
  const bool is_rtl = base::i18n::IsRTL();
  const float width_pixels = width() * scale;

  SkPathBuilder path_builder;
  if (active_tab_path.has_value() && !active_tab_path->isEmpty() &&
      active_tab_path->countPoints() > 0) {
    SkPoint start_pt = active_tab_path->getPoint(0);
    SkPoint end_pt = active_tab_path->getLastPt().value_or(start_pt);

    const float start_x = is_rtl ? stroke_inset : 0.0f;
    const float end_x = is_rtl ? width_pixels : (width_pixels - stroke_inset);

    if (start_pt.x() > start_x) {
      path_builder.moveTo(start_x, start_pt.y());
      path_builder.lineTo(start_pt.x(), start_pt.y());
    }
    path_builder.addPath(*active_tab_path);

    if (end_x > end_pt.x()) {
      path_builder.lineTo(end_x, end_pt.y());
    }
  } else {
    float bottom_extension =
        GetLayoutConstant(LayoutConstant::kTabstripToolbarOverlap) * scale;
    float y = height() * scale - bottom_extension - 0.5f * stroke_thickness;

    if (is_rtl) {
      path_builder.moveTo(stroke_inset, y);
      path_builder.lineTo(width_pixels, y);
    } else {
      path_builder.moveTo(0, y);
      path_builder.lineTo(width_pixels - stroke_inset, y);
    }
  }
  return path_builder.detach();
}

SkPath TabGroupLineView::GetOffsetOverlinePath(const TabView& tab_view,
                                               float scale) const {
  if (!tab_view.tab_styling()) {
    return SkPath();
  }
  SkPath path = tab_view.tab_styling()->GetOverlinePath(scale);
  if (path.isEmpty() || path.countPoints() == 0) {
    return SkPath();
  }
  gfx::Point origin =
      views::View::ConvertPointToTarget(&tab_view, this, gfx::Point(0, 0));
  float tx = origin.x() * scale;
  float ty = origin.y() * scale;
  return path.makeOffset(tx, ty);
}

bool TabGroupLineView::IsViewDragging(const views::View* view) const {
  CHECK(view);
  return tab_group_view_->IsViewDragging(*view);
}

std::optional<SkPath> TabGroupLineView::GetActiveTabOverlinePath(
    const TabCollectionNode& node,
    float scale) const {
  auto* tab_view = views::AsViewClass<TabView>(node.view());
  if (!tab_view || !tab_view->IsActive() || IsViewDragging(tab_view)) {
    return std::nullopt;
  }
  SkPath path = GetOffsetOverlinePath(*tab_view, scale);
  if (path.isEmpty() || path.countPoints() == 0) {
    return std::nullopt;
  }
  return path;
}

std::optional<SkPath> TabGroupLineView::GetActiveSplitOverlinePath(
    const TabCollectionNode& split_node,
    float scale) const {
  if (IsViewDragging(split_node.view())) {
    return std::nullopt;
  }
  bool is_split_active = false;
  TabView* left_tab = nullptr;
  TabView* right_tab = nullptr;
  for (const auto& split_child : split_node.children()) {
    auto* tab_view = views::AsViewClass<TabView>(split_child->view());
    CHECK(tab_view);
    CHECK(tab_view->tab_styling());
    if (tab_view->IsActive()) {
      is_split_active = true;
    }
    if (tab_view->tab_styling()->delegate()->IsLeftSplitTab()) {
      left_tab = tab_view;
    } else if (tab_view->tab_styling()->delegate()->IsRightSplitTab()) {
      right_tab = tab_view;
    }
  }
  if (!is_split_active) {
    return std::nullopt;
  }
  if (left_tab && right_tab) {
    SkPath left_path = GetOffsetOverlinePath(*left_tab, scale);
    SkPath right_path = GetOffsetOverlinePath(*right_tab, scale);
    SkPathBuilder builder;
    builder.addPath(left_path);
    builder.addPath(right_path, SkPath::AddPathMode::kExtend_AddPathMode);
    return builder.detach();
  }
  return std::nullopt;
}

std::optional<SkPath> TabGroupLineView::GetActiveOverlinePath(
    float scale) const {
  if (!tab_group_view_->collection_node()) {
    return std::nullopt;
  }

  for (const auto& child : tab_group_view_->collection_node()->children()) {
    if (child->type() == TabCollectionNode::Type::TAB) {
      if (auto path = GetActiveTabOverlinePath(*child, scale)) {
        return path;
      }
    } else if (child->type() == TabCollectionNode::Type::SPLIT) {
      if (auto path = GetActiveSplitOverlinePath(*child, scale)) {
        return path;
      }
    }
  }
  return std::nullopt;
}

BEGIN_METADATA(TabGroupLineView)
END_METADATA
