// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"

#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/exclusive_access/pointer_lock_controller.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/chrome_switches.h"
#include "components/input/native_web_keyboard_event.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "ui/events/keycodes/keyboard_codes.h"

using content::WebContents;

namespace {

// Amount of time the user must press on Esc to make it a press-and-hold event.
constexpr base::TimeDelta kHoldEscapeTime = base::Milliseconds(1500);

// Amount of time the user must press on Esc to see the Exclusive Access Bubble
// showing up.
constexpr base::TimeDelta kShowExitBubbleTime = base::Milliseconds(500);

constexpr char kHistogramFullscreenLockStateAtEntryViaApi[] =
    "WebCore.Fullscreen.LockStateAtEntryViaApi";
constexpr char kHistogramFullscreenLockStateAtEntryViaBrowserUi[] =
    "WebCore.Fullscreen.LockStateAtEntryViaBrowserUi";
constexpr char kHistogramEscKeyPressedDownWithModifier[] =
    "Browser.EscKeyPressedDownWithModifier";

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class LockState {
  kUnlocked = 0,
  kKeyboardLocked = 1,
  kPointerLocked = 2,
  kKeyboardAndPointerLocked = 3,
  kMaxValue = kKeyboardAndPointerLocked,
};

// Check whether `event` is a kRawKeyDown type and doesn't have non-stateful
// modifiers (i.e. shift, ctrl etc.).
bool IsUnmodifiedEscKeyDownEvent(const input::NativeWebKeyboardEvent& event) {
  if (event.GetType() != input::NativeWebKeyboardEvent::Type::kRawKeyDown) {
    return false;
  }
  if (event.GetModifiers() & blink::WebInputEvent::kKeyModifiers) {
    return false;
  }
  return true;
}

}  // namespace

ExclusiveAccessManager::ExclusiveAccessManager(
    ExclusiveAccessContext* exclusive_access_context)
    : exclusive_access_context_(exclusive_access_context),
      fullscreen_controller_(this),
      keyboard_lock_controller_(this),
      pointer_lock_controller_(this),
      exclusive_access_controllers_({&fullscreen_controller_,
                                     &keyboard_lock_controller_,
                                     &pointer_lock_controller_}),
      permission_manager_(exclusive_access_context) {}

ExclusiveAccessManager::~ExclusiveAccessManager() = default;

ExclusiveAccessBubbleType
ExclusiveAccessManager::GetExclusiveAccessExitBubbleType() const {
  // In kiosk and exclusive app mode we always want to be fullscreen and do not
  // want to show exit instructions for browser mode fullscreen.
  bool app_mode = false;
#if !BUILDFLAG(IS_MAC)  // App mode (kiosk) is not available on Mac yet.
  app_mode = IsRunningInAppMode();
#endif

  if (fullscreen_controller_.IsWindowFullscreenForTabOrPending()) {
    if (!fullscreen_controller_.IsTabFullscreen())
      return EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_EXIT_INSTRUCTION;

    if (pointer_lock_controller_.IsPointerLockedSilently()) {
      return EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE;
    }

    if (keyboard_lock_controller_.RequiresPressAndHoldEscToExit())
      return EXCLUSIVE_ACCESS_BUBBLE_TYPE_KEYBOARD_LOCK_EXIT_INSTRUCTION;

    if (pointer_lock_controller_.IsPointerLocked()) {
      return EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_POINTERLOCK_EXIT_INSTRUCTION;
    }

    return EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_EXIT_INSTRUCTION;
  }

  if (pointer_lock_controller_.IsPointerLockedSilently()) {
    return EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE;
  }

  if (pointer_lock_controller_.IsPointerLocked()) {
    return EXCLUSIVE_ACCESS_BUBBLE_TYPE_POINTERLOCK_EXIT_INSTRUCTION;
  }

  if (fullscreen_controller_.IsExtensionFullscreenOrPending())
    return EXCLUSIVE_ACCESS_BUBBLE_TYPE_EXTENSION_FULLSCREEN_EXIT_INSTRUCTION;

  if (fullscreen_controller_.IsControllerInitiatedFullscreen() && !app_mode)
    return EXCLUSIVE_ACCESS_BUBBLE_TYPE_BROWSER_FULLSCREEN_EXIT_INSTRUCTION;

  return EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE;
}

void ExclusiveAccessManager::UpdateBubble(
    ExclusiveAccessBubbleHideCallback first_hide_callback,
    bool force_update) {
  exclusive_access_context_->UpdateExclusiveAccessBubble(
      {.url = GetExclusiveAccessBubbleURL(),
       .type = GetExclusiveAccessExitBubbleType(),
       .force_update = force_update},
      std::move(first_hide_callback));
}

GURL ExclusiveAccessManager::GetExclusiveAccessBubbleURL() const {
  GURL result = fullscreen_controller_.GetURLForExclusiveAccessBubble();
  if (!result.is_valid())
    result = pointer_lock_controller_.GetURLForExclusiveAccessBubble();
  return result;
}

void ExclusiveAccessManager::RecordLockStateOnEnteringApiFullscreen() const {
  RecordLockStateOnEnteringFullscreen(
      kHistogramFullscreenLockStateAtEntryViaApi);
}

