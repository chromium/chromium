// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/fullscreen_control/fullscreen_control_host.h"

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/user_metrics.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/views/exclusive_access_bubble_views.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_switches.h"
#include "components/fullscreen_control/fullscreen_control_view.h"
#include "components/version_info/channel.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/event_monitor.h"
#include "ui/views/view.h"

namespace {

// +----------------------------+
// |         +-------+          |
// |         |Control|          |
// |         +-------+          |
// |                            | <-- Control.bottom * kExitHeightScaleFactor
// |          Screen            |       Buffer for mouse moves or pointer events
// |                            |       before closing the fullscreen exit
// |                            |       control.
// +----------------------------+
//
// The same value is also used for timeout cooldown.
// This is a common scenario where people play video or present slides and they
// just want to keep their cursor on the top. In this case we timeout the exit
// control so that it doesn't show permanently. The user will then need to move
// the cursor out of the cooldown area and move it back to the top to re-trigger
// the exit UI.
constexpr float kExitHeightScaleFactor = 1.5f;

// +----------------------------+
// |                            |
// |                            |
// |                            | <-- kShowFullscreenExitControlHeight
// |          Screen            |       If a mouse move or pointer event is
// |                            |       above this line, show the fullscreen
// |                            |       exit control.
// |                            |
// +----------------------------+
constexpr float kShowFullscreenExitControlHeight = 3.f;

// Time to wait to hide the popup after it is triggered.
constexpr base::TimeDelta kMousePopupTimeout = base::Seconds(3);
constexpr base::TimeDelta kTouchPopupTimeout = base::Seconds(10);

// Time to wait before showing the popup when the escape key is held.
constexpr base::TimeDelta kKeyPressPopupDelay = base::Seconds(1);

bool IsExitUiEnabled() {
#if BUILDFLAG(IS_MAC)
  // Exit UI is unnecessary, since Mac uses the OS fullscreen such that window
  // menu and controls reveal when the cursor is moved to the top.
  return false;
#else
  // Kiosk mode is a fullscreen experience, which makes the exit UI
  // inappropriate.
  return !IsRunningInAppMode();
#endif
}

}  // namespace

FullscreenControlHost::FullscreenControlHost(BrowserView* browser_view)
    : browser_view_(browser_view) {
  if (IsFullscreenExitUIEnabled()) {
    event_monitor_ = views::EventMonitor::CreateWindowMonitor(
        this, browser_view->GetNativeWindow(),
        {ui::EventType::kMouseMoved, ui::EventType::kKeyPressed,
         ui::EventType::kKeyReleased, ui::EventType::kTouchPressed,
         ui::EventType::kGestureLongPress});
  }
}

FullscreenControlHost::~FullscreenControlHost() = default;

// static
bool FullscreenControlHost::IsFullscreenExitUIEnabled() {
  // TODO(joedow): Remove this function and all uses of it. The fullscreen exit
  // UI is now always enabled because the keyboard lock UI is always enabled.
  return true;
}

void FullscreenControlHost::OnEvent(const ui::Event& event) {
  if (event.IsKeyEvent())
    OnKeyEvent(*event.AsKeyEvent());
  else if (event.IsMouseEvent())
    OnMouseEvent(*event.AsMouseEvent());
  else if (event.IsTouchEvent())
    OnTouchEvent(*event.AsTouchEvent());
  else if (event.IsGestureEvent())
    OnGestureEvent(*event.AsGestureEvent());
}

