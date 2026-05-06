// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ANIMATIONS_TAB_STRIP_ANIMATIONS_H_
#define CHROME_BROWSER_UI_VIEWS_ANIMATIONS_TAB_STRIP_ANIMATIONS_H_

#include "chrome/browser/ui/animation/browser_animation_provider.h"
#include "chrome/browser/ui/animation/browser_animation_types.h"
#include "ui/base/identifier/unique_identifier.h"
#include "ui/base/interaction/framework_specific_implementation.h"

// Provides tab strip animations.
class TabStripAnimations : public CachingBrowserAnimationProvider {
 public:
  DECLARE_FRAMEWORK_SPECIFIC_METADATA()

  TabStripAnimations();
  ~TabStripAnimations() override;

  // Animations for the Vertical Tabstrip.
  DECLARE_CLASS_BROWSER_ANIMATION_GROUP(kVerticalTabStrip);

  // Vertical tabstrip can expand and collapse.
  DECLARE_CLASS_BROWSER_ANIMATION_MOTION(kExpand);
  DECLARE_CLASS_BROWSER_ANIMATION_MOTION(kCollapse);
  DECLARE_CLASS_BROWSER_ANIMATION_MOTION(kExpandOnHover);
  DECLARE_CLASS_BROWSER_ANIMATION_MOTION(kCollapseOnHover);

  // This always plays during expand/collapse.
  DECLARE_CLASS_BROWSER_ANIMATION_SEQUENCE(kTabStripWidth);

  // This is additional width used by the hover animations.
  DECLARE_CLASS_BROWSER_ANIMATION_SEQUENCE(kTabStripHoverWidth);

  // These only play during expand/collapse when the tabstrip slides under the
  // caption buttons.
  DECLARE_CLASS_BROWSER_ANIMATION_SEQUENCE(kTabStripTop);

  // Corners vary between -1 (inside corner) and +1 (outside corner).
  DECLARE_CLASS_BROWSER_ANIMATION_SEQUENCE(kTopCorner);
  DECLARE_CLASS_BROWSER_ANIMATION_SEQUENCE(kBottomCorner);

  // CachingBrowserAnimationProvider:
  GroupInfos GenerateAnimations() const override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_ANIMATIONS_TAB_STRIP_ANIMATIONS_H_
