// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill_prediction_improvements/prediction_improvements_animated_gradient_view.h"

#include "base/numerics/angle_conversions.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_manager.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/background.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace autofill_prediction_improvements {

namespace {

// The angle of the linear gradient.
constexpr float kGradientAngleInRad = base::DegToRad(45.0f);
// Helper angle.
constexpr float k90DegInRad = base::DegToRad(90.0f);
// Top rectangle showing the animated gradient.
constexpr float kRectTopWidth = 196;
constexpr float kRectHeight = 14;
constexpr gfx::RectF kRectTop(0.0f, 0.0f, kRectTopWidth, kRectHeight);
// Bottom rectangle showing the animated gradient.
constexpr float kGapBetweenRects = 8;
constexpr float kRectBottomWidth = 148;
constexpr float kRectBottomDistanceToTop = kRectHeight + kGapBetweenRects;
constexpr gfx::RectF kRectBottom(0.0f,
                                 kRectBottomDistanceToTop,
                                 kRectBottomWidth,
                                 kRectHeight);
// Width of `PredictionImprovementsAnimatedGradientView`.
constexpr int kWidth = kRectTop.right();
// Height of `PredictionImprovementsAnimatedGradientView`.
constexpr int kHeight = kRectBottom.bottom();

// Duration of one animation cycle.
constexpr base::TimeDelta kAnimationDuration = base::Seconds(3);
// Multiplier to extend the length of the gradient from 3-stops to 4-stops.
// Explanation: We want a 3-stops gradient to precisely cover a rectangle. At
// each gradient stop, the respective color assigned to it is fully opaque. E.g.
// consider a gradient defined by
// - start stop "s" at position 0% (the beginning of the gradient)
// - middle stop "m" at position 50%
// - end stop "e" at position 100% (the end of the gradient).
// The color of "s" is fully opaque at position 0% and becomes fully transparent
// at position 50%. While the "m" color is fully transparent at position 0% and
// becomes fully opaque at position 50%. The transitions between position 0% and
// position 50% are smooth and linear (because a linear gradient is used). Same
// applies for transitions from "m" to "e".
// Now we want to animate the gradient, i.e. move it along the x-axis. In order
// for the gradient to still cover the whole rectangle when moved, it needs to
// be repeated. With the above described approach however the transitions
// between gradient repetitions aren't smooth:
//
//   ... s m es m e ...
//   ... |X|X||X|X| ...
//   Figure 1: Two repetitions of the above described 3-stops gradient. Further
//   repetitions are hinted by dots. Stop "e" of the left gradient instance is
//   right next to stop "s" of the right gradient instance. At both those stops
//   their respetive, unequal colors are fully opaque which violates the goal of
//   having smooth transitions everywhere.
//
// To solve that problem, a 4th stop, with the color of the 1st stop, is added
// such that the stops are still equally distributed:
//
//   s m e s
//   |X|X|X|
//   Figure 2: Visualization of the 4-stops gradient.
//
// Transitions between repetitions of the 4-stops gradient are smooth:
//
//   ... s m e ss m e s ...
//   ... |X|X|X||X|X|X| ...
//   Figure 3: Two repetitions of the 4-stops gradient. Further repetitions are
//   hinted by dots. The colors of first and last stops of each gradient
//   repetition now coincide, resulting in smooth transitions everywhere.
//
// With the constraint to cover the whole rectangle with the first three stops,
// the length of the 4-stops gradient simply is 50% longer than the length of
// the 3-stops gradient. So we can calculate the length of the gradient as if it
// only had 3 stops and then multiply that length by 1.5.
constexpr float kGradientLengthMultiplier = 1.5;

}  // namespace

