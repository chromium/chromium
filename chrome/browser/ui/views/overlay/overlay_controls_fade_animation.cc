// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/overlay/overlay_controls_fade_animation.h"

#include "ui/compositor/layer.h"
#include "ui/views/view.h"

namespace {

constexpr base::TimeDelta kDuration = base::Milliseconds(250);

}  // namespace

OverlayControlsFadeAnimation::OverlayControlsFadeAnimation(
    const std::vector<raw_ptr<views::View>>& controls,
    Type type)
    : gfx::LinearAnimation(kDuration, kDefaultFrameRate, nullptr),
      controls_(controls),
      type_(type) {}

OverlayControlsFadeAnimation::~OverlayControlsFadeAnimation() = default;

void OverlayControlsFadeAnimation::AnimateToState(double state) {
  const double opacity = (type_ == Type::kToShown) ? state : (1.0 - state);
  for (auto& control : controls_) {
    control->SetVisible(opacity != 0);
    control->layer()->SetOpacity(opacity);
  }
}
