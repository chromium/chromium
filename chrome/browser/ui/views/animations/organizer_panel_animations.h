// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ANIMATIONS_ORGANIZER_PANEL_ANIMATIONS_H_
#define CHROME_BROWSER_UI_VIEWS_ANIMATIONS_ORGANIZER_PANEL_ANIMATIONS_H_

#include "chrome/browser/ui/animation/browser_animation_provider.h"
#include "chrome/browser/ui/animation/browser_animation_types.h"
#include "ui/base/identifier/unique_identifier.h"
#include "ui/base/interaction/safe_castable.h"

// Provides organizer panel animations.
class OrganizerPanelAnimations : public CachingBrowserAnimationProvider {
 public:
  DECLARE_SAFE_CAST_TARGET()

  OrganizerPanelAnimations();
  ~OrganizerPanelAnimations() override;

  // Animations for the Organizer Panel.
  DECLARE_CLASS_BROWSER_ANIMATION_GROUP(kOrganizerPanel);

  // Panel can show and hide.
  DECLARE_CLASS_BROWSER_ANIMATION_MOTION(kShow);
  DECLARE_CLASS_BROWSER_ANIMATION_MOTION(kHide);

  // The percentage of the panel that should be visible.
  DECLARE_CLASS_BROWSER_ANIMATION_SEQUENCE(kVisibleWidth);

  // CachingBrowserAnimationProvider:
  GroupInfos GenerateAnimations() const override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_ANIMATIONS_ORGANIZER_PANEL_ANIMATIONS_H_
