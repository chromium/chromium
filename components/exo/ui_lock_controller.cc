// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/ui_lock_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_types.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_state_observer.h"
#include "base/bind.h"
#include "base/feature_list.h"
#include "base/optional.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/ui/base/window_properties.h"
#include "components/exo/seat.h"
#include "components/exo/shell_surface_util.h"
#include "components/exo/surface.h"
#include "components/exo/wm_helper.h"
#include "components/fullscreen_control/fullscreen_control_popup.h"
#include "components/fullscreen_control/subtle_notification_view.h"
#include "components/strings/grit/components_strings.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/widget/widget.h"

namespace {

// The Esc hold bubble shows a message to press and hold Esc to exit fullscreen.
// The bubble will hide after a 4s timeout and will not display again for that
// window even if it toggles fullscreen.
//
// The exit popup is a circle with an 'X' close icon which exits fullscreen when
// the user clicks it.
// * It is not shown on windows such as borealis with property
//   kEscHoldExitFullscreenToMinimized.
// * It is displayed when the mouse moves to the top 3px of the screen.
// * It will hide after a 3s timeout, or the user moves below 150px.
// * After hiding, there is a cooldown where it will not display again until the
//   mouse moves below 150px.

// Duration to show the 'Press and hold Esc' bubble.
constexpr auto kEscNotifyBubbleDuration = base::TimeDelta::FromSeconds(4);
// Position of Esc notify bubble from top of screen.
const int kEscNotifyBubbleTopPx = 45;
// Duration to show the exit 'X' popup.
constexpr auto kExitPopupDuration = base::TimeDelta::FromSeconds(3);
// Display the exit popup if mouse is above this height.
constexpr float kExitPopupDisplayHeight = 3.f;
// Hide the exit popup if mouse is below this height.
constexpr float kExitPopupHideHeight = 150.f;

// Create and position Esc notify bubble.
views::Widget* CreateEscNotifyBubble(aura::Window* parent) {
  auto content_view = std::make_unique<SubtleNotificationView>();
  std::u16string accelerator = l10n_util::GetStringUTF16(IDS_APP_ESC_KEY);
  content_view->UpdateContent(l10n_util::GetStringFUTF16(
      IDS_FULLSCREEN_HOLD_ESC_TO_EXIT_FULLSCREEN, accelerator));
  gfx::Size size = content_view->GetPreferredSize();
  views::Widget* popup = SubtleNotificationView::CreatePopupWidget(
      parent, std::move(content_view));
  popup->SetZOrderLevel(ui::ZOrderLevel::kSecuritySurface);
  int x = (parent->bounds().width() - size.width()) / 2;
  popup->SetBounds(gfx::Rect(gfx::Point(x, kEscNotifyBubbleTopPx), size));
  return popup;
}

// Exits fullscreen to either default or minimized.
void ExitFullscreen(aura::Window* window) {
  ash::WindowState* window_state = ash::WindowState::Get(window);
  if (window->GetProperty(chromeos::kEscHoldExitFullscreenToMinimized))
    window_state->Minimize();
  else
    window_state->Restore();
}

// Shows 'Press and hold ESC to exit fullscreen' message, and exit popup.
class EscHoldNotifier : public ui::EventHandler,
                        public ash::WindowStateObserver {
 public:
  explicit EscHoldNotifier(aura::Window* window) : window_(window) {
    ash::WindowState* window_state = ash::WindowState::Get(window);
    window_state_observation_.Observe(window_state);
    if (window_state->IsFullscreen())
      OnFullscreen();
  }

  EscHoldNotifier(const EscHoldNotifier&) = delete;
  EscHoldNotifier& operator=(const EscHoldNotifier&) = delete;

  ~EscHoldNotifier() override { CloseAll(); }

  views::Widget* esc_notify_bubble() { return esc_notify_bubble_; }

  FullscreenControlPopup* exit_popup() { return exit_popup_.get(); }

 private:
  // Overridden from ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override {
    gfx::PointF point = event->location_f();
    aura::Window::ConvertPointToTarget(
        static_cast<aura::Window*>(event->target()), window_, &point);
    if (!esc_notify_bubble_ && !exit_popup_cooldown_ &&
        window_ == exo::WMHelper::GetInstance()->GetActiveWindow() &&
        point.y() <= kExitPopupDisplayHeight) {
      // Show exit popup if mouse is above 3px, unless esc notify bubble is
      // visible, or during cooldown (popup shown and mouse still at top).
      if (!exit_popup_) {
        exit_popup_ = std::make_unique<FullscreenControlPopup>(
            window_, base::BindRepeating(&ExitFullscreen, window_),
            base::DoNothing());
      }
      views::Widget* widget =
          views::Widget::GetTopLevelWidgetForNativeView(window_);
      exit_popup_->Show(widget->GetClientAreaBoundsInScreen());
      exit_popup_timer_.Start(FROM_HERE, kExitPopupDuration,
                              base::BindOnce(&EscHoldNotifier::HideExitPopup,
                                             base::Unretained(this),
                                             /*animate=*/true));
      exit_popup_cooldown_ = true;
    } else if (point.y() > kExitPopupHideHeight) {
      // Hide exit popup if mouse is below 150px, reset cooloff.
      HideExitPopup(/*animate=*/true);
      exit_popup_cooldown_ = false;
    }
  }