PredictionImprovementsAnimatedGradientView::
    PredictionImprovementsAnimatedGradientView()
    : corner_radius_(ChromeLayoutProvider::Get()->GetCornerRadiusMetric(
          views::Emphasis::kLow)),
      end_point_offset_(CalculateGradientEndPoint()),
      overflow_point_x_value_(CalculateOverflowPointXValue()),
      animation_{{gfx::MultiAnimation::Part{kAnimationDuration,
                                            gfx::Tween::Type::LINEAR}}} {
  SetPaintToLayer();
  SetFocusBehavior(FocusBehavior::NEVER);
  SetSize(gfx::Size(kWidth, kHeight));
  SetPreferredSize(size());
  SetBackground(
      views::CreateThemedSolidBackground(ui::kColorDropdownBackground));

  animation_.set_delegate(this);
  animation_.Start();
}

PredictionImprovementsAnimatedGradientView::
    ~PredictionImprovementsAnimatedGradientView() {
  animation_.Stop();
  if (views::Widget* widget = GetWidget()) {
    widget->Close();
  }
}

void PredictionImprovementsAnimatedGradientView::AnimationProgressed(
    const gfx::Animation* animation) {
  current_animation_state_ = animation->GetCurrentValue();
  OnPropertyChanged(&current_animation_state_, views::kPropertyEffectsPaint);
}

void PredictionImprovementsAnimatedGradientView::OnPaint(gfx::Canvas* canvas) {
  views::View::OnPaintBackground(canvas);

  if (is_first_paint_) {
    // Set gradient colors on first paint to ensure that the `ui::ColorProvider`
    // instance is available.
    if (ui::ColorProvider* color_provider = GetColorProvider()) {
      const SkColor4f start_and_end_color = SkColor4f::FromColor(
          color_provider->GetColor(ui::kColorDropdownBackground));
      gradient_colors_ = {start_and_end_color,
                          SkColor4f::FromColor(color_provider->GetColor(
                              ui::kColorSysGradientTertiary)),
                          SkColor4f::FromColor(color_provider->GetColor(
                              ui::kColorSysGradientPrimary)),
                          start_and_end_color};
    }
    is_first_paint_ = false;
  }

  canvas->Save();
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setShader(CreateGradientForCurrentAnimationState());
  canvas->DrawRoundRect(kRectTop, corner_radius_, flags);
  canvas->DrawRoundRect(kRectBottom, corner_radius_, flags);
  canvas->Restore();
}

sk_sp<cc::PaintShader> PredictionImprovementsAnimatedGradientView::
    CreateGradientForCurrentAnimationState() {
  std::array<SkPoint, 2> points = CalculateGradientStartAndEndPoints();
  return cc::PaintShader::MakeLinearGradient(
      points.data(), gradient_colors_.data(), kGradientPositions,
      kNoGradientStops, SkTileMode::kRepeat);
}

std::array<SkPoint, 2> PredictionImprovementsAnimatedGradientView::
    CalculateGradientStartAndEndPoints() {
  // The `start` point should be the top-left corner of the view's rectangle,
  // shifted by the `current_animation_state_`.
  gfx::Point start = bounds().origin();
  start.Offset(
      SkScalarRoundToInt(overflow_point_x_value_ * current_animation_state_),
      0);
  // The `end` point should follow the same x-directional shift as `start`.
  gfx::Point end = start;
  end -= end_point_offset_;
  return {gfx::PointToSkPoint(start), gfx::PointToSkPoint(end)};
}

gfx::Vector2d
PredictionImprovementsAnimatedGradientView::CalculateGradientEndPoint() {
  const float cos_a = std::cos(kGradientAngleInRad);
  const float sin_a = std::sin(kGradientAngleInRad);
  const float e_length = kHeight * cos_a + kWidth * sin_a;
  const float e_length_4_stops = kGradientLengthMultiplier * e_length;
  return {SkScalarRoundToInt(e_length_4_stops * cos_a),
          SkScalarRoundToInt(e_length_4_stops * sin_a)};
}

float PredictionImprovementsAnimatedGradientView::
    CalculateOverflowPointXValue() {
  return bounds().origin().x() +
         kGradientLengthMultiplier *
             (kWidth + kHeight * std::tan(k90DegInRad - kGradientAngleInRad));
}

BEGIN_METADATA(PredictionImprovementsAnimatedGradientView)
END_METADATA

}  // namespace autofill_prediction_improvements
