// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/animations/tab_strip_animations.h"

#include "chrome/browser/ui/animation/browser_animation_types.h"
#include "chrome/browser/ui/views/animations/common_animation_values.h"
#include "ui/base/interaction/safe_castable.h"

DEFINE_SAFE_CAST_TARGET(TabStripAnimations)

DEFINE_CLASS_BROWSER_ANIMATION_GROUP(TabStripAnimations, kVerticalTabStrip);

// Want to ensure every motion gets performance logging.
// LINT.IfChange(TabStripMotions)
DEFINE_CLASS_BROWSER_ANIMATION_MOTION(TabStripAnimations, kExpand);
DEFINE_CLASS_BROWSER_ANIMATION_MOTION(TabStripAnimations, kCollapse);
DEFINE_CLASS_BROWSER_ANIMATION_MOTION(TabStripAnimations, kExpandOnHover);
DEFINE_CLASS_BROWSER_ANIMATION_MOTION(TabStripAnimations, kCollapseOnHover);
// LINT.ThenChange(:TabStripAnimation)

DEFINE_CLASS_BROWSER_ANIMATION_SEQUENCE(TabStripAnimations, kTabStripWidth);
DEFINE_CLASS_BROWSER_ANIMATION_SEQUENCE(TabStripAnimations,
                                        kTabStripHoverWidth);
DEFINE_CLASS_BROWSER_ANIMATION_SEQUENCE(TabStripAnimations,
                                        kTabStripHoverOpacity);
DEFINE_CLASS_BROWSER_ANIMATION_SEQUENCE(TabStripAnimations, kTabStripTop);
DEFINE_CLASS_BROWSER_ANIMATION_SEQUENCE(TabStripAnimations, kTopCorner);
DEFINE_CLASS_BROWSER_ANIMATION_SEQUENCE(TabStripAnimations, kBottomCorner);

TabStripAnimations::TabStripAnimations() {
  // The collapsed state is the "home" state for most animations, so use that
  // as the default.
  SetSequenceParams(kVerticalTabStrip, Default(kTabStripWidth, 0.0, true),
                    Default(kTabStripHoverWidth, 0.0, true),
                    Default(kTabStripHoverOpacity, 0.0, true),
                    Default(kTabStripTop, 1.0, true),
                    // Top corner default will be updated as needed by the
                    // layout, as it depends on other UI elements.
                    Default(kTopCorner, -1.0, true),
                    Default(kBottomCorner, 1.0, true));

  SetHistogramName(kVerticalTabStrip, "TabStrip.Vertical");

  // LINT.IfChange(TabStripAnimation)
  SetHistogramName(kExpand, "Expand");
  SetHistogramName(kCollapse, "Collapse");
  SetHistogramName(kExpandOnHover, "ExpandOnHover");
  SetHistogramName(kCollapseOnHover, "CollapseOnHover");
  // LINT.ThenChange(//tools/metrics/histograms/metadata/tab/histograms.xml:TabStripAnimation)
}

TabStripAnimations::~TabStripAnimations() = default;

TabStripAnimations::GroupInfos TabStripAnimations::GenerateAnimations() const {
  const int expand_duration_ms = browser_animations::GetLongExpandShowMs();
  const int collapse_duration_ms = browser_animations::GetLongExpandHideMs();
  const gfx::Tween::Type expand_collapse_tween =
      browser_animations::GetLongExpandTweenType();

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
             Sequence(kTopCorner, Transition::kCapAtOldValue,
                      Keyframe(AtPercent(0), Value(DefaultValue())),
                      Keyframe(AtPercent(kFirstCheckpoint),
                               Value(MaxOfDefaultAnd(0.0))),
                      Keyframe(AtPercent(kSecondCheckpoint),
                               Value(MaxOfDefaultAnd(0.0))),
                      Keyframe(AtPercent(1.0), Value(1.0))),
             Sequence(kTabStripTop,
                      Keyframe(AtPercent(kFirstCheckpoint), Value(1.0)),
                      Keyframe(AtPercent(kSecondCheckpoint), Value(0.0))),
             Sequence(kTabStripHoverOpacity, StartingValue(1.0),
                      Transition::kStartAtOldValue,
                      Segment(StartMs(expand_duration_ms -
                                      browser_animations::kShortExpandFadeMs),
                              EndMs(expand_duration_ms), ToValue(0.0),
                              browser_animations::kShortExpandFadeOutTween))),
      Motion(kCollapse, TotalDurationMs(collapse_duration_ms),
             expand_collapse_tween,
             Animate(kTabStripWidth, FromValue(1.0), ToValue(0.0)),
             Sequence(kTopCorner, Transition::kCapAtOldValue,
                      Keyframe(AtPercent(0), Value(1.0)),
                      Keyframe(AtPercent(kFirstCheckpoint),
                               Value(MaxOfDefaultAnd(0.0))),
                      Keyframe(AtPercent(kSecondCheckpoint),
                               Value(MaxOfDefaultAnd(0.0))),
                      Keyframe(AtPercent(1.0), Value(DefaultValue()))),
             Sequence(kTabStripTop,
                      Keyframe(AtPercent(kFirstCheckpoint), Value(0.0)),
                      Keyframe(AtPercent(kSecondCheckpoint), Value(1.0)))),

      // The expand and collapse hover animation doesn't shift contents during
      // the animation and so shares the same animation parameters across all
      // the supported platforms.
      Motion(kExpandOnHover,
             TotalDurationMs(browser_animations::kShortExpandShowMs),
             browser_animations::kShortExpandTween,
             Animate(kTabStripHoverWidth, FromValue(0.0), ToValue(1.0)),
             Animate(kTopCorner, FromValue(DefaultValue()), ToValue(-1.0)),
             Animate(kBottomCorner, FromValue(1.0), ToValue(-1.0)),
             Sequence(kTabStripHoverOpacity, StartingValue(0.0),
                      Transition::kStartAtOldValue,
                      Segment(StartMs(0),
                              EndMs(browser_animations::kShortExpandFadeMs),
                              ToValue(1.0),
                              browser_animations::kShortExpandFadeInTween))),
      Motion(kCollapseOnHover,
             TotalDurationMs(browser_animations::kShortExpandHideMs),
             browser_animations::kShortExpandTween,
             Animate(kTabStripHoverWidth, FromValue(1.0), ToValue(0.0)),
             Animate(kTopCorner, FromValue(-1.0), ToValue(DefaultValue())),
             Animate(kBottomCorner, FromValue(-1.0), ToValue(1.0)),
             Sequence(kTabStripHoverOpacity, StartingValue(1.0),
                      Transition::kStartAtOldValue,
                      Segment(StartMs(browser_animations::kShortExpandHideMs -
                                      browser_animations::kShortExpandFadeMs),
                              EndMs(browser_animations::kShortExpandHideMs),
                              ToValue(0.0),
                              browser_animations::kShortExpandFadeOutTween)))));
}
