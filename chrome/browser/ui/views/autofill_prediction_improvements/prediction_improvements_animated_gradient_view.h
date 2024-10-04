// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_PREDICTION_IMPROVEMENTS_PREDICTION_IMPROVEMENTS_ANIMATED_GRADIENT_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_PREDICTION_IMPROVEMENTS_PREDICTION_IMPROVEMENTS_ANIMATED_GRADIENT_VIEW_H_

#include "cc/paint/paint_shader.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/color/color_id.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/multi_animation.h"
#include "ui/views/layout/box_layout_view.h"

namespace autofill_prediction_improvements {

// This view generates an animated, repeating linear gradient at an angle across
// its rectangle. In its `OnPaint()` method only path `mask_` will be drawn.
// I.e. with the following diagram showing the view's rectangle with
// size `kWidth` and `kHeight`,
//              `kWidth`
// +-------------------------------+
// |  __________________________   |
// | (________`kRectTop`________)  | `kHeight`
// |  _________________            |
// | (__`kRectBottom`__)           |
// +-------------------------------+
// bars `kRectTop` and `kRectBottom` will be visible in the UI, showing the
// animated gradient "washing" over them.
class PredictionImprovementsAnimatedGradientView
    : public gfx::AnimationDelegate,
      public views::BoxLayoutView {
  METADATA_HEADER(PredictionImprovementsAnimatedGradientView,
                  views::BoxLayoutView)

 public:
  PredictionImprovementsAnimatedGradientView();
  ~PredictionImprovementsAnimatedGradientView() override;

  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override;

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;

#if defined(UNIT_TEST)
  bool IsAnimatingForTest() const { return animation_.is_animating(); }
  void SetAnimationDelegateForTest(gfx::AnimationDelegate* delegate) {
    animation_.set_delegate(delegate);
  }
#endif

 private:
  // Creates the `cc::PaintShader` instance for the `current_animation_state_`.
  // That means that the x-components of start and end points respectively are
  // shifted by `current_animation_state_` * `overflow_point_x_value_` (see
  // `CalculateGradientEndPoint()`).
  // The returned paint shader will draw a 4-stops, repeated gradient. The 4th
  // stop is added only to make the gradient look seamless when moved in
  // x-direction. The 0% and 100% stops have the same color.
  sk_sp<cc::PaintShader> CreateGradientForCurrentAnimationState();

  // Default gradient colors.
  static constexpr SkColor kGradientMiddleColorDefault =
      SkColorSetARGB(255, 211, 227, 253);
  static constexpr SkColor kGradientEndColorDefault =
      SkColorSetARGB(255, 231, 248, 237);
  // Parameters for `cc::PaintShader::MakeLinearGradient()`.
  static constexpr int kNoGradientStops = 4;
  static constexpr SkScalar kGradientPositions[kNoGradientStops] = {
      0.0f, 1.0f / 3, 2.0f / 3, 1.0f};

  // Creates the start and end points for the gradient. They are shifted from
  // their original states proportional to the `current_animation_state` in
  // x-direction.
  std::array<SkPoint, 2> CalculateGradientStartAndEndPoints();

  // Calculates and returns the "end point" of a linear gradient to be used in
  // `cc::PaintShader::MakeLinearGradient()`. With this approach a rectangle
  // will be covered precisely by the gradient from its top left corner `tl` to
  // the bottom right corner `br`, while preserving the angle 0 rad <
  // `kGradientAngleInRad` < rad(90°) of the gradient.
  //
  // Notes about the diagram below:
  // - IMPORTANT: P is perpendicular to the line from `tl` to `e`. So, angle
  // tl-e-br has 90 degrees.
  // - P is parallel to P'.
  // - "w" is the width, "h" is the height of the rectangle `rect_`.
  // - "a" is `kGradientAngleInRad`.
  // - The "overflow point" o is only relevant for
  // `CalculateOverflowPointXValue()`.
  //
  //     / P
  // tl /       w
  //   + ------------+ - + o
  //  /|\            |h /
  // / |a\           | / P'
  //   |  \          |/
  //   +---+---------+ br
  //        \      a/
  //         \     /
  //          \   /
  //           \ /
  //            + e
  gfx::Vector2d CalculateGradientEndPoint();

  // Calculates and returns the x value of the "overflow point" (see diagram
  // above).
  float CalculateOverflowPointXValue();

  // `gradient_colors` is initialized with default colors that will be
  // overwritten, if possible, on first paint.
  std::array<SkColor4f, 4> gradient_colors_ = {
      SkColors::kWhite, SkColor4f::FromColor(kGradientMiddleColorDefault),
      SkColor4f::FromColor(kGradientEndColorDefault),
      // To ensure a smooth transition at the beginning of the repeating
      // gradient, an extra stop with the first color is added outside the
      // rectangle. This prevents abrupt color changes when the gradient is
      // animated. The math in `CalculateGradientEndPoint()` and
      // `CalculateOverflowPointXValue()` is aimed at 3 stops and then adjusted
      // to 4 stops (by multiplying by 1.5).
      SkColors::kWhite};

  // Corner radius of the rectangles painted in this view.
  const int corner_radius_;

  // The gradient end point offset. For `current_animation_state_ == 0.0` this
  // is equal to the end point of the gradient. In other cases its x component
  // is shifted by `current_animation_state_ * overflow_point_x_value_`.
  const gfx::Vector2d end_point_offset_;

  // See `CalculateOverflowPointXValue()`.
  const float overflow_point_x_value_;

  // The `animation_` is parametrized in the constructor as a linear, repeating
  // animation.
  gfx::MultiAnimation animation_;

  // The current animation state has values between 0.0 and 1.0.
  double current_animation_state_ = 0;

  // Will be set to `false` on first paint.
  bool is_first_paint_ = true;
};

}  // namespace autofill_prediction_improvements

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_PREDICTION_IMPROVEMENTS_PREDICTION_IMPROVEMENTS_ANIMATED_GRADIENT_VIEW_H_
