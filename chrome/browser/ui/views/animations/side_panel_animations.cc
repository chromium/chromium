// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/animations/side_panel_animations.h"

#include "chrome/browser/ui/ui_features.h"
#include "ui/base/interaction/safe_castable.h"
#include "ui/gfx/animation/tween.h"

DEFINE_SAFE_CAST_TARGET(SidePanelAnimations)

DEFINE_CLASS_BROWSER_ANIMATION_GROUP(SidePanelAnimations, kSidePanel);

// Any new motion that is added should also be set to record performance
// histograms below.
// LINT.IfChange(SidePanelMotions)
DEFINE_CLASS_BROWSER_ANIMATION_MOTION(SidePanelAnimations, kOpen);
DEFINE_CLASS_BROWSER_ANIMATION_MOTION(SidePanelAnimations,
                                      kOpenWithContentTransition);
DEFINE_CLASS_BROWSER_ANIMATION_MOTION(SidePanelAnimations, kClose);
// LINT.ThenChange(:SidePanelAnimationType)

DEFINE_CLASS_BROWSER_ANIMATION_SEQUENCE(SidePanelAnimations, kPanelWidth);
DEFINE_CLASS_BROWSER_ANIMATION_SEQUENCE(SidePanelAnimations, kMainAreaShadow);
DEFINE_CLASS_BROWSER_ANIMATION_SEQUENCE(SidePanelAnimations,
                                        kContentScrimOpacity);
DEFINE_CLASS_BROWSER_ANIMATION_SEQUENCE(SidePanelAnimations,
                                        kContentTransitionOffset);

SidePanelAnimations::SidePanelAnimations() {
  SetSequenceParams(kSidePanel, Persist(kPanelWidth), Persist(kMainAreaShadow));

  // Because we want to keep separate prefixes for content- vs. toolbar-height
  // panels (despite them using the same motions), instead of saving a single
  // histogram name here for `kSidePanel`, a histogram prefix will be specified
  // when starting the animation based on whether it's a true toolbar-height
  // side panel or not.

  // LINT.IfChange(SidePanelAnimationType)
  SetHistogramName(kClose, "Close");
  SetHistogramName(kOpen, "Open");
  SetHistogramName(kOpenWithContentTransition, "OpenWithContentTransition");
  // LINT.ThenChange(//tools/metrics/histograms/metadata/browser/histograms.xml:SidePanelAnimationType)
}

SidePanelAnimations::~SidePanelAnimations() = default;

SidePanelAnimations::GroupInfos SidePanelAnimations::GenerateAnimations()
    const {
  const int kDefaultAnimationMs = features::kSidePanelFlyoverDurationMs.Get();
  const bool use_flyover = features::UseSidePanelFlyoverAnimation();
  const gfx::Tween::Type tween = use_flyover
                                     ? gfx::Tween::Type::ACCEL_80_DECEL_20
                                     : gfx::Tween::Type::ACCEL_45_DECEL_88;
  const auto show_shadow_sequence =
      Sequence(kMainAreaShadow, StartingValue(0.0),
               Segment(StartMs(150), LengthMs(100), ToValue(1.0)));
  const auto hide_shadow_sequence =
      Sequence(kMainAreaShadow, StartingValue(1.0),
               Segment(StartMs(0), LengthMs(100), ToValue(0.0)));

  return Groups(
      Group(kSidePanel,
            Motion(kOpen, TotalDurationMs(kDefaultAnimationMs), tween,
                   Animate(kPanelWidth, FromValue(0.0), ToValue(1.0)),
                   show_shadow_sequence),
            Motion(kOpenWithContentTransition, TotalDurationMs(400),
                   gfx::Tween::Type::LINEAR,
                   Snap(kPanelWidth, FromValue(0.0), ToValue(1.0), AtMs(100)),
                   Sequence(kContentScrimOpacity, StartingValue(0.0),
                            Segment(StartMs(0), LengthMs(100), ToValue(1.0),
                                    gfx::Tween::Type::ACCEL_30_DECEL_20_85),
                            Segment(StartMs(150), LengthMs(100), ToValue(0.0),
                                    gfx::Tween::Type::ACCEL_5_70_DECEL_90)),
                   Sequence(kContentTransitionOffset, StartingValue(0.0),
                            Segment(StartMs(0), LengthMs(100), ToValue(-100.0),
                                    gfx::Tween::Type::ACCEL_30_DECEL_20_85),
                            Segment(StartMs(100), LengthMs(0), ToValue(125.0)),
                            Segment(StartMs(100), LengthMs(300), ToValue(0.0),
                                    gfx::Tween::Type::ACCEL_5_70_DECEL_90)),
                   show_shadow_sequence),
            Motion(kClose, TotalDurationMs(kDefaultAnimationMs), tween,
                   Animate(kPanelWidth, FromValue(1.0), ToValue(0.0)),
                   hide_shadow_sequence)));
}
