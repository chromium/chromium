// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/shadow_overlay_view.h"

#include "cc/paint/paint_flags.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkPathBuilder.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view_shadow.h"

class ShadowOverlayView::ShadowBox : public views::View {
  METADATA_HEADER(ShadowBox, views::View)

 public:
  ShadowBox() { SetCanProcessEventsWithinSubtree(false); }
  ~ShadowBox() override = default;

  void SetShadowVisible(bool visible) {
    // No-op if visible set is the same as current state.
    if ((visible && layer()) || (!visible && !layer())) {
      return;
    }

    if (visible) {
      const int rounded_corner_radius =
          GetLayoutProvider()->GetCornerRadiusMetric(views::Emphasis::kHigh);
      const int elevation =
          GetLayoutProvider()->GetShadowElevationMetric(views::Emphasis::kHigh);

      view_shadow_ = std::make_unique<views::ViewShadow>(this, elevation);
      view_shadow_->SetRoundedCornerRadius(rounded_corner_radius);
    } else {
      view_shadow_.reset();
      DestroyLayer();
    }
  }

 private:
  // The shadow and elevation around main_container to visually separate the
  // container from MainRegionBackground when the toolbar_height_side_panel is
  // visible.
  std::unique_ptr<views::ViewShadow> view_shadow_;
};

using ShadowBox = ShadowOverlayView::ShadowBox;

BEGIN_METADATA(ShadowBox)
END_METADATA

ShadowOverlayView::ShadowOverlayView() {
  SetCanProcessEventsWithinSubtree(false);
  shadow_box_ = AddChildView(std::make_unique<ShadowBox>());
  SetLayoutManager(std::make_unique<views::FillLayout>());

  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  // Starts hidden; visibility set by layout.
  SetVisible(false);
}

ShadowOverlayView::~ShadowOverlayView() = default;

void ShadowOverlayView::VisibilityChanged(View* starting_from, bool visible) {
  if (starting_from == this) {
    shadow_box_->SetShadowVisible(visible);
  }
}

void ShadowOverlayView::OnPaint(gfx::Canvas* canvas) {
  views::View::OnPaint(canvas);

  cc::PaintFlags flags;
  flags.setStrokeWidth(1);
  flags.setColor(GetColorProvider()->GetColor(kColorToolbar));
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setAntiAlias(true);

  const float corner_radius =
      GetLayoutProvider()->GetCornerRadiusMetric(views::Emphasis::kHigh);

  SkPathBuilder upper_left;
  upper_left.moveTo(0, 0);
  upper_left.lineTo(corner_radius, 0);
  upper_left.arcTo(SkVector(corner_radius, corner_radius), 0,
                   SkPathBuilder::kSmall_ArcSize, SkPathDirection::kCCW,
                   SkPoint(0, corner_radius));
  upper_left.close();
  canvas->DrawPath(upper_left.detach(), flags);

  SkPathBuilder upper_right;
  upper_right.moveTo(width(), 0);
  upper_right.lineTo(width(), corner_radius);
  upper_right.arcTo(SkVector(corner_radius, corner_radius), 0,
                    SkPathBuilder::kSmall_ArcSize, SkPathDirection::kCCW,
                    SkPoint(width() - corner_radius, 0));
  upper_right.close();
  canvas->DrawPath(upper_right.detach(), flags);

  SkPathBuilder lower_left;
  lower_left.moveTo(0, height());
  lower_left.lineTo(0, height() - corner_radius);
  lower_left.arcTo(SkVector(corner_radius, corner_radius), 0,
                   SkPathBuilder::kSmall_ArcSize, SkPathDirection::kCCW,
                   SkPoint(corner_radius, height()));
  lower_left.close();
  canvas->DrawPath(lower_left.detach(), flags);

  SkPathBuilder lower_right;
  lower_right.moveTo(width(), height());
  lower_right.lineTo(width() - corner_radius, height());
  lower_right.arcTo(SkVector(corner_radius, corner_radius), 0,
                    SkPathBuilder::kSmall_ArcSize, SkPathDirection::kCCW,
                    SkPoint(width(), height() - corner_radius));
  lower_right.close();
  canvas->DrawPath(lower_right.detach(), flags);
}

BEGIN_METADATA(ShadowOverlayView)
END_METADATA
