// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_ACCESSIBILITY_ACCESSIBILITY_MANAGER_H_
#define CHROMECAST_BROWSER_ACCESSIBILITY_ACCESSIBILITY_MANAGER_H_

#include <memory>
#include <vector>

#include "chromecast/browser/accessibility/accessibility_sound_proxy.h"
#include "chromecast/browser/accessibility/touch_exploration_manager.h"
#include "chromecast/graphics/accessibility/accessibility_focus_ring_controller.h"
#include "chromecast/graphics/gestures/multiple_tap_detector.h"

namespace aura {
class WindowTreeHost;
}  // namespace aura

namespace chromecast {
namespace shell {

// Responsible for delegating chromecast browser process accessibility functions
// to the responsible party.
class AccessibilityManager : public MultipleTapDetectorDelegate {
 public:
  // Sets the focus ring color.
  virtual void SetFocusRingColor(SkColor color) = 0;

  // Resets the focus ring color back to the default.
  virtual void ResetFocusRingColor() = 0;

  // Draws a focus ring around the given set of rects in screen coordinates. Use
  // |focus_ring_behavior| to specify whether the focus ring should persist or
  // fade out.
  virtual void SetFocusRing(const std::vector<gfx::Rect>& rects_in_screen,
                            FocusRingBehavior focus_ring_behavior) = 0;

  // Hides focus ring on screen.
  virtual void HideFocusRing() = 0;

  // Draws a highlight at the given rects in screen coordinates. Rects may be
  // overlapping and will be merged into one layer. This looks similar to
  // selecting a region with the cursor, except it is drawn in the foreground
  // rather than behind a text layer.
  virtual void SetHighlights(const std::vector<gfx::Rect>& rects_in_screen,
                             SkColor color) = 0;

  // Hides highlight on screen.
  virtual void HideHighlights() = 0;

  // Enable or disable screen reader support, including touch exploration.
  virtual void SetScreenReader(bool enable) = 0;

  // Update the touch exploration controller so that synthesized
  // touch events are anchored at this point.
  virtual void SetTouchAccessibilityAnchorPoint(
      const gfx::Point& anchor_point) = 0;

  // Sets the bounds for virtual keyboard.
  virtual void SetVirtualKeyboardBounds(const gfx::Rect& rect) = 0;

  // Get the window tree host this AccessibilityManager was created with.
  virtual aura::WindowTreeHost* window_tree_host() const = 0;

  // Enable or disable the triple-tap gesture to turn on magnification.
  virtual void SetMagnificationGestureEnabled(bool enabled) = 0;

  // Returns whether the magnification gesture is currently enabled.
  virtual bool IsMagnificationGestureEnabled() const = 0;

  // Sets the player for earcons.
  virtual void SetAccessibilitySoundPlayer(
      std::unique_ptr<AccessibilitySoundPlayer> player) {}
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_ACCESSIBILITY_ACCESSIBILITY_MANAGER_H_
