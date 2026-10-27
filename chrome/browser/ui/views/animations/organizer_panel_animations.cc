// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/animations/organizer_panel_animations.h"

#include "base/time/time.h"
#include "chrome/browser/ui/animation/browser_animation_types.h"
#include "chrome/browser/ui/views/animations/common_animation_values.h"
#include "ui/base/interaction/safe_castable.h"

DEFINE_SAFE_CAST_TARGET(OrganizerPanelAnimations)

DEFINE_CLASS_BROWSER_ANIMATION_GROUP(OrganizerPanelAnimations, kOrganizerPanel);

// Want to ensure every motion gets performance logging.
// LINT.IfChange(OrganizerPanelMotions)
DEFINE_CLASS_BROWSER_ANIMATION_MOTION(OrganizerPanelAnimations, kShowInTray);
DEFINE_CLASS_BROWSER_ANIMATION_MOTION(OrganizerPanelAnimations, kHideInTray);
DEFINE_CLASS_BROWSER_ANIMATION_MOTION(OrganizerPanelAnimations,
                                      kShowExpandFromVerticalTabs);
DEFINE_CLASS_BROWSER_ANIMATION_MOTION(OrganizerPanelAnimations,
                                      kHideExpandFromVerticalTabs);
DEFINE_CLASS_BROWSER_ANIMATION_MOTION(OrganizerPanelAnimations,
                                      kShowInVerticalTabs);
DEFINE_CLASS_BROWSER_ANIMATION_MOTION(OrganizerPanelAnimations,
                                      kHideInVerticalTabs);
// LINT.ThenChange(:OrganizerPanelAnimations)

DEFINE_CLASS_BROWSER_ANIMATION_SEQUENCE(OrganizerPanelAnimations,
                                        kVisibleWidth);
DEFINE_CLASS_BROWSER_ANIMATION_SEQUENCE(OrganizerPanelAnimations,
                                        kBackgroundOpacity);

// static
bool OrganizerPanelAnimations::IsShow(BrowserAnimationMotion motion) {
  if (motion == kShowInTray || motion == kShowExpandFromVerticalTabs ||
      motion == kShowInVerticalTabs) {
    return true;
  }
  CHECK(motion == kHideInTray || motion == kHideExpandFromVerticalTabs ||
        motion == kHideInVerticalTabs);
  return false;
}

OrganizerPanelAnimations::OrganizerPanelAnimations() {
  // The collapsed state is the "home" state for most animations, so use that
  // as the default.
  SetSequenceParams(kOrganizerPanel, Default(kVisibleWidth, 0.0, true),
                    Default(kBackgroundOpacity, 0.0, true));

  SetHistogramName(kOrganizerPanel, "Projects.ProjectsPanel");

  // LINT.IfChange(OrganizerPanelAnimations)
  SetHistogramName(kShowInTray, "Show");
  SetHistogramName(kHideInTray, "Hide");
  SetHistogramName(kShowExpandFromVerticalTabs, "Show");
  SetHistogramName(kHideExpandFromVerticalTabs, "Hide");
  SetHistogramName(kShowInVerticalTabs, "Show");
  SetHistogramName(kHideInVerticalTabs, "Hide");
  // LINT.ThenChange(//tools/metrics/histograms/metadata/projects/histograms.xml:OrganizerPanelAnimations)
}

OrganizerPanelAnimations::~OrganizerPanelAnimations() = default;

OrganizerPanelAnimations::GroupInfos
OrganizerPanelAnimations::GenerateAnimations() const {
  const auto show_motion = [](BrowserAnimationMotion motion, int ms,
                              gfx::Tween::Type tween, int fade_ms,
                              gfx::Tween::Type fade_tween) {
    return Motion(motion, TotalDurationMs(ms), tween,
                  Animate(kVisibleWidth, FromValue(0.0), ToValue(1.0)),
                  Sequence(kBackgroundOpacity, StartingValue(0.0),
                           Transition::kStartAtOldValue,
                           Segment(StartMs(0), EndMs(fade_ms), ToValue(1.0),
                                   fade_tween)));
  };

  const auto hide_motion = [](BrowserAnimationMotion motion, int ms,
                              gfx::Tween::Type tween, int fade_ms,
                              gfx::Tween::Type fade_tween) {
    return Motion(motion, TotalDurationMs(ms), tween,
                  Animate(kVisibleWidth, FromValue(1.0), ToValue(0.0)),
                  Sequence(kBackgroundOpacity, StartingValue(1.0),
                           Transition::kStartAtOldValue,
                           Segment(StartMs(ms - fade_ms), EndMs(ms),
                                   ToValue(0.0), fade_tween)));
  };

  const auto tray_tween = browser_animations::GetLongExpandTweenType();
  const auto vts_tween = browser_animations::kShortExpandTween;
  const auto fade_ms = browser_animations::kShortExpandFadeMs;
  const auto fade_in_tween = browser_animations::kShortExpandFadeInTween;
  const auto fade_out_tween = browser_animations::kShortExpandFadeOutTween;
  const int slide_ms = 250;
  const auto slide_tween = gfx::Tween::FAST_OUT_SLOW_IN;

  return Groups(
      Group(kOrganizerPanel,
            show_motion(kShowInTray, browser_animations::GetLongExpandShowMs(),
                        tray_tween, fade_ms, fade_in_tween),
            hide_motion(kHideInTray, browser_animations::GetLongExpandHideMs(),
                        tray_tween, fade_ms, fade_out_tween),
            show_motion(kShowExpandFromVerticalTabs,
                        browser_animations::kShortExpandShowMs, vts_tween,
                        fade_ms, fade_in_tween),
            hide_motion(kHideExpandFromVerticalTabs,
                        browser_animations::kShortExpandHideMs, vts_tween,
                        fade_ms, fade_out_tween),
            show_motion(kShowInVerticalTabs, slide_ms, slide_tween, slide_ms,
                        slide_tween),
            hide_motion(kHideInVerticalTabs, slide_ms, slide_tween, slide_ms,
                        slide_tween)));
}
