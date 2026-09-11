// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/animations/organizer_panel_animations.h"

#include "chrome/browser/ui/animation/browser_animation_types.h"
#include "chrome/browser/ui/ui_features.h"
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

OrganizerPanelAnimations::OrganizerPanelAnimations() {
  // The collapsed state is the "home" state for most animations, so use that
  // as the default.
  SetSequenceParams(kOrganizerPanel, Default(kVisibleWidth, 0.0, true));

  SetHistogramName(kOrganizerPanel, "Projects.ProjectsPanel");

  // LINT.IfChange(OrganizerPanelAnimations)
  SetHistogramName(kShow, "Show");
  SetHistogramName(kHide, "Hide");
  // LINT.ThenChange(//tools/metrics/histograms/metadata/projects/histograms.xml:OrganizerPanelAnimations)
}

OrganizerPanelAnimations::~OrganizerPanelAnimations() = default;

OrganizerPanelAnimations::GroupInfos
OrganizerPanelAnimations::GenerateAnimations() const {
  static constexpr int kShowMs = 250;
  static constexpr int kHideMs = 200;
  constexpr auto kShowHideTween = gfx::Tween::Type::EASE_IN_OUT_EMPHASIZED;

  return Groups(
      Group(kOrganizerPanel,
            Motion(kShow, TotalDurationMs(kShowMs), kShowHideTween,
                   Animate(kVisibleWidth, FromValue(0.0), ToValue(1.0))),
            Motion(kHide, TotalDurationMs(kHideMs), kShowHideTween,
                   Animate(kVisibleWidth, FromValue(1.0), ToValue(0.0)))));
}
