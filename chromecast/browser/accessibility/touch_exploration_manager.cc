// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copied with modifications from ash/accessibility, refactored for use in
// chromecast.

#include "chromecast/browser/accessibility/touch_exploration_manager.h"

#include <utility>
#include <vector>

#include "chromecast/browser/cast_browser_context.h"
#include "chromecast/browser/cast_browser_process.h"
#include "chromecast/common/extensions_api/accessibility_private.h"
#include "chromecast/graphics/cast_focus_client_aura.h"
#include "chromecast/graphics/gestures/cast_gesture_handler.h"
#include "extensions/browser/event_router.h"
#include "ui/accessibility/ax_enum_util.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window_tree_host.h"
#include "ui/display/screen.h"

namespace chromecast {
namespace shell {

TouchExplorationManager::TouchExplorationManager(
    aura::Window* root_window,
    wm::ActivationClient* activation_client,
    AccessibilityFocusRingController* accessibility_focus_ring_controller,
    AccessibilitySoundPlayer* accessibility_sound_player,
    CastGestureHandler* cast_gesture_handler)
    : touch_exploration_enabled_(false),
      root_window_(root_window),
      activation_client_(activation_client),
      accessibility_focus_ring_controller_(accessibility_focus_ring_controller),
      accessibility_sound_player_(accessibility_sound_player),
      cast_gesture_handler_(cast_gesture_handler) {
  DCHECK(root_window);
  root_window->GetHost()->GetEventSource()->AddEventRewriter(this);
  UpdateTouchExplorationState();
}

TouchExplorationManager::~TouchExplorationManager() {
  root_window_->GetHost()->GetEventSource()->RemoveEventRewriter(this);
}

void TouchExplorationManager::Enable(bool enabled) {
  touch_exploration_enabled_ = enabled;
  UpdateTouchExplorationState();
}

ui::EventDispatchDetails TouchExplorationManager::RewriteEvent(
    const ui::Event& event,
    const Continuation continuation) {
  return touch_exploration_controller_
             ? touch_exploration_controller_->RewriteEvent(event, continuation)
             : SendEvent(continuation, &event);
}

void TouchExplorationManager::HandleAccessibilityGesture(
    ax::mojom::Gesture gesture) {
  // (Code copied from Chrome's
  // AccessibilityController::HandleAccessibilityGestore.)
  extensions::EventRouter* event_router = extensions::EventRouter::Get(
      shell::CastBrowserProcess::GetInstance()->browser_context());
  std::unique_ptr<base::ListValue> event_args =
      std::make_unique<base::ListValue>();
  event_args->AppendString(ui::ToString(gesture));
  std::unique_ptr<extensions::Event> event(new extensions::Event(
      extensions::events::ACCESSIBILITY_PRIVATE_ON_ACCESSIBILITY_GESTURE,
      extensions::cast::api::accessibility_private::OnAccessibilityGesture::
          kEventName,
      std::move(event_args)));
  event_router->DispatchEventWithLazyListener(
      extension_misc::kChromeVoxExtensionId, std::move(event));
}

void TouchExplorationManager::HandleTap(const gfx::Point touch_location) {
  cast_gesture_handler_->HandleTapDownGesture(touch_location);
  cast_gesture_handler_->HandleTapGesture(touch_location);
}

void TouchExplorationManager::OnWindowActivated(
    ::wm::ActivationChangeObserver::ActivationReason reason,
    aura::Window* gained_active,
    aura::Window* lost_active) {
  UpdateTouchExplorationState();
}

void TouchExplorationManager::SetTouchAccessibilityAnchorPoint(
    const gfx::Point& anchor_point) {
  if (touch_exploration_controller_) {
    touch_exploration_controller_->SetTouchAccessibilityAnchorPoint(
        anchor_point);
  }
}

void TouchExplorationManager::UpdateTouchExplorationState() {
  // See https://crbug.com/603745 for more details.
  aura::Window* active_window = activation_client_->GetActiveWindow();
  const bool pass_through_surface =
      active_window &&
      active_window->GetProperty(
          aura::client::kAccessibilityTouchExplorationPassThrough);

  if (touch_exploration_enabled_) {
    if (!touch_exploration_controller_.get()) {
      touch_exploration_controller_ =
          std::make_unique<TouchExplorationController>(root_window_, this,
              accessibility_sound_player_);
    }
    if (pass_through_surface) {
      const display::Display display =
          display::Screen::GetScreen()->GetDisplayNearestWindow(root_window_);
      const gfx::Rect work_area = display.work_area();
      touch_exploration_controller_->SetExcludeBounds(work_area);
      // Clear the focus highlight.
      accessibility_focus_ring_controller_->SetFocusRing(
          std::vector<gfx::Rect>(),
          FocusRingBehavior::PERSIST_FOCUS_RING);
    } else {
      touch_exploration_controller_->SetExcludeBounds(gfx::Rect());
    }
  } else {
    touch_exploration_controller_.reset();
  }
}

}  // namespace shell
}  // namespace chromecast
