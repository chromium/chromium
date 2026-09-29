// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/animations/organizer_panel_animations.h"

#include "chrome/browser/ui/animation/browser_animation_types.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/animations/common_animation_values.h"
#include "ui/base/interaction/safe_castable.h"

DEFINE_SAFE_CAST_TARGET(OrganizerPanelAnimations)

DEFINE_CLASS_BROWSER_ANIMATION_GROUP(OrganizerPanelAnimations, kOrganizerPanel);

// Want to ensure every motion gets performance logging.
// LINT.IfChange(OrganizerPanelMotions)
DEFINE_CLASS_BROWSER_ANIMATION_MOTION(OrganizerPanelAnimations, kShow);
DEFINE_CLASS_BROWSER_ANIMATION_MOTION(OrganizerPanelAnimations, kHide);
// LINT.ThenChange(:OrganizerPanelAnimations)

DEFINE_CLASS_BROWSER_ANIMATION_SEQUENCE(OrganizerPanelAnimations,
                                        kVisibleWidth);
DEFINE_CLASS_BROWSER_ANIMATION_SEQUENCE(OrganizerPanelAnimations,
                                        kBackgroundOpacity);

OrganizerPanelAnimations::OrganizerPanelAnimations() {
  // The collapsed state is the "home" state for most animations, so use that
  // as the default.
  SetSequenceParams(kOrganizerPanel, Default(kVisibleWidth, 0.0, true),
                    Default(kBackgroundOpacity, 0.0, true));

  SetHistogramName(kOrganizerPanel, "Projects.ProjectsPanel");

  // LINT.IfChange(OrganizerPanelAnimations)
  SetHistogramName(kShow, "Show");
  SetHistogramName(kHide, "Hide");
  // LINT.ThenChange(//tools/metrics/histograms/metadata/projects/histograms.xml:OrganizerPanelAnimations)
}

OrganizerPanelAnimations::~OrganizerPanelAnimations() = default;

OrganizerPanelAnimations::GroupInfos
OrganizerPanelAnimations::GenerateAnimations() const {
  return Groups(Group(
      kOrganizerPanel,
      Motion(
          kShow, TotalDurationMs(browser_animations::kFlyoutShowMs),
          browser_animations::kFlyoutTween,
          Animate(kVisibleWidth, FromValue(0.0), ToValue(1.0)),
          Sequence(
              kBackgroundOpacity, StartingValue(0.0),
              Transition::kStartAtOldValue,
              Segment(StartMs(0), EndMs(browser_animations::kFlyoutFadeMs),
                      ToValue(1.0), browser_animations::kFlyoutFadeInTween))),
      Motion(kHide, TotalDurationMs(browser_animations::kFlyoutHideMs),
             browser_animations::kFlyoutTween,
             Animate(kVisibleWidth, FromValue(1.0), ToValue(0.0)),
             Sequence(
                 kBackgroundOpacity, StartingValue(1.0),
                 Transition::kStartAtOldValue,
                 Segment(StartMs(browser_animations::kFlyoutHideMs -
                                 browser_animations::kFlyoutFadeMs),
                         EndMs(browser_animations::kFlyoutHideMs), ToValue(0.0),
                         browser_animations::kFlyoutFadeOutTween)))));
}
