// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ui/frame/caption_buttons/frame_size_button.h"

#include <memory>

#include "base/i18n/rtl.h"
#include "base/metrics/user_metrics.h"
#include "chromeos/ui/base/display_util.h"
#include "chromeos/ui/base/window_properties.h"
#include "chromeos/ui/frame/caption_buttons/snap_controller.h"
#include "chromeos/ui/wm/features.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/base/hit_test.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/widget/widget.h"

namespace chromeos {

namespace {

// The default delay between the user pressing the size button and the buttons
// adjacent to the size button morphing into buttons for snapping left and
// right.
const int kSetButtonsToSnapModeDelayMs = 150;

// The amount that a user can overshoot one of the caption buttons while in
// "snap mode" and keep the button hovered/pressed.
const int kMaxOvershootX = 200;
const int kMaxOvershootY = 50;

// Returns true if a mouse drag while in "snap mode" at |location_in_screen|
// would hover/press |button| or keep it hovered/pressed.
bool HitTestButton(const views::FrameCaptionButton* button,
                   const gfx::Point& location_in_screen) {
  gfx::Rect expanded_bounds_in_screen = button->GetBoundsInScreen();
  if (button->GetState() == views::Button::STATE_HOVERED ||
      button->GetState() == views::Button::STATE_PRESSED) {
    expanded_bounds_in_screen.Inset(
        gfx::Insets::VH(-kMaxOvershootY, -kMaxOvershootX));
  }
  return expanded_bounds_in_screen.Contains(location_in_screen);
}

SnapDirection GetSnapDirection(const views::FrameCaptionButton* to_hover) {
  if (to_hover) {
    const bool is_primary_display_layout = chromeos::IsDisplayLayoutPrimary(
        display::Screen::GetScreen()->GetDisplayNearestWindow(
            to_hover->GetWidget()->GetNativeWindow()));
    const bool is_primary_snap =
        is_primary_display_layout ||
        !chromeos::wm::features::IsVerticalSnapEnabled();
    switch (to_hover->GetIcon()) {
      case views::CAPTION_BUTTON_ICON_LEFT_TOP_SNAPPED:
        return is_primary_snap ? SnapDirection::kPrimary
                               : SnapDirection::kSecondary;
      case views::CAPTION_BUTTON_ICON_RIGHT_BOTTOM_SNAPPED:
        return is_primary_snap ? SnapDirection::kSecondary
                               : SnapDirection::kPrimary;
      case views::CAPTION_BUTTON_ICON_FLOAT:
      case views::CAPTION_BUTTON_ICON_MAXIMIZE_RESTORE:
      case views::CAPTION_BUTTON_ICON_MINIMIZE:
      case views::CAPTION_BUTTON_ICON_CLOSE:
      case views::CAPTION_BUTTON_ICON_BACK:
      case views::CAPTION_BUTTON_ICON_LOCATION:
      case views::CAPTION_BUTTON_ICON_MENU:
      case views::CAPTION_BUTTON_ICON_ZOOM:
      case views::CAPTION_BUTTON_ICON_CENTER:
      case views::CAPTION_BUTTON_ICON_CUSTOM:
      case views::CAPTION_BUTTON_ICON_COUNT:
        NOTREACHED();
        break;
    }
  }

  return SnapDirection::kNone;
}

}  // namespace

// The class to observe the to-be-snapped window during the waiting-for-snap
// mode. If the window's window state is changed or the window is put in
// overview during the waiting mode, cancel the snap.
class FrameSizeButton::SnappingWindowObserver : public aura::WindowObserver {
 public:
  SnappingWindowObserver(aura::Window* window, FrameSizeButton* size_button)
      : window_(window), size_button_(size_button) {
    window_->AddObserver(this);
  }

  SnappingWindowObserver(const SnappingWindowObserver&) = delete;
  SnappingWindowObserver& operator=(const SnappingWindowObserver&) = delete;

  ~SnappingWindowObserver() override {
    if (window_) {
      window_->RemoveObserver(this);
      window_ = nullptr;
    }
  }

  // aura::WindowObserver:
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override {
    DCHECK_EQ(window_, window);
    if ((key == chromeos::kIsShowingInOverviewKey &&
         window_->GetProperty(chromeos::kIsShowingInOverviewKey)) ||
        key == chromeos::kWindowStateTypeKey) {
      // If the window is put in overview while we're in waiting-for-snapping
      // mode, or the window's window state has changed, cancel the snap.
      size_button_->CancelSnap();
    }
  }

  void OnWindowDestroying(aura::Window* window) override {
    DCHECK_EQ(window_, window);
    window_->RemoveObserver(this);
    window_ = nullptr;
    size_button_->CancelSnap();
  }