void FullscreenControlHost::OnKeyEvent(const ui::KeyEvent& event) {
  if (event.key_code() != ui::VKEY_ESCAPE ||
      (input_entry_method_ != InputEntryMethod::NOT_ACTIVE &&
       input_entry_method_ != InputEntryMethod::KEYBOARD)) {
    return;
  }

  ExclusiveAccessManager* const exclusive_access_manager =
      browser_view_->browser()->exclusive_access_manager();

  // FullscreenControlHost UI is not needed for the keyboard input method in any
  // fullscreen mode except for tab-initiated fullscreen (and only when the user
  // is required to press and hold the escape key to exit).

  // If we are not in tab-initiated fullscreen, then we want to make sure the
  // UI exit bubble is not displayed.  This can occur when:
  // 1.) The user enters browser fullscreen (F11)
  // 2.) The website then enters tab-initiated fullscreen
  // 3.) User performs a press and hold gesture on escape
  //
  // In this case, the fullscreen controller will revert back to browser
  // fullscreen mode but there won't be a fullscreen exit message to trigger
  // the UI cleanup for the exit bubble.  To handle this case, we need to check
  // to make sure the UI is in the right fullscreen mode before proceeding.
  if (!exclusive_access_manager->fullscreen_controller()
           ->IsWindowFullscreenForTabOrPending()) {
    key_press_delay_timer_.Stop();
    if (IsVisible() && input_entry_method_ == InputEntryMethod::KEYBOARD)
      Hide(true);
    return;
  }

  // Note: This logic handles the UI feedback element used when holding down the
  // esc key, however the logic for exiting fullscreen is handled by the
  // KeyboardLockController class.
  if (event.type() == ui::EventType::kKeyPressed &&
      !key_press_delay_timer_.IsRunning() &&
      exclusive_access_manager->keyboard_lock_controller()
          ->RequiresPressAndHoldEscToExit()) {
    key_press_delay_timer_.Start(
        FROM_HERE, kKeyPressPopupDelay,
        base::BindOnce(&FullscreenControlHost::ShowForInputEntryMethod,
                       base::Unretained(this), InputEntryMethod::KEYBOARD));
  } else if (event.type() == ui::EventType::kKeyReleased) {
    key_press_delay_timer_.Stop();
    if (IsVisible() && input_entry_method_ == InputEntryMethod::KEYBOARD)
      Hide(true);
  }
}

void FullscreenControlHost::OnMouseEvent(const ui::MouseEvent& event) {
  if (!IsExitUiEnabled())
    return;

  if (event.type() != ui::EventType::kMouseMoved || IsAnimating() ||
      (input_entry_method_ != InputEntryMethod::NOT_ACTIVE &&
       input_entry_method_ != InputEntryMethod::MOUSE)) {
    return;
  }

  // TODO(crbug.com/957455) Do not show fullscreen exit button while in pointer
  // lock mode. This is only necessary because the current implementation of
  // pointer lock doesn't constrain the mouse cursor position, so the exit
  // button may still appear even though the mouse cursor is invisible and its
  // position is technically undefined. This mitigation will become unnecessary
  // when pointer lock is re-implemented using relative motion events.
  if (IsPointerLocked()) {
    return;
  }

  if (IsExitUiNeeded()) {
    if (IsVisible()) {
      if (event.y() >= CalculateCursorBufferHeight())
        Hide(true);
    } else {
      DCHECK_EQ(InputEntryMethod::NOT_ACTIVE, input_entry_method_);
      if (!in_mouse_cooldown_mode_ &&
          event.y() <= kShowFullscreenExitControlHeight) {
        // If the exit fullscreen prompt is being shown (say user just pressed
        // F11 with the cursor on the top of the screen) then we suppress the
        // fullscreen control host and just put it in cooldown mode.
        const auto* bubble = browser_view_->exclusive_access_bubble();
        if (bubble && bubble->IsShowing())
          in_mouse_cooldown_mode_ = true;
        else
          ShowForInputEntryMethod(InputEntryMethod::MOUSE);
      } else if (in_mouse_cooldown_mode_ &&
                 event.y() >= CalculateCursorBufferHeight()) {
        in_mouse_cooldown_mode_ = false;
      }
    }
  } else if (IsVisible()) {
    Hide(true);
  }
}

void FullscreenControlHost::OnTouchEvent(const ui::TouchEvent& event) {
  if (input_entry_method_ != InputEntryMethod::TOUCH)
    return;

  DCHECK(IsVisible());

  // Hide the popup if it is showing and the user touches outside of the popup.
  if (event.type() == ui::EventType::kTouchPressed && !IsAnimating()) {
    Hide(true);
  }
}

void FullscreenControlHost::OnGestureEvent(const ui::GestureEvent& event) {
  if (!IsExitUiEnabled())
    return;

  if (event.type() == ui::EventType::kGestureLongPress && IsExitUiNeeded() &&
      !IsVisible()) {
    ShowForInputEntryMethod(InputEntryMethod::TOUCH);
  }
}

