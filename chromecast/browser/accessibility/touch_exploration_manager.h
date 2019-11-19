// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copied with modifications from ash/accessibility, refactored for use in
// chromecast.

#ifndef CHROMECAST_BROWSER_ACCESSIBILITY_TOUCH_EXPLORATION_MANAGER_H_
#define CHROMECAST_BROWSER_ACCESSIBILITY_TOUCH_EXPLORATION_MANAGER_H_

#include <memory>

#include "chromecast/browser/accessibility/accessibility_sound_player.h"
#include "chromecast/browser/accessibility/touch_exploration_controller.h"
#include "chromecast/graphics/accessibility/accessibility_focus_ring_controller.h"
#include "ui/events/event_rewriter.h"
#include "ui/wm/public/activation_change_observer.h"
#include "ui/wm/public/activation_client.h"

namespace chromecast {

class CastGestureHandler;

namespace shell {

// Responsible for initializing TouchExplorationController when spoken feedback
// is on. Implements TouchExplorationControllerDelegate which allows touch
// gestures to manipulate the system.
class TouchExplorationManager : public ui::EventRewriter,
                                public TouchExplorationControllerDelegate,
                                public ::wm::ActivationChangeObserver {
 public:
  TouchExplorationManager(
      aura::Window* root_window,
      wm::ActivationClient* activation_client,
      AccessibilityFocusRingController* accessibility_focus_ring_controller,
      AccessibilitySoundPlayer* accessibility_sound_player,
      CastGestureHandler* cast_gesture_handler);
  ~TouchExplorationManager() override;

  // Enable or disable touch exploration.
  // (In the Chrome version this is handled as an AccessibilityObserver.)
  void Enable(bool enabled);

  // ui::EventRewriter overrides:
  ui::EventDispatchDetails RewriteEvent(
      const ui::Event& event,
      const Continuation continuation) override;

  // TouchExplorationControllerDelegate overrides:
  void HandleAccessibilityGesture(ax::mojom::Gesture gesture) override;
  void HandleTap(const gfx::Point touch_location) override;

  // wm::ActivationChangeObserver overrides:
  void OnWindowActivated(
      ::wm::ActivationChangeObserver::ActivationReason reason,
      aura::Window* gained_active,
      aura::Window* lost_active) override;

  // Update the touch exploration controller so that synthesized touch
  // events are anchored at this point.
  void SetTouchAccessibilityAnchorPoint(const gfx::Point& anchor_point);

 private:
  void UpdateTouchExplorationState();

  std::unique_ptr<TouchExplorationController> touch_exploration_controller_;
  bool touch_exploration_enabled_;

  // Not owned; must outlive TouchExplorationManager.
  aura::Window* root_window_;
  wm::ActivationClient* activation_client_;
  AccessibilityFocusRingController* accessibility_focus_ring_controller_;
  AccessibilitySoundPlayer* accessibility_sound_player_;
  CastGestureHandler* cast_gesture_handler_;

  DISALLOW_COPY_AND_ASSIGN(TouchExplorationManager);
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_ACCESSIBILITY_TOUCH_EXPLORATION_MANAGER_H_
