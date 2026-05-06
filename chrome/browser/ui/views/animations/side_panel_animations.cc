// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/animations/side_panel_animations.h"

#include "base/time/time.h"
#include "chrome/browser/ui/ui_features.h"
#include "ui/base/identifier/unique_identifier.h"
#include "ui/base/interaction/framework_specific_implementation.h"
#include "ui/gfx/animation/tween.h"

DEFINE_FRAMEWORK_SPECIFIC_METADATA(SidePanelAnimations)

DEFINE_CLASS_BROWSER_ANIMATION_GROUP(SidePanelAnimations, kSidePanel);
DEFINE_CLASS_BROWSER_ANIMATION_MOTION(SidePanelAnimations, kOpen);
DEFINE_CLASS_BROWSER_ANIMATION_MOTION(SidePanelAnimations,
                                      kOpenWithContentTransition);
DEFINE_CLASS_BROWSER_ANIMATION_MOTION(SidePanelAnimations, kClose);
DEFINE_CLASS_BROWSER_ANIMATION_SEQUENCE(SidePanelAnimations, kPanelWidth);
DEFINE_CLASS_BROWSER_ANIMATION_SEQUENCE(SidePanelAnimations, kMainAreaShadow);
DEFINE_CLASS_BROWSER_ANIMATION_SEQUENCE(SidePanelAnimations, kContentTop);
DEFINE_CLASS_BROWSER_ANIMATION_SEQUENCE(SidePanelAnimations, kContentLeft);
DEFINE_CLASS_BROWSER_ANIMATION_SEQUENCE(SidePanelAnimations, kContentBottom);
DEFINE_CLASS_BROWSER_ANIMATION_SEQUENCE(SidePanelAnimations, kContentWidth);

SidePanelAnimations::SidePanelAnimations() {
  SetSequenceParams(kSidePanel, Persist(kPanelWidth), Persist(kMainAreaShadow));
}

SidePanelAnimations::~SidePanelAnimations() = default;

SidePanelAnimations::GroupInfos SidePanelAnimations::GenerateAnimations()
    const {
  const int kDefaultAnimationMs = features::kSidePanelFlyoverDurationMs.Get();
  const bool use_flyover = features::UseSidePanelFlyoverAnimation();
  const gfx::Tween::Type tween = use_flyover
                                     ? gfx::Tween::Type::ACCEL_30_DECEL_20_85
                                     : gfx::Tween::Type::ACCEL_45_DECEL_88;
  const gfx::Tween::Type content_transition_tween =
      gfx::Tween::Type::ACCEL_45_DECEL_88;

  const auto show_shadow_sequence =
      Sequence(kMainAreaShadow, StartingValue(0.0),
               Segment(StartMs(150), LengthMs(100), ToValue(1.0)));
  const auto hide_shadow_sequence =
      Sequence(kMainAreaShadow, StartingValue(1.0),
               Segment(StartMs(0), LengthMs(100), ToValue(0.0)));

  return Groups(Group(
      kSidePanel,
      Motion(kOpen, TotalDurationMs(kDefaultAnimationMs), tween,
             Animate(kPanelWidth, FromValue(0.0), ToValue(1.0)),
             show_shadow_sequence),
      Motion(kOpenWithContentTransition, TotalDurationMs(kDefaultAnimationMs),
             content_transition_tween,
             Snap(kPanelWidth, FromValue(0.0), ToValue(1.0), AtPercent(0.0)),
             Snap(kContentTop, FromValue(0.0), ToValue(1.0), AtPercent(0.0)),
             Snap(kContentBottom, FromValue(0.0), ToValue(1.0), AtPercent(0.0)),
             Animate(kContentLeft, FromValue(0.0), ToValue(1.0)),
             Sequence(kContentWidth, StartingValue(0.0),
                      Segment(StartMs(0), LengthMs(200), ToValue(1.0),
                              content_transition_tween)),
             show_shadow_sequence),
      Motion(kClose, TotalDurationMs(kDefaultAnimationMs), tween,
             Animate(kPanelWidth, FromValue(1.0), ToValue(0.0)),
             hide_shadow_sequence)));
}
