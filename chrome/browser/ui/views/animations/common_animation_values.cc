// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/animations/common_animation_values.h"

#include "ui/gfx/animation/tween.h"

namespace browser_animations {

namespace {

bool UseFlyover() {
  return browser_animations::GetPanelAnimationType() ==
         browser_animations::PanelAnimationType::kClipAndSingleReflow;
}

}  // namespace

int GetLongExpandShowMs() {
  return UseFlyover() ? 300 : 400;
}

int GetLongExpandHideMs() {
  return UseFlyover() ? 250 : 350;
}

gfx::Tween::Type GetLongExpandTweenType() {
  return UseFlyover() ? gfx::Tween::ACCEL_80_DECEL_20
                      : gfx::Tween::EASE_IN_OUT_EMPHASIZED;
}

}  // namespace browser_animations