 private:
  aura::Window* window_;
  FrameSizeButton* size_button_;
};

FrameSizeButton::FrameSizeButton(PressedCallback callback,
                                 FrameSizeButtonDelegate* delegate)
    : views::FrameCaptionButton(std::move(callback),
                                views::CAPTION_BUTTON_ICON_MAXIMIZE_RESTORE,
                                HTMAXBUTTON),
      delegate_(delegate),
      set_buttons_to_snap_mode_delay_ms_(kSetButtonsToSnapModeDelayMs),
      in_snap_mode_(false) {}

FrameSizeButton::~FrameSizeButton() = default;

bool FrameSizeButton::OnMousePressed(const ui::MouseEvent& event) {
  // The minimize and close buttons are set to snap left and right when snapping
  // is enabled. Do not enable snapping if the minimize button is not visible.
  // The close button is always visible.
  if (IsTriggerableEvent(event) && !in_snap_mode_ &&
      delegate_->IsMinimizeButtonVisible() && delegate_->CanSnap()) {
    StartSetButtonsToSnapModeTimer(event);
  }
  views::FrameCaptionButton::OnMousePressed(event);
  return true;
}

bool FrameSizeButton::OnMouseDragged(const ui::MouseEvent& event) {
  UpdateSnapPreview(event);
  // By default a FrameCaptionButton reverts to STATE_NORMAL once the mouse
  // leaves its bounds. Skip FrameCaptionButton's handling when
  // |in_snap_mode_| == true because we want different behavior.
  if (!in_snap_mode_)
    views::FrameCaptionButton::OnMouseDragged(event);
  return true;
}

void FrameSizeButton::OnMouseReleased(const ui::MouseEvent& event) {
  if (IsTriggerableEvent(event))
    CommitSnap(event);

  views::FrameCaptionButton::OnMouseReleased(event);
}

void FrameSizeButton::OnMouseCaptureLost() {
  SetButtonsToNormalMode(FrameSizeButtonDelegate::Animate::kYes);
  views::FrameCaptionButton::OnMouseCaptureLost();
}

void FrameSizeButton::OnMouseMoved(const ui::MouseEvent& event) {
  // Ignore any synthetic mouse moves during a drag.
  if (!in_snap_mode_)
    views::FrameCaptionButton::OnMouseMoved(event);
}

void FrameSizeButton::OnGestureEvent(ui::GestureEvent* event) {
  if (event->details().touch_points() > 1) {
    SetButtonsToNormalMode(FrameSizeButtonDelegate::Animate::kYes);
    return;
  }
  if (event->type() == ui::ET_GESTURE_TAP_DOWN && delegate_->CanSnap()) {
    StartSetButtonsToSnapModeTimer(*event);
    // Go through FrameCaptionButton's handling so that the button gets pressed.
    views::FrameCaptionButton::OnGestureEvent(event);
    return;
  }

  if (event->type() == ui::ET_GESTURE_SCROLL_BEGIN ||
      event->type() == ui::ET_GESTURE_SCROLL_UPDATE) {
    UpdateSnapPreview(*event);
    event->SetHandled();
    return;
  }

  if (event->type() == ui::ET_GESTURE_TAP ||
      event->type() == ui::ET_GESTURE_SCROLL_END ||
      event->type() == ui::ET_SCROLL_FLING_START ||
      event->type() == ui::ET_GESTURE_END) {
    if (CommitSnap(*event)) {
      event->SetHandled();
      return;
    }
  }

  views::FrameCaptionButton::OnGestureEvent(event);
}

void FrameSizeButton::StartSetButtonsToSnapModeTimer(
    const ui::LocatedEvent& event) {
  set_buttons_to_snap_mode_timer_event_location_ = event.location();
  if (set_buttons_to_snap_mode_delay_ms_ == 0) {
    AnimateButtonsToSnapMode();
  } else {
    set_buttons_to_snap_mode_timer_.Start(
        FROM_HERE, base::Milliseconds(set_buttons_to_snap_mode_delay_ms_), this,
        &FrameSizeButton::AnimateButtonsToSnapMode);
  }
}

void FrameSizeButton::AnimateButtonsToSnapMode() {
  SetButtonsToSnapMode(FrameSizeButtonDelegate::Animate::kYes);

  // Start observing the to-be-snapped window.
  snapping_window_observer_ = std::make_unique<SnappingWindowObserver>(
      GetWidget()->GetNativeWindow(), this);
}

void FrameSizeButton::SetButtonsToSnapMode(
    FrameSizeButtonDelegate::Animate animate) {
  in_snap_mode_ = true;

  // When using a right-to-left layout the close button is left of the size
  // button and the minimize button is right of the size button.
  if (base::i18n::IsRTL()) {
    delegate_->SetButtonIcons(views::CAPTION_BUTTON_ICON_RIGHT_BOTTOM_SNAPPED,
                              views::CAPTION_BUTTON_ICON_LEFT_TOP_SNAPPED,
                              animate);
  } else {
    delegate_->SetButtonIcons(views::CAPTION_BUTTON_ICON_LEFT_TOP_SNAPPED,
                              views::CAPTION_BUTTON_ICON_RIGHT_BOTTOM_SNAPPED,
                              animate);
  }
  // Show Multitask Menu if float is enabled. Note here float flag is also used
  // to represent other relatable UI/UX changes.
  // TODO(shidi) Move this when long hover trigger (crbug.com/1330016) is
  // implemented.
  if (chromeos::wm::features::IsFloatWindowEnabled()) {
    multitask_menu_ = std::make_unique<MultitaskMenu>(
        /*anchor=*/this, GetWidget()->GetNativeWindow());
    multitask_menu_->ShowBubble();
  }
}

void FrameSizeButton::UpdateSnapPreview(const ui::LocatedEvent& event) {
  if (!in_snap_mode_) {
    // Set the buttons adjacent to the size button to snap left and right early
    // if the user drags past the drag threshold.
    // |set_buttons_to_snap_mode_timer_| is checked to avoid entering the snap
    // mode as a result of an unsupported drag type (e.g. only the right mouse
    // button is pressed).
    gfx::Vector2d delta(event.location() -
                        set_buttons_to_snap_mode_timer_event_location_);
    if (!set_buttons_to_snap_mode_timer_.IsRunning() ||
        !views::View::ExceededDragThreshold(delta)) {
      return;
    }
    AnimateButtonsToSnapMode();
  }

  const views::FrameCaptionButton* to_hover = GetButtonToHover(event);
  SnapDirection snap = GetSnapDirection(to_hover);

  gfx::Point event_location_in_screen(event.location());
  views::View::ConvertPointToScreen(this, &event_location_in_screen);
  bool press_size_button =
      to_hover || HitTestButton(this, event_location_in_screen);

  if (to_hover) {
    // Progress the minimize and close icon morph animations to the end if they
    // are in progress.
    SetButtonsToSnapMode(FrameSizeButtonDelegate::Animate::kNo);
  }

  delegate_->SetHoveredAndPressedButtons(to_hover,
                                         press_size_button ? this : nullptr);
  delegate_->ShowSnapPreview(snap,
                             /*allow_haptic_feedback=*/event.IsMouseEvent());
}

const views::FrameCaptionButton* FrameSizeButton::GetButtonToHover(
    const ui::LocatedEvent& event) const {
  gfx::Point event_location_in_screen(event.location());
  views::View::ConvertPointToScreen(this, &event_location_in_screen);
  const views::FrameCaptionButton* closest_button =
      delegate_->GetButtonClosestTo(event_location_in_screen);
  if ((closest_button->GetIcon() ==
           views::CAPTION_BUTTON_ICON_LEFT_TOP_SNAPPED ||
       closest_button->GetIcon() ==
           views::CAPTION_BUTTON_ICON_RIGHT_BOTTOM_SNAPPED) &&
      HitTestButton(closest_button, event_location_in_screen)) {
    return closest_button;
  }
  return nullptr;
}

bool FrameSizeButton::CommitSnap(const ui::LocatedEvent& event) {
  snapping_window_observer_.reset();
  SnapDirection snap = GetSnapDirection(GetButtonToHover(event));
  delegate_->CommitSnap(snap);
  delegate_->SetHoveredAndPressedButtons(nullptr, nullptr);

  if (snap == SnapDirection::kPrimary) {
    base::RecordAction(base::UserMetricsAction("MaxButton_MaxLeft"));
  } else if (snap == SnapDirection::kSecondary) {
    base::RecordAction(base::UserMetricsAction("MaxButton_MaxRight"));
  } else {
    SetButtonsToNormalMode(FrameSizeButtonDelegate::Animate::kYes);
    return false;
  }

  SetButtonsToNormalMode(FrameSizeButtonDelegate::Animate::kNo);
  return true;
}

void FrameSizeButton::CancelSnap() {
  snapping_window_observer_.reset();
  delegate_->CommitSnap(SnapDirection::kNone);
  delegate_->SetHoveredAndPressedButtons(nullptr, nullptr);
  SetButtonsToNormalMode(FrameSizeButtonDelegate::Animate::kYes);
}

void FrameSizeButton::SetButtonsToNormalMode(
    FrameSizeButtonDelegate::Animate animate) {
  in_snap_mode_ = false;
  set_buttons_to_snap_mode_timer_.Stop();
  delegate_->SetButtonsToNormal(animate);
}

}  // namespace chromeos