  // Overridden from ash::WindowStateObserver:
  void OnPostWindowStateTypeChange(
      ash::WindowState* window_state,
      chromeos::WindowStateType old_type) override {
    DCHECK_EQ(window_, window_state->window());
    if (window_state->IsFullscreen()) {
      OnFullscreen();
    } else {
      CloseAll();
    }
  }

  void OnFullscreen() {
    // Register ui::EventHandler to watch if mouse goes to top of screen.
    if (!is_handling_events_ &&
        !window_->GetProperty(chromeos::kEscHoldExitFullscreenToMinimized)) {
      window_->AddPreTargetHandler(this);
      is_handling_events_ = true;
    }

    // Only show Esc notify bubble once per window when window is active.
    if (esc_notify_bubble_shown_ ||
        window_ != exo::WMHelper::GetInstance()->GetActiveWindow()) {
      return;
    }

    if (!esc_notify_bubble_)
      esc_notify_bubble_ = CreateEscNotifyBubble(window_);
    esc_notify_bubble_->Show();

    // Close Esc notify bubble after 4s.
    esc_notify_bubble_timer_.Start(
        FROM_HERE, kEscNotifyBubbleDuration,
        base::BindOnce(&EscHoldNotifier::CloseEscNotifyBubble,
                       base::Unretained(this),
                       /*closed_by_timer=*/true));
  }

  void CloseAll() {
    if (is_handling_events_) {
      window_->RemovePreTargetHandler(this);
      is_handling_events_ = false;
    }
    CloseEscNotifyBubble();
    HideExitPopup();
  }

  void CloseEscNotifyBubble(bool closed_by_timer = false) {
    if (esc_notify_bubble_) {
      esc_notify_bubble_->CloseWithReason(
          views::Widget::ClosedReason::kUnspecified);
      esc_notify_bubble_ = nullptr;
      // Esc notify bubble is not reshown after it is closed by the timer.
      if (closed_by_timer) {
        esc_notify_bubble_shown_ = true;
      }
    }
  }

  void HideExitPopup(bool animate = false) {
    if (exit_popup_)
      exit_popup_->Hide(animate);
  }

  aura::Window* window_;
  views::Widget* esc_notify_bubble_ = nullptr;
  std::unique_ptr<FullscreenControlPopup> exit_popup_;
  bool is_handling_events_ = false;
  bool esc_notify_bubble_shown_ = false;
  bool exit_popup_cooldown_ = false;
  base::OneShotTimer esc_notify_bubble_timer_;
  base::OneShotTimer exit_popup_timer_;
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
  // TODO(oshima): Rather than handling key event here, add a hook in
  // keyboard.cc to intercept key event and handle this.

  // If no surface is focused, let another handler process the event.
  aura::Window* window = static_cast<aura::Window*>(event->target());
  if (!GetTargetSurfaceForKeyboardFocus(window))
    return;

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
  return window->GetProperty(kEscHoldNotifierKey)->esc_notify_bubble();
}

FullscreenControlPopup* UILockController::GetExitPopupForTesting(
    aura::Window* window) {
  return window->GetProperty(kEscHoldNotifierKey)->exit_popup();
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

  ExitFullscreen(surface->window()->GetToplevelWindow());
}

void UILockController::StopTimer() {
  if (exit_fullscreen_timer_.IsRunning()) {
    exit_fullscreen_timer_.Stop();
    focused_surface_to_unlock_ = nullptr;
  }
}

}  // namespace exo
