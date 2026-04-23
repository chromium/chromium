// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permissions/chip/permission_dashboard_view.h"

#include <algorithm>
#include <vector>

#include "base/time/time.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/permissions/chip/permission_chip_view.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkPathBuilder.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/view_class_properties.h"

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(PermissionDashboardView,
                                      kDashboardElementId);

namespace {

// Distance drawn under the indicator chip.
constexpr int kExtraArcPadding = 2;

class IndicatorDividerBackground : public views::Background {
 public:
  // Background will have right rounded side with |arc_radius|.
  IndicatorDividerBackground(SkColor color, SkScalar arc_radius)
      : arc_radius_(arc_radius) {
    SetColor(color);
  }

  IndicatorDividerBackground(const IndicatorDividerBackground&) = delete;
  IndicatorDividerBackground& operator=(const IndicatorDividerBackground&) =
      delete;

  ~IndicatorDividerBackground() override = default;

  // views::Background
  void Paint(gfx::Canvas* canvas, views::View* view) const override {
    /*
     *   The background draws a rectangle with arc right side.
     *   * * * * * \
     *   *          \
     *   *           |
     *   *          /
     *   * * * * * /
     */
    const SkScalar height = view->height();

    // The arc is drawn between two chips and its width is equal to a distance
    // between chips and an extra padding that is drawn under the indicator
    // chip.
    const int arc_width =
        GetLayoutConstant(LayoutConstant::kLocationBarChipPadding) +
        kExtraArcPadding;
    const SkScalar arc_x = view->width() - arc_width;

    const SkPath path =
        SkPathBuilder()
            .lineTo(arc_x, 0)
            .rArcTo(SkVector(arc_radius_, arc_radius_), 0,
                    SkPathBuilder::kSmall_ArcSize, SkPathDirection::kCW,
                    SkPoint(0, height))
            .lineTo(0, height)
            .close()
            .detach();

    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setColor(color().ResolveToSkColor(view->GetColorProvider()));
    canvas->DrawPath(path, flags);
  }

 private:
  SkScalar arc_radius_;
};

}  // namespace

PermissionDashboardView::PermissionDashboardView() {
  SetProperty(views::kElementIdentifierKey, kDashboardElementId);

  SetVisible(false);

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal));

  // Left-Hand Side Activity indicators chip.
  auto indicator_chip = std::make_unique<PermissionChipView>(
      PermissionChipView::Role::kIndicatorChip,
      PermissionChipView::PressedCallback());
  indicator_chip->SetVisible(false);
  indicator_chip_ = AddChildView(std::move(indicator_chip));

  // An empty view is created to be placed between the LHS activity indicator
  // chip and the permission request chip. This view is a divider that creates
  // an illusion of an empty space between chips.
  auto chip_divider_view = std::make_unique<views::View>();
  chip_divider_view->SetPaintToLayer();
  chip_divider_view->layer()->SetFillsBoundsOpaquely(false);
  chip_divider_view->SetVisible(false);
  chip_divider_view_ = AddChildView(std::move(chip_divider_view));

  // Permission request chip.
  auto request_chip = std::make_unique<PermissionChipView>(
      PermissionChipView::Role::kPermissionRequestChip,
      PermissionChipView::PressedCallback());
  request_chip->SetVisible(false);
  request_chip_ = AddChildView(std::move(request_chip));

  // This is needed to make sure that the permission dashboard view is
  // recognized as a single button. Individual elements inside this view should
  // not be accessible and/or focusable.
  indicator_chip_->GetViewAccessibility().SetIsIgnored(true);
  request_chip_->GetViewAccessibility().SetIsIgnored(true);
  chip_divider_view_->GetViewAccessibility().SetIsIgnored(true);

  GetViewAccessibility().SetRole(ax::mojom::Role::kButton);
}

PermissionDashboardView::~PermissionDashboardView() = default;

void PermissionDashboardView::SetDividerBackgroundColor(
    SkColor background_color) {
  constexpr int kDividerViewArcRadius = 16;
  chip_divider_view_->SetBackground(
      std::make_unique<IndicatorDividerBackground>(background_color,
                                                   kDividerViewArcRadius));
}

void PermissionDashboardView::SetVisible(bool visible) {
  views::View::SetVisible(visible);
}

bool PermissionDashboardView::GetVisible() const {
  return views::View::GetVisible();
}

PermissionChipView* PermissionDashboardView::GetRequestChip() {
  return request_chip_;
}

PermissionChipView* PermissionDashboardView::GetIndicatorChip() {
  return indicator_chip_;
}

views::BubbleAnchor PermissionDashboardView::GetAnchor() {
  return views::BubbleAnchor(this);
}

void PermissionDashboardView::UpdateDividerViewVisibility() {
  // This method can be called even if both chips are hidden. Exit early to
  // avoid unnecessary computations.
  if (!indicator_chip_->GetVisible() && !request_chip_->GetVisible()) {
    chip_divider_view_->SetVisible(false);
    return;
  }

  const bool is_visible =
      indicator_chip_->GetVisible() && request_chip_->GetVisible();

  if (is_visible) {
    int width = indicator_chip_->GetIconViewWidth();
    chip_divider_view_->SetPreferredSize(
        gfx::Size(width, indicator_chip_->GetHeightForWidth(width)));
    // `chip_divider_view_` should be shown under `indicator_chip_`. Move
    // `chip_divider_view_` to the left by setting a negative margin.
    chip_divider_view_->SetProperty(
        views::kMarginsKey,
        gfx::Insets::TLBR(
            0,
            GetLayoutConstant(LayoutConstant::kLocationBarChipPadding) - width,
            0, 0));
  }

  // The divivder arc's width is needed to offset the request chip and draw it
  // under the arc.
  const int arc_width =
      GetLayoutConstant(LayoutConstant::kLocationBarChipPadding) +
      kExtraArcPadding;
  request_chip_->UpdateForDividerVisibility(is_visible, arc_width);
  chip_divider_view_->SetVisible(is_visible);
}

gfx::Size PermissionDashboardView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  if (!request_chip_->GetVisible() && !indicator_chip_->GetVisible()) {
    return gfx::Size();
  }

  if (!request_chip_->GetVisible()) {
    return indicator_chip_->GetPreferredSize();
  }

  if (!indicator_chip_->GetVisible()) {
    return request_chip_->GetPreferredSize();
  }

  // Part of the request chip that is drawn under the arc.
  const int request_chip_margin =
      GetLayoutConstant(LayoutConstant::kLocationBarChipPadding) +
      kExtraArcPadding;

  // Visible width of the request chip.
  int request_chip_visible_width =
      request_chip_->GetPreferredSize().width() - request_chip_margin;

  gfx::Size size = indicator_chip_->GetPreferredSize();
  size.Enlarge(request_chip_visible_width +
                   GetLayoutConstant(LayoutConstant::kLocationBarChipPadding),
               0);
  return size;
}

views::View::Views PermissionDashboardView::GetChildrenInZOrder() {
  View::Views paint_order = View::GetChildrenInZOrder();
  std::ranges::reverse(paint_order);
  return paint_order;
}

void PermissionDashboardView::ChildVisibilityChanged(views::View* child) {
  if (child == indicator_chip_ || child == request_chip_) {
    UpdateDividerViewVisibility();
  }
}

BEGIN_METADATA(PermissionDashboardView)
END_METADATA
