// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/ui_lock_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_types.h"
#include "ash/wm/window_state.h"
#include "base/feature_list.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/ui/base/window_properties.h"
#include "components/exo/seat.h"
#include "components/exo/shell_surface_util.h"
#include "components/exo/surface.h"
#include "components/exo/ui_lock_bubble.h"
#include "components/exo/wm_helper.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/views/widget/widget.h"

namespace exo {

constexpr auto kLongPressEscapeDuration = base::TimeDelta::FromSeconds(2);
constexpr auto kExcludedFlags = ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN |
                                ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN |
                                ui::EF_ALTGR_DOWN | ui::EF_IS_REPEAT;

UILockController::UILockController(Seat* seat) : seat_(seat) {
  WMHelper::GetInstance()->AddPreTargetHandler(this);
  seat_->AddObserver(this);
}

UILockController::~UILockController() {
  seat_->RemoveObserver(this);
  WMHelper::GetInstance()->RemovePreTargetHandler(this);
  if (bubble_widget_) {
    bubble_widget_->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
    bubble_widget_ = nullptr;
  }
}

void UILockController::OnKeyEvent(ui::KeyEvent* event) {
  // If the event target is not an exo::Surface, let another handler process the
  // event.
  if (!GetShellRootSurface(static_cast<aura::Window*>(event->target())) &&
      !Surface::AsSurface(static_cast<aura::Window*>(event->target()))) {
    return;
  }

  if (event->code() == ui::DomCode::ESCAPE &&
      (event->flags() & kExcludedFlags) == 0) {
    OnEscapeKey(event->type() == ui::ET_KEY_PRESSED);
  }
}

void UILockController::OnSurfaceFocused(Surface* gained_focus) {
  if (gained_focus != focused_surface_to_unlock_)
    StopTimer();

  if (!base::FeatureList::IsEnabled(chromeos::features::kExoLockNotification))
    return;

  if (!gained_focus || !gained_focus->window())
    return;

  views::Widget* top_level_widget =
      views::Widget::GetTopLevelWidgetForNativeView(gained_focus->window());
  aura::Window* native_window =
      top_level_widget ? top_level_widget->GetNativeWindow() : nullptr;
  ash::WindowState* window_state = ash::WindowState::Get(native_window);

  // If the window is not fullscreen do not display.
  if (!window_state || !window_state->IsFullscreen())
    return;

  // If the bubble exists and is already anchored to the current view then exit.
  if (bubble_widget_ && bubble_widget_->parent()->GetContentsView() ==
                            top_level_widget->GetContentsView()) {
    return;
  }

  // If the bubble exists and is anchored to a different surface, destroy that
  // bubble before creating a new one.
  if (bubble_widget_ && bubble_widget_->parent()->GetContentsView() !=
                            top_level_widget->GetContentsView()) {
    bubble_widget_->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
    bubble_widget_ = nullptr;
  }

  bubble_widget_ =
      UILockBubbleView::DisplayBubble(top_level_widget->GetContentsView());
}

void UILockController::OnPostWindowStateTypeChange(
    ash::WindowState* window_state,
    chromeos::WindowStateType old_type) {
  // If the window is no longer fullscreen and there is a bubble showing, close
  // it.
  if (!window_state->IsFullscreen() && bubble_widget_) {
    bubble_widget_->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
    bubble_widget_ = nullptr;
  }
}

views::Widget* UILockController::GetBubbleForTesting() {
  return bubble_widget_;
}

namespace {
bool EscapeHoldShouldExitFullscreen(Seat* seat) {
  auto* surface = seat->GetFocusedSurface();
  if (!surface)
    return false;

  auto* widget =
      views::Widget::GetTopLevelWidgetForNativeView(surface->window());
  if (!widget)
    return false;

  aura::Window* window = widget->GetNativeWindow();
  if (!window || !window->GetProperty(chromeos::kEscHoldToExitFullscreen)) {
    return false;
  }

  auto* window_state = ash::WindowState::Get(window);
  return window_state && window_state->IsFullscreen();
}
}  // namespace

void UILockController::OnEscapeKey(bool pressed) {
  if (pressed) {
    if (EscapeHoldShouldExitFullscreen(seat_) &&
        !exit_fullscreen_timer_.IsRunning()) {
      focused_surface_to_unlock_ = seat_->GetFocusedSurface();
      exit_fullscreen_timer_.Start(
          FROM_HERE, kLongPressEscapeDuration,
          base::BindOnce(&UILockController::OnEscapeHeld,
                         base::Unretained(this)));
    }
  } else {
    StopTimer();
  }
}

void UILockController::OnEscapeHeld() {
  auto* surface = seat_->GetFocusedSurface();
  if (!surface || surface != focused_surface_to_unlock_) {
    focused_surface_to_unlock_ = nullptr;
    return;
  }

  if (bubble_widget_) {
    bubble_widget_->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
    bubble_widget_ = nullptr;
  }

  focused_surface_to_unlock_ = nullptr;

  auto* widget =
      views::Widget::GetTopLevelWidgetForNativeView(surface->window());
  auto* window_state =
      ash::WindowState::Get(widget ? widget->GetNativeWindow() : nullptr);
  if (window_state) {
    if (window_state->window()->GetProperty(
            chromeos::kEscHoldExitFullscreenToMinimized)) {
      window_state->Minimize();
    } else {
      window_state->Restore();
    }
  }
}

void UILockController::StopTimer() {
  if (exit_fullscreen_timer_.IsRunning()) {
    exit_fullscreen_timer_.Stop();
    focused_surface_to_unlock_ = nullptr;
  }
}

}  // namespace exo
