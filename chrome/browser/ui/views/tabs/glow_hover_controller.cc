// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/glow_hover_controller.h"

#include "ui/views/view.h"

// Amount to scale the opacity. The spec is in terms of a Sketch radial gradient
// from color A (#FFF, 0.9) at center to color B (#FFF, 0.0) at edge, with a
// gradient opacity of 0.5. So this premultiplies for a center opacity of 0.45.
static const double kSubtleOpacityScale = 0.45;
static const double kPronouncedOpacityScale = 1.0;

GlowHoverController::GlowHoverController(views::View* view)
    : AnimationDelegateViews(view),
      view_(view),
      animation_(this),
      opacity_scale_(kSubtleOpacityScale),
      subtle_opacity_scale_(kSubtleOpacityScale) {
  animation_.set_delegate(this);
}

GlowHoverController::~GlowHoverController() {}

void GlowHoverController::SetAnimationContainer(
    gfx::AnimationContainer* container) {
  animation_.SetContainer(container);
}

void GlowHoverController::SetLocation(const gfx::Point& location) {
  location_ = location;
  if (ShouldDraw())
    view_->SchedulePaint();
}

void GlowHoverController::SetSubtleOpacityScale(double opacity_scale) {
  subtle_opacity_scale_ = opacity_scale;
}

void GlowHoverController::Show(TabStyle::ShowHoverStyle style) {
  switch (style) {
    case TabStyle::ShowHoverStyle::kSubtle:
      opacity_scale_ = subtle_opacity_scale_;
      animation_.SetSlideDuration(base::TimeDelta::FromMilliseconds(200));
      animation_.SetTweenType(gfx::Tween::EASE_OUT);
      animation_.Show();
      break;
    case TabStyle::ShowHoverStyle::kPronounced:
      opacity_scale_ = kPronouncedOpacityScale;
      // Force the end state to show immediately.
      animation_.Show();
      animation_.End();
      break;
  }
}

void GlowHoverController::Hide(TabStyle::HideHoverStyle style) {
  switch (style) {
    case TabStyle::HideHoverStyle::kGradual:
      animation_.SetTweenType(gfx::Tween::EASE_IN);
      animation_.Hide();
      break;
    case TabStyle::HideHoverStyle::kImmediate:
      if (ShouldDraw())
        view_->SchedulePaint();
      animation_.Reset();
      break;
  }
}

double GlowHoverController::GetAnimationValue() const {
  return animation_.GetCurrentValue();
}

SkAlpha GlowHoverController::GetAlpha() const {
  return static_cast<SkAlpha>(animation_.CurrentValueBetween(
      0, gfx::ToRoundedInt(255 * opacity_scale_)));
}

bool GlowHoverController::ShouldDraw() const {
  return animation_.IsShowing() || animation_.is_animating();
}

void GlowHoverController::AnimationEnded(const gfx::Animation* animation) {
  view_->Layout();
}

void GlowHoverController::AnimationProgressed(const gfx::Animation* animation) {
  view_->SchedulePaint();
}