void FullscreenControlHost::Hide(bool animate) {
  if (IsPopupCreated())
    GetPopup()->Hide(animate);
}

bool FullscreenControlHost::IsVisible() const {
  return IsPopupCreated() && fullscreen_control_popup_->IsVisible();
}

FullscreenControlPopup* FullscreenControlHost::GetPopup() {
  if (!IsPopupCreated()) {
    fullscreen_control_popup_ = std::make_unique<FullscreenControlPopup>(
        browser_view_->GetBubbleParentView(),
        base::BindRepeating(
            &FullscreenControlHost::OnExitFullscreenPopupClicked,
            base::Unretained(this)),
        base::BindRepeating(&FullscreenControlHost::OnVisibilityChanged,
                            base::Unretained(this)));
  }
  return fullscreen_control_popup_.get();
}

bool FullscreenControlHost::IsPopupCreated() const {
  return fullscreen_control_popup_.get() != nullptr;
}

bool FullscreenControlHost::IsAnimating() const {
  return IsPopupCreated() && fullscreen_control_popup_->IsAnimating();
}

void FullscreenControlHost::ShowForInputEntryMethod(
    InputEntryMethod input_entry_method) {
  input_entry_method_ = input_entry_method;
  auto* bubble = browser_view_->exclusive_access_bubble();
  if (bubble)
    bubble->HideImmediately();
  GetPopup()->Show(browser_view_->GetClientAreaBoundsInScreen());

  // Exit cooldown mode in case the exit UI is triggered by a different method.
  in_mouse_cooldown_mode_ = false;
}

void FullscreenControlHost::OnVisibilityChanged() {
  if (!IsVisible()) {
    input_entry_method_ = InputEntryMethod::NOT_ACTIVE;
    key_press_delay_timer_.Stop();
  } else if (input_entry_method_ == InputEntryMethod::MOUSE) {
    StartPopupTimeout(InputEntryMethod::MOUSE, kMousePopupTimeout);
  } else if (input_entry_method_ == InputEntryMethod::TOUCH) {
    StartPopupTimeout(InputEntryMethod::TOUCH, kTouchPopupTimeout);
  }

  if (on_popup_visibility_changed_)
    std::move(on_popup_visibility_changed_).Run();
}

void FullscreenControlHost::StartPopupTimeout(
    InputEntryMethod expected_input_method,
    base::TimeDelta timeout) {
  popup_timeout_timer_.Start(
      FROM_HERE, timeout,
      base::BindOnce(&FullscreenControlHost::OnPopupTimeout,
                     base::Unretained(this), expected_input_method));
}

void FullscreenControlHost::OnPopupTimeout(
    InputEntryMethod expected_input_method) {
  if (IsVisible() && !IsAnimating() &&
      input_entry_method_ == expected_input_method) {
    if (input_entry_method_ == InputEntryMethod::MOUSE)
      in_mouse_cooldown_mode_ = true;
    Hide(true);
  }
}

bool FullscreenControlHost::IsExitUiNeeded() {
  return browser_view_->IsFullscreen() &&
         browser_view_->CanUserExitFullscreen() &&
         browser_view_->ShouldHideUIForFullscreen();
}

bool FullscreenControlHost::IsPointerLocked() {
  if (!browser_view_) {
    return false;
  }

  auto* web_contents = browser_view_->GetActiveWebContents();
  if (!web_contents) {
    return false;
  }

  auto* rwhv = web_contents->GetRenderWidgetHostView();
  if (!rwhv) {
    return false;
  }

  return rwhv->IsPointerLocked();
}

float FullscreenControlHost::CalculateCursorBufferHeight() const {
  float control_bottom = FullscreenControlPopup::GetButtonBottomOffset();
  return control_bottom * kExitHeightScaleFactor;
}

void FullscreenControlHost::OnExitFullscreenPopupClicked() {
  base::RecordAction(
      base::UserMetricsAction("ExitFullscreen_PopupCloseButton"));
  browser_view_->ExitFullscreen();
}
