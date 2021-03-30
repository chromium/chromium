// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/ring_progress_bar.h"

#include "cc/paint/paint_flags.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/skia_util.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/metadata/metadata_impl_macros.h"

namespace {
constexpr float kStrokeWidth = 4;
constexpr base::TimeDelta kAnimationDuration =
    base::TimeDelta::FromMilliseconds(200);
static constexpr SkColor kRingColor = SkColorSetRGB(66, 133, 224);
static constexpr SkColor kBackgroundColor = SkColorSetRGB(218, 220, 224);
}  // namespace

RingProgressBar::RingProgressBar() = default;
RingProgressBar::~RingProgressBar() = default;

void RingProgressBar::SetValue(double initial, double target) {
  initial_ = std::max(0., std::min(initial, 1.));
  target_ = std::max(0., std::min(target, 1.));
  animation_ = std::make_unique<gfx::LinearAnimation>(this);
  animation_->SetDuration(kAnimationDuration);
  animation_->Start();
}

void RingProgressBar::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kProgressIndicator;
}

void RingProgressBar::OnPaint(gfx::Canvas* canvas) {
  gfx::Rect content_bounds = GetContentsBounds();
  // Draw the background ring that gets progressively filled.
  gfx::PointF center(content_bounds.width() / 2, content_bounds.height() / 2);
  const double radius =
      (std::min(content_bounds.width(), content_bounds.height()) -
       kStrokeWidth) /
      2;
  cc::PaintFlags background_flags;
  background_flags.setStyle(cc::PaintFlags::Style::kStroke_Style);
  background_flags.setAntiAlias(true);
  background_flags.setColor(kBackgroundColor);
  background_flags.setStrokeWidth(kStrokeWidth);
  canvas->DrawCircle(center, radius, std::move(background_flags));

  // Draw the filled portion of the circle.
  SkPath ring_path;
  SkRect bounds{/*fLeft=*/(content_bounds.width() / 2 - radius),
                /*fTop=*/(content_bounds.height() / 2 - radius),
                /*fRight=*/(content_bounds.width() / 2 + radius),
                /*fBottom=*/(content_bounds.height() / 2 + radius)};
  ring_path.addArc(
      std::move(bounds), /*startAngle=*/-90,
      /*sweepAngle=*/360 * animation_->CurrentValueBetween(initial_, target_));
  cc::PaintFlags ring_flags;
  ring_flags.setStyle(cc::PaintFlags::Style::kStroke_Style);
  ring_flags.setAntiAlias(true);
  ring_flags.setColor(kRingColor);
  ring_flags.setStrokeWidth(kStrokeWidth);
  canvas->DrawPath(std::move(ring_path), std::move(ring_flags));
}

void RingProgressBar::AnimationProgressed(const gfx::Animation* animation) {
  SchedulePaint();
}

BEGIN_METADATA(RingProgressBar, views::View)
END_METADATA
