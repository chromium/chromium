// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/ring_progress_bar.h"

#include <algorithm>

#include "cc/paint/paint_flags.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/progress_ring_utils.h"

namespace {
constexpr float kStrokeWidth = 4;
constexpr base::TimeDelta kAnimationDuration = base::Milliseconds(200);
}  // namespace

RingProgressBar::RingProgressBar() {
  GetViewAccessibility().SetRole(ax::mojom::Role::kProgressIndicator);
}

RingProgressBar::~RingProgressBar() = default;

void RingProgressBar::SetValue(double initial, double target) {
  initial_ = std::clamp(initial, 0., 1.);
  target_ = std::clamp(target, 0., 1.);
  animation_ = std::make_unique<gfx::LinearAnimation>(this);
  animation_->SetDuration(kAnimationDuration);
  animation_->Start();
}

void RingProgressBar::OnPaint(gfx::Canvas* canvas) {
  gfx::Rect content_bounds = GetContentsBounds();
  const float radius =
      (std::min(content_bounds.width(), content_bounds.height()) -
       kStrokeWidth) /
      2;

  SkRect bounds{/*fLeft=*/(content_bounds.width() / 2 - radius),
                /*fTop=*/(content_bounds.height() / 2 - radius),
                /*fRight=*/(content_bounds.width() / 2 + radius),
                /*fBottom=*/(content_bounds.height() / 2 + radius)};
  const auto* const color_provider = GetColorProvider();
  views::DrawProgressRing(
      canvas, bounds,
      color_provider->GetColor(kColorWebAuthnProgressRingBackground),
      color_provider->GetColor(kColorWebAuthnProgressRingForeground),
      kStrokeWidth,
      /*start_angle=*/-90,
      /*sweep_angle=*/360 * animation_->CurrentValueBetween(initial_, target_));
}

void RingProgressBar::AnimationProgressed(const gfx::Animation* animation) {
  SchedulePaint();
}

BEGIN_METADATA(RingProgressBar)
END_METADATA
