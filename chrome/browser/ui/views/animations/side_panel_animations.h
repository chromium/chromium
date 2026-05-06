// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ANIMATIONS_SIDE_PANEL_ANIMATIONS_H_
#define CHROME_BROWSER_UI_VIEWS_ANIMATIONS_SIDE_PANEL_ANIMATIONS_H_

#include "chrome/browser/ui/animation/browser_animation_provider.h"
#include "chrome/browser/ui/animation/browser_animation_types.h"
#include "ui/base/identifier/unique_identifier.h"
#include "ui/base/interaction/framework_specific_implementation.h"

class SidePanelAnimations : public CachingBrowserAnimationProvider {
 public:
  DECLARE_FRAMEWORK_SPECIFIC_METADATA()

  SidePanelAnimations();
  ~SidePanelAnimations() override;

  DECLARE_CLASS_BROWSER_ANIMATION_GROUP(kSidePanel);
  DECLARE_CLASS_BROWSER_ANIMATION_MOTION(kOpen);
  DECLARE_CLASS_BROWSER_ANIMATION_MOTION(kOpenWithContentTransition);
  DECLARE_CLASS_BROWSER_ANIMATION_MOTION(kClose);
  DECLARE_CLASS_BROWSER_ANIMATION_SEQUENCE(kPanelWidth);
  DECLARE_CLASS_BROWSER_ANIMATION_SEQUENCE(kMainAreaShadow);
  DECLARE_CLASS_BROWSER_ANIMATION_SEQUENCE(kContentTop);
  DECLARE_CLASS_BROWSER_ANIMATION_SEQUENCE(kContentBottom);
  DECLARE_CLASS_BROWSER_ANIMATION_SEQUENCE(kContentLeft);
  DECLARE_CLASS_BROWSER_ANIMATION_SEQUENCE(kContentWidth);

  // CachingBrowserAnimationProvider:
  GroupInfos GenerateAnimations() const override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_ANIMATIONS_SIDE_PANEL_ANIMATIONS_H_
