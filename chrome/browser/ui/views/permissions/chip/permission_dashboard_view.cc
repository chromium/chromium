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
#include "components/vector_icons/vector_icons.h"
#include "third_party/skia/include/core/SkColor.h"
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
    SetNativeControlColor(color);
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
    SkPath path;

    const SkScalar height = view->height();

    // The arc is drawn between two chips and its width is equal to a distance
    // between chips and an extra padding that is drawn under the indicator
    // chip.
    const int arc_width =
        GetLayoutConstant(LOCATION_BAR_CHIP_PADDING) + kExtraArcPadding;
    const SkScalar arc_x = view->width() - arc_width;

    path.lineTo(arc_x, 0);
    path.rArcTo(arc_radius_, arc_radius_, 0, SkPath::kSmall_ArcSize,
                SkPathDirection::kCW, 0, height);
    path.lineTo(0, height);
    path.close();

    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setColor(get_color());
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
  anchored_chip_ = AddChildView(std::make_unique<PermissionChipView>(
      PermissionChipView::PressedCallback()));

  // An empty view is created to be placed between the LHS activity indicator
  // chip and the permission request chip. This view is a divider that creates
  // an illusion of an empty space between chips.
  chip_divider_view_ = AddChildView(std::make_unique<views::View>());
  chip_divider_view_->SetPaintToLayer();
  chip_divider_view_->layer()->SetFillsBoundsOpaquely(false);
  chip_divider_view_->SetVisible(false);

  // Permission request chip.
  secondary_chip_ = AddChildView(std::make_unique<PermissionChipView>(
      PermissionChipView::PressedCallback()));

  // It is unclear which chip will be shown first, hence hide both of them.
  secondary_chip_->SetVisible(false);
  anchored_chip_->SetVisible(false);

  // This is needed to make sure that the permission dashboard view is
  // recognized as a single button. Individual elements inside this view should
  // not be accessible and/or focusable.
  anchored_chip_->GetViewAccessibility().SetIsIgnored(true);
  secondary_chip_->GetViewAccessibility().SetIsIgnored(true);
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

void PermissionDashboardView::UpdateDividerViewVisibility() {
  const bool is_visible =
      anchored_chip_->GetVisible() && secondary_chip_->GetVisible();

  if (is_visible) {
    int width = anchored_chip_->GetIconViewWidth();
    chip_divider_view_->SetPreferredSize(
        gfx::Size(width, anchored_chip_->GetHeightForWidth(width)));
    // `chip_divider_view_` should be shown under `anchored_chip_`. Move
    // `chip_divider_view_` to the left by setting a negative margin.
    chip_divider_view_->SetProperty(
        views::kMarginsKey,
        gfx::Insets::TLBR(
            0, GetLayoutConstant(LOCATION_BAR_CHIP_PADDING) - width, 0, 0));
  }

  // The divivder arc's width is needed to offset the request chip and draw it
  // under the arc.
  const int arc_width =
      GetLayoutConstant(LOCATION_BAR_CHIP_PADDING) + kExtraArcPadding;
  secondary_chip_->UpdateForDividerVisibility(is_visible, arc_width);
  chip_divider_view_->SetVisible(is_visible);
}

gfx::Size PermissionDashboardView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  if (!secondary_chip_->GetVisible() && !anchored_chip_->GetVisible()) {
    return gfx::Size();
  }

  if (!secondary_chip_->GetVisible()) {
    return anchored_chip_->GetPreferredSize();
  }

  if (!anchored_chip_->GetVisible()) {
    return secondary_chip_->GetPreferredSize();
  }

  // Part of the request chip that is drawn under the arc.
  const int secondary_chip_margin =
      GetLayoutConstant(LOCATION_BAR_CHIP_PADDING) + kExtraArcPadding;

  // Visible width of the request chip.
  int secondary_chip_visible_width =
      secondary_chip_->GetPreferredSize().width() - secondary_chip_margin;

  gfx::Size size = anchored_chip_->GetPreferredSize();
  size.Enlarge(secondary_chip_visible_width +
                   GetLayoutConstant(LOCATION_BAR_CHIP_PADDING),
               0);
  return size;
}

views::View::Views PermissionDashboardView::GetChildrenInZOrder() {
  View::Views paint_order = View::GetChildrenInZOrder();

  std::reverse(paint_order.begin(), paint_order.end());

  return paint_order;
}

BEGIN_METADATA(PermissionDashboardView)
END_METADATA