void ExclusiveAccessManager::RecordLockStateOnEnteringBrowserFullscreen()
    const {
  RecordLockStateOnEnteringFullscreen(
      kHistogramFullscreenLockStateAtEntryViaBrowserUi);
}

void ExclusiveAccessManager::OnTabDeactivated(WebContents* web_contents) {
  for (auto controller : exclusive_access_controllers_) {
    controller->OnTabDeactivated(web_contents);
  }
}

void ExclusiveAccessManager::OnTabDetachedFromView(WebContents* web_contents) {
  for (auto controller : exclusive_access_controllers_) {
    controller->OnTabDetachedFromView(web_contents);
  }
}

void ExclusiveAccessManager::OnTabClosing(WebContents* web_contents) {
  for (auto controller : exclusive_access_controllers_) {
    controller->OnTabClosing(web_contents);
  }
}

bool ExclusiveAccessManager::HandleUserKeyEvent(
    const input::NativeWebKeyboardEvent& event) {
  if (event.windows_key_code != ui::VKEY_ESCAPE) {
    OnUserInput();
    return false;
  }

  // When `features::kPressAndHoldEscToExitBrowserFullscreen` is enabled, the
  // `esc_key_hold_timer_` starts on `kRawKeyDown` events, unless the key press
  // event comes with a modifier key. This metrics records how often the timer
  // does not start due to using the modifier key.
  if (event.GetType() == input::NativeWebKeyboardEvent::Type::kRawKeyDown) {
    base::UmaHistogramBoolean(
        kHistogramEscKeyPressedDownWithModifier,
        event.GetModifiers() != blink::WebInputEvent::kNoModifiers);
  }

  if (base::FeatureList::IsEnabled(
          features::kPressAndHoldEscToExitBrowserFullscreen)) {
    if (event.GetType() == input::NativeWebKeyboardEvent::Type::kKeyUp &&
        esc_key_hold_timer_.IsRunning()) {
      esc_key_hold_timer_.Stop();
      show_exit_bubble_timer_.Stop();
      for (auto controller : exclusive_access_controllers_) {
        controller->HandleUserReleasedEscapeEarly();
      }
    } else if (IsUnmodifiedEscKeyDownEvent(event) &&
               !esc_key_hold_timer_.IsRunning()) {
      esc_key_hold_timer_.Start(
          FROM_HERE, kHoldEscapeTime,
          base::BindOnce(&ExclusiveAccessManager::HandleUserHeldEscape,
                         base::Unretained(this)));
      show_exit_bubble_timer_.Start(
          FROM_HERE, kShowExitBubbleTime,
          base::BindOnce(&ExclusiveAccessManager::UpdateBubble,
                         base::Unretained(this), base::NullCallback(),
                         /*force_update=*/true));
    }
    // If the keyboard lock is enabled and requires press-and-hold Esc to exit,
    // do not pass the event to other controllers. Returns false as we don't
    // want to prevent the event from propagating to the webpage.
    if (keyboard_lock_controller_.RequiresPressAndHoldEscToExit()) {
      return false;
    }
  } else {
    // Give the `keyboard_lock_controller_` first chance at handling the Esc
    // event as there are specific UX behaviors that occur when that mode is
    // active which are coordinated by that class.  Return false as we don't
    // want to prevent the event from propagating to the webpage.
    if (keyboard_lock_controller_.HandleKeyEvent(event)) {
      return false;
    }
  }

  bool handled = false;
  for (auto controller : exclusive_access_controllers_) {
    if (controller->HandleUserPressedEscape()) {
      handled = true;
    }
  }
  return handled;
}

void ExclusiveAccessManager::OnUserInput() {
  exclusive_access_context_->OnExclusiveAccessUserInput();
}

void ExclusiveAccessManager::ExitExclusiveAccess() {
  for (auto controller : exclusive_access_controllers_) {
    controller->ExitExclusiveAccessToPreviousState();
  }
}

void ExclusiveAccessManager::HandleUserHeldEscape() {
  for (auto controller : exclusive_access_controllers_) {
    controller->HandleUserHeldEscape();
  }
}

void ExclusiveAccessManager::RecordLockStateOnEnteringFullscreen(
    const char histogram_name[]) const {
  LockState lock_state = LockState::kUnlocked;
  if (keyboard_lock_controller_.IsKeyboardLockActive()) {
    if (pointer_lock_controller_.IsPointerLocked()) {
      lock_state = LockState::kKeyboardAndPointerLocked;
    } else {
      lock_state = LockState::kKeyboardLocked;
    }
  } else if (pointer_lock_controller_.IsPointerLocked()) {
    lock_state = LockState::kPointerLocked;
  }
  base::UmaHistogramEnumeration(histogram_name, lock_state);
  if (fullscreen_controller_.exclusive_access_tab()) {
    ukm::SourceId source_id = fullscreen_controller_.exclusive_access_tab()
                                  ->GetPrimaryMainFrame()
                                  ->GetPageUkmSourceId();
    ukm::builders::Fullscreen_Enter(source_id)
        .SetLockState(static_cast<int64_t>(lock_state))
        .Record(ukm::UkmRecorder::Get());
  }
}
