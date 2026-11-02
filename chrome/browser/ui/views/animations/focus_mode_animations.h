// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ANIMATIONS_FOCUS_MODE_ANIMATIONS_H_
#define CHROME_BROWSER_UI_VIEWS_ANIMATIONS_FOCUS_MODE_ANIMATIONS_H_

#include "chrome/browser/ui/animation/browser_animation_provider.h"
#include "chrome/browser/ui/animation/browser_animation_types.h"
#include "ui/base/identifier/unique_identifier.h"
#include "ui/base/interaction/safe_castable.h"

// Provides tab strip focus mode animations. These are shared by group and
// non-group focus.
class FocusModeAnimations : public CachingBrowserAnimationProvider {
 public:
  DECLARE_SAFE_CAST_TARGET()

  FocusModeAnimations();
  ~FocusModeAnimations() override;

  // Animations for tab strip focus mode.
  DECLARE_CLASS_BROWSER_ANIMATION_GROUP(kFocusMode);

  // Focus mode can enter and exit.
  DECLARE_CLASS_BROWSER_ANIMATION_MOTION(kEnter);
  DECLARE_CLASS_BROWSER_ANIMATION_MOTION(kExit);

  // Overall progress of the focus transition (0.0 unfocused -> 1.0 focused).
  DECLARE_CLASS_BROWSER_ANIMATION_SEQUENCE(kProgress);

  // Opacity of tabs and groups outside the focused content.
  DECLARE_CLASS_BROWSER_ANIMATION_SEQUENCE(kUnrelatedTabsOpacity);

  // Opacity of the Unfocus button's text label.
  DECLARE_CLASS_BROWSER_ANIMATION_SEQUENCE(kUnfocusLabelOpacity);

  // CachingBrowserAnimationProvider:
  GroupInfos GenerateAnimations() const override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_ANIMATIONS_FOCUS_MODE_ANIMATIONS_H_
