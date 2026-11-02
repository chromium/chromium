// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/animations/focus_mode_animations.h"

#include "ui/base/interaction/safe_castable.h"
#include "ui/gfx/animation/tween.h"

namespace {

// Exit plays the enter motion in reverse on the time-reversed curve.
constexpr int kEnterMs = 300;
constexpr int kExitMs = 250;
constexpr gfx::Tween::Type kEnterTween = gfx::Tween::ACCEL_5_70_DECEL_90;
constexpr gfx::Tween::Type kExitTween = gfx::Tween::ACCEL_90_DECEL_5_70;

}  // namespace

DEFINE_SAFE_CAST_TARGET(FocusModeAnimations)

DEFINE_CLASS_BROWSER_ANIMATION_GROUP(FocusModeAnimations, kFocusMode);

DEFINE_CLASS_BROWSER_ANIMATION_MOTION(FocusModeAnimations, kEnter);
DEFINE_CLASS_BROWSER_ANIMATION_MOTION(FocusModeAnimations, kExit);

DEFINE_CLASS_BROWSER_ANIMATION_SEQUENCE(FocusModeAnimations, kProgress);
DEFINE_CLASS_BROWSER_ANIMATION_SEQUENCE(FocusModeAnimations,
                                        kUnrelatedTabsOpacity);
DEFINE_CLASS_BROWSER_ANIMATION_SEQUENCE(FocusModeAnimations,
                                        kUnfocusLabelOpacity);

FocusModeAnimations::FocusModeAnimations() {
  // Unfocused is the resting state. All sequences persist so that the current
  // values are always available between animations.
  SetSequenceParams(kFocusMode, Default(kProgress, 0.0, true),
                    Default(kUnrelatedTabsOpacity, 1.0, true),
                    Default(kUnfocusLabelOpacity, 0.0, true));
}

FocusModeAnimations::~FocusModeAnimations() = default;

FocusModeAnimations::GroupInfos FocusModeAnimations::GenerateAnimations()
    const {
  return Groups(
      Group(kFocusMode,
            Motion(kEnter, TotalDurationMs(kEnterMs), kEnterTween,
                   Animate(kProgress, FromValue(0.0), ToValue(1.0)),
                   // Unrelated tabs fade out front-loaded, gone by 100 ms.
                   Sequence(kUnrelatedTabsOpacity, StartingValue(1.0),
                            Segment(StartMs(0), EndMs(100), ToValue(0.0),
                                    gfx::Tween::EASE_OUT)),
                   Sequence(kUnfocusLabelOpacity, StartingValue(0.0),
                            Segment(StartMs(180), EndMs(220), ToValue(1.0),
                                    gfx::Tween::LINEAR))),
            // Exit plays the enter choreography in reverse, compressed from 300
            // ms to 250 ms, on the time-reversed enter curve: the Unfocus label
            // fades out first while the button is still near full size, and the
            // unrelated tabs fade back in last, so they are not visible while
            // still collapsed behind the focused tabs.
            Motion(kExit, TotalDurationMs(kExitMs), kExitTween,
                   Animate(kProgress, FromValue(1.0), ToValue(0.0)),
                   Sequence(kUnrelatedTabsOpacity, StartingValue(0.0),
                            Segment(StartMs(167), EndMs(250), ToValue(1.0),
                                    gfx::Tween::EASE_IN)),
                   Sequence(kUnfocusLabelOpacity, StartingValue(1.0),
                            Segment(StartMs(67), EndMs(100), ToValue(0.0),
                                    gfx::Tween::LINEAR)))));
}
