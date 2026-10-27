// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ANIMATIONS_COMMON_ANIMATION_VALUES_H_
#define CHROME_BROWSER_UI_VIEWS_ANIMATIONS_COMMON_ANIMATION_VALUES_H_

#include "build/build_config.h"
#include "ui/gfx/animation/tween.h"

namespace browser_animations {

// Defines how browser panels (such as side panels and the vertical tab strip)
// animate when resizing the main content area.
enum class PanelAnimationType {
  // Web contents are laid out once at the target size and clipped during the
  // animation.
  kClipAndSingleReflow,
  // Web contents bounds are continuously resized on each animation frame.
  kContinuousFrameResize,
  // Web contents bounds are continuously resized on each animation frame with
  // surface synchronization when the content origin changes.
  kContinuousFrameResizeWithSurfaceSync,
};

constexpr PanelAnimationType GetPanelAnimationType() {
#if BUILDFLAG(IS_MAC)
#if defined(ARCH_CPU_ARM64)
  return PanelAnimationType::kContinuousFrameResizeWithSurfaceSync;
#else
  return PanelAnimationType::kContinuousFrameResize;
#endif
#else
  return PanelAnimationType::kClipAndSingleReflow;
#endif
}

// Common parameters for panels which do a short bump-out over the browser's
// content area or an internal slide transition (vertical tab strip
// expand-on-hover, organizer panel-in-vertical-tabs, etc.)
inline constexpr int kShortExpandShowMs = 250;
inline constexpr int kShortExpandHideMs = 200;
inline constexpr gfx::Tween::Type kShortExpandTween =
    gfx::Tween::EASE_IN_OUT_EMPHASIZED;
inline constexpr int kShortExpandFadeMs = 100;
inline constexpr gfx::Tween::Type kShortExpandFadeInTween =
    gfx::Tween::EASE_OUT;
inline constexpr gfx::Tween::Type kShortExpandFadeOutTween =
    gfx::Tween::FAST_OUT_LINEAR_IN;

// Common parameters for panels which expand out from at or near the edge of the
// window, such as organizer panel in horizontal tab strip mode and vertical tab
// strip expand/collapse.
int GetLongExpandShowMs();
int GetLongExpandHideMs();
gfx::Tween::Type GetLongExpandTweenType();

}  // namespace browser_animations

#endif  // CHROME_BROWSER_UI_VIEWS_ANIMATIONS_COMMON_ANIMATION_VALUES_H_
