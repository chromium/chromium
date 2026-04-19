// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/animations/tab_strip_animations.h"

#include "base/time/time.h"
#include "chrome/browser/ui/animation/browser_animation_types.h"
#include "chrome/browser/ui/ui_features.h"
#include "ui/base/identifier/unique_identifier.h"

DEFINE_CLASS_BROWSER_ANIMATION_GROUP(TabStripAnimations, kVerticalTabStrip);
DEFINE_CLASS_BROWSER_ANIMATION_MOTION(TabStripAnimations, kExpand);
DEFINE_CLASS_BROWSER_ANIMATION_MOTION(TabStripAnimations, kCollapse);
DEFINE_CLASS_BROWSER_ANIMATION_MOTION(TabStripAnimations, kExpandOnHover);
DEFINE_CLASS_BROWSER_ANIMATION_MOTION(TabStripAnimations, kCollapseOnHover);
DEFINE_CLASS_BROWSER_ANIMATION_SEQUENCE(TabStripAnimations, kTabStripWidth);
DEFINE_CLASS_BROWSER_ANIMATION_SEQUENCE(TabStripAnimations,
                                        kTabStripHoverWidth);
DEFINE_CLASS_BROWSER_ANIMATION_SEQUENCE(TabStripAnimations, kTabStripTop);
DEFINE_CLASS_BROWSER_ANIMATION_SEQUENCE(TabStripAnimations, kTopCorner);
DEFINE_CLASS_BROWSER_ANIMATION_SEQUENCE(TabStripAnimations, kBottomCorner);

TabStripAnimations::GroupInfos TabStripAnimations::GenerateAnimations() const {
  const int expand_duration_ms =
      features::UseSidePanelFlyoverAnimation() ? 300 : 400;
  const int collapse_duration_ms =
      features::UseSidePanelFlyoverAnimation() ? 250 : 350;
  const gfx::Tween::Type expand_collapse_tween =
      features::UseSidePanelFlyoverAnimation()
          ? gfx::Tween::ACCEL_30_DECEL_20_85
          : gfx::Tween::EASE_IN_OUT_EMPHASIZED;
  // The sequence goes: one corner disappears, the top animates, and then the
  // other corner appears. These are the percentages of the animation at which
  // these changes happen.
  constexpr double kFirstCheckpoint = 0.25;
  constexpr double kSecondCheckpoint = 0.75;

  return Groups(Group(
      kVerticalTabStrip,
      Motion(kExpand, TotalDurationMs(expand_duration_ms),
             expand_collapse_tween,
             Animate(kTabStripWidth, FromValue(0.0), ToValue(1.0)),
             Sequence(kTopCorner, Keyframe(AtPercent(0), Value(-1.0)),
                      Keyframe(AtPercent(kFirstCheckpoint), Value(0.0)),
                      Keyframe(AtPercent(kSecondCheckpoint), Value(0.0)),
                      Keyframe(AtPercent(1.0), Value(1.0))),
             Sequence(kTabStripTop,
                      Keyframe(AtPercent(kFirstCheckpoint), Value(1.0)),
                      Keyframe(AtPercent(kSecondCheckpoint), Value(0.0))),
             // These are only used when transitioning from hovered to expanded;
             // the values are combined with the previous state to determine a
             // bound or starting point for the animation and should not be used
             // directly during this motion.
             Animate(kTabStripHoverWidth, FromValue(1.0), ToValue(0.0)),
             Animate(kBottomCorner, FromValue(-1.0), ToValue(1.0))),
      Motion(kCollapse, TotalDurationMs(collapse_duration_ms),
             expand_collapse_tween,
             Animate(kTabStripWidth, FromValue(1.0), ToValue(0.0)),
             Sequence(kTopCorner, Keyframe(AtPercent(0), Value(1.0)),
                      Keyframe(AtPercent(kFirstCheckpoint), Value(0.0)),
                      Keyframe(AtPercent(kSecondCheckpoint), Value(0.0)),
                      Keyframe(AtPercent(1.0), Value(-1.0))),
             Sequence(kTabStripTop,
                      Keyframe(AtPercent(kFirstCheckpoint), Value(0.0)),
                      Keyframe(AtPercent(kSecondCheckpoint), Value(1.0)))),

      // The expand and collapse hover animation doesn't shift contents during
      // the animation and so shares the same animation parameters across all
      // the supported platforms.
      Motion(kExpandOnHover, TotalDurationMs(250),
             gfx::Tween::EASE_IN_OUT_EMPHASIZED,
             Animate(kTabStripHoverWidth, FromValue(0.0), ToValue(1.0)),
             Animate(kTopCorner, FromValue(1.0), ToValue(-1.0)),
             Animate(kBottomCorner, FromValue(1.0), ToValue(-1.0))),
      Motion(kCollapseOnHover, TotalDurationMs(200),
             gfx::Tween::EASE_IN_OUT_EMPHASIZED,
             Animate(kTabStripHoverWidth, FromValue(1.0), ToValue(0.0)),
             Animate(kTopCorner, FromValue(-1.0), ToValue(1.0)),
             Animate(kBottomCorner, FromValue(-1.0), ToValue(1.0)))));
}
