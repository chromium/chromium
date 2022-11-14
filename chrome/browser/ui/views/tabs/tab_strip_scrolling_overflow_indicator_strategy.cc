// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_strip_scrolling_overflow_indicator_strategy.h"
#include "cc/paint/paint_shader.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/canvas.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/view_utils.h"

TabStripScrollingOverflowIndicatorStrategy::
    TabStripScrollingOverflowIndicatorStrategy(views::ScrollView* scroll_view,
                                               TabStrip* tab_strip)
    : scroll_view_(scroll_view), tab_strip_(tab_strip) {}

GradientIndicatorView::GradientIndicatorView(
    views::OverflowIndicatorAlignment side,
    int opaque_width,
    int shadow_opaque_width,
    int shadow_blur_width)
    : side_(side),
      opaque_width_(opaque_width),
      shadow_opaque_width_(shadow_opaque_width),
      shadow_blur_width_(shadow_blur_width) {
  DCHECK(side_ == views::OverflowIndicatorAlignment::kLeft ||
         side_ == views::OverflowIndicatorAlignment::kRight);
  opaque_width_ = opaque_width;
  shadow_opaque_width_ = shadow_opaque_width;
  shadow_blur_width_ = shadow_blur_width;
}

GradientIndicatorView::GradientIndicatorView(
    views::OverflowIndicatorAlignment side)
    : GradientIndicatorView(side,
                            kDefaultOpaqueWidth,
                            kDefaultShadowSpread,
                            kDefaultShadowBlur) {}

void GradientIndicatorView::OnPaint(gfx::Canvas* canvas) {
  // Mirror how the indicator is painted for the right vs left sides.
  SkPoint points[2];
  if (side_ == views::OverflowIndicatorAlignment::kLeft) {
    points[0].iset(GetContentsBounds().origin().x(), GetContentsBounds().y());
    points[1].iset(GetContentsBounds().right(), GetContentsBounds().y());
  } else {
    points[0].iset(GetContentsBounds().right(), GetContentsBounds().y());
    points[1].iset(GetContentsBounds().origin().x(), GetContentsBounds().y());
  }

  SkColor4f colors[5];
  SkScalar color_positions[5];
  // Paint an opaque region on the outside.
  colors[0] = frame_color_;
  colors[1] = frame_color_;
  color_positions[0] = 0;
  color_positions[1] = static_cast<float>(opaque_width_) / GetTotalWidth();

  // Paint a shadow-like gradient on the inside.
  colors[2] = shadow_color_;
  colors[3] = shadow_color_;
  colors[4] = shadow_color_;
  colors[4].fA = 0.0f;
  color_positions[2] = static_cast<float>(opaque_width_) / GetTotalWidth();
  color_positions[3] =
      static_cast<float>(opaque_width_ + shadow_opaque_width_) /
      GetTotalWidth();
  color_positions[4] = 1;

  cc::PaintFlags flags;
  flags.setShader(cc::PaintShader::MakeLinearGradient(
      points, colors, color_positions, 5, SkTileMode::kClamp));
  canvas->DrawRect(GetContentsBounds(), flags);
}

void GradientIndicatorView::SetShadowColor(SkColor4f new_shadow_color) {
  shadow_color_ = new_shadow_color;
  SchedulePaint();
}

void GradientIndicatorView::SetFrameColor(SkColor4f new_frame_color) {
  frame_color_ = new_frame_color;
  SchedulePaint();
}

BEGIN_METADATA(GradientIndicatorView, views::View)
END_METADATA

GradientOverflowIndicatorStrategy::GradientOverflowIndicatorStrategy(
    views::ScrollView* scroll_view,
    TabStrip* tab_strip)
    : TabStripScrollingOverflowIndicatorStrategy(scroll_view, tab_strip) {}

void GradientOverflowIndicatorStrategy::Init() {
  scroll_view()->SetDrawOverflowIndicator(true);

  std::unique_ptr<GradientIndicatorView> left_overflow_indicator =
      std::make_unique<GradientIndicatorView>(
          views::OverflowIndicatorAlignment::kLeft);
  left_overflow_indicator_ = left_overflow_indicator.get();

  std::unique_ptr<GradientIndicatorView> right_overflow_indicator =
      std::make_unique<GradientIndicatorView>(
          views::OverflowIndicatorAlignment::kRight);
  right_overflow_indicator_ = right_overflow_indicator.get();

  scroll_view()->SetCustomOverflowIndicator(
      views::OverflowIndicatorAlignment::kLeft,
      std::move(left_overflow_indicator),
      left_overflow_indicator_->GetTotalWidth(), false);

  scroll_view()->SetCustomOverflowIndicator(
      views::OverflowIndicatorAlignment::kRight,
      std::move(right_overflow_indicator),
      right_overflow_indicator_->GetTotalWidth(), false);
}

// Sometimes the views need to be redrawn to get color updates.

ShadowOverflowIndicatorStrategy::ShadowOverflowIndicatorStrategy(
    views::ScrollView* scroll_view,
    TabStrip* tab_strip)
    : GradientOverflowIndicatorStrategy(scroll_view, tab_strip) {}

void ShadowOverflowIndicatorStrategy::FrameColorsChanged() {
  SkColor4f frame_color =
      SkColor4f::FromColor(tab_strip()->controller()->GetFrameColor(
          BrowserFrameActiveState::kUseCurrent));
  SkColor4f shadow_color = SkColor4f::FromColor(
      tab_strip()->GetColorProvider()->GetColor(ui::kColorShadowBase));

  left_overflow_indicator()->SetFrameColor(frame_color);
  right_overflow_indicator()->SetFrameColor(frame_color);

  left_overflow_indicator()->SetShadowColor(shadow_color);
  right_overflow_indicator()->SetShadowColor(shadow_color);
}
