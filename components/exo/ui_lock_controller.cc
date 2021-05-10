// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/ui_lock_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_types.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_state_observer.h"
#include "base/feature_list.h"
#include "base/optional.h"
#include "base/scoped_observation.h"
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

namespace {

constexpr auto kEscHoldMessageDuration = base::TimeDelta::FromSeconds(4);

// Shows 'Press and hold ESC to exit fullscreen' message.
class EscHoldNotifier : public ash::WindowStateObserver {
 public:
  explicit EscHoldNotifier(aura::Window* window) {
    ash::WindowState* window_state = ash::WindowState::Get(window);
    window_state_observation_.Observe(window_state);
    if (window_state->IsFullscreen())
      ShowBubble(window);
  }

  EscHoldNotifier(const EscHoldNotifier&) = delete;
  EscHoldNotifier& operator=(const EscHoldNotifier&) = delete;

  ~EscHoldNotifier() override { CloseBubble(); }

  views::Widget* bubble() { return bubble_; }

 private:
  // Overridden from ash::WindowStateObserver:
  void OnPreWindowStateTypeChange(ash::WindowState* window_state,
                                  chromeos::WindowStateType old_type) override {
  }
  void OnPostWindowStateTypeChange(
      ash::WindowState* window_state,
      chromeos::WindowStateType old_type) override {
    if (window_state->IsFullscreen()) {
      ShowBubble(window_state->window());
    } else {
      CloseBubble();
    }
  }

  void ShowBubble(aura::Window* window) {
    // Only show message once per window.
    if (has_been_shown_)
      return;

    views::Widget* widget =
        views::Widget::GetTopLevelWidgetForNativeView(window);
    if (!widget)
      return;
    bubble_ = exo::UILockBubbleView::DisplayBubble(widget->GetContentsView());

    // Close bubble after 4s.
    close_timer_.Start(
        FROM_HERE, kEscHoldMessageDuration,
        base::BindOnce(&EscHoldNotifier::CloseBubble, base::Unretained(this),
                       /*closed_by_timer=*/true));
  }

  void CloseBubble(bool closed_by_timer = false) {
    if (bubble_) {
      bubble_->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
      bubble_ = nullptr;
      // Message is only shown once as long as it shows for the full 4s.
      if (closed_by_timer) {
        has_been_shown_ = true;
        window_state_observation_.Reset();
      }
    }
  }

  views::Widget* bubble_ = nullptr;
  bool has_been_shown_ = false;
  base::OneShotTimer close_timer_;
  base::ScopedObservation<ash::WindowState, ash::WindowStateObserver>
      window_state_observation_{this};
};

}  // namespace

DEFINE_UI_CLASS_PROPERTY_TYPE(EscHoldNotifier*)

namespace exo {
namespace {
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(EscHoldNotifier,
                                   kEscHoldNotifierKey,
                                   nullptr)
}

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

  aura::Window* window = gained_focus->window()->GetToplevelWindow();
  if (!window)
    return;

  // If the window does not have kEscHoldToExitFullscreen, or we are already
  // tracking it, then ignore.
  if (!window->GetProperty(chromeos::kEscHoldToExitFullscreen) ||
      window->GetProperty(kEscHoldNotifierKey)) {
    return;
  }

  // Object is owned as a window property.
  window->SetProperty(kEscHoldNotifierKey, new EscHoldNotifier(window));
}

bool UILockController::IsBubbleVisibleForTesting(aura::Window* window) {
  return window->GetProperty(kEscHoldNotifierKey)->bubble();
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
