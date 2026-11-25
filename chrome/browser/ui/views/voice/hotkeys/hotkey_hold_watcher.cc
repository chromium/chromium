// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/voice/hotkeys/hotkey_hold_watcher.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/types/event_type.h"
#include "ui/views/event_monitor.h"
#include "ui/views/widget/widget.h"

namespace voice {

// static
bool HotkeyHoldWatcher::IsChordKey(const ui::Accelerator& hotkey,
                                   ui::KeyboardCode key_code) {
  if (key_code == hotkey.key_code()) {
    return true;
  }
  const int modifiers = hotkey.modifiers();
  if ((modifiers & ui::EF_SHIFT_DOWN) &&
      (key_code == ui::VKEY_SHIFT || key_code == ui::VKEY_LSHIFT ||
       key_code == ui::VKEY_RSHIFT)) {
    return true;
  }
  if ((modifiers & ui::EF_CONTROL_DOWN) &&
      (key_code == ui::VKEY_CONTROL || key_code == ui::VKEY_LCONTROL ||
       key_code == ui::VKEY_RCONTROL)) {
    return true;
  }
  if ((modifiers & ui::EF_ALT_DOWN) &&
      (key_code == ui::VKEY_MENU || key_code == ui::VKEY_LMENU ||
       key_code == ui::VKEY_RMENU)) {
    return true;
  }
  if ((modifiers & ui::EF_COMMAND_DOWN) &&
      (key_code == ui::VKEY_COMMAND || key_code == ui::VKEY_LWIN ||
       key_code == ui::VKEY_RWIN || key_code == ui::VKEY_APPS)) {
    return true;
  }
  return false;
}

HotkeyHoldWatcher::HotkeyHoldWatcher(
    BrowserWindowInterface& browser,
    views::Widget& browser_widget,
    const ui::Accelerator& hotkey,
    base::TimeDelta hold_threshold,
    base::OnceClosure on_hold_threshold_reached,
    CompletionCallback completion_callback)
    : hotkey_(hotkey),
      on_hold_threshold_reached_(std::move(on_hold_threshold_reached)),
      completion_callback_(std::move(completion_callback)) {
  CHECK(!hotkey_.IsEmpty());
  CHECK(completion_callback_);
  CHECK(browser_widget.GetNativeWindow());

  widget_observation_.Observe(&browser_widget);

  // `base::Unretained(this)` is safe below because `this` owns the
  // subscriptions and `hold_threshold_timer_`.
  browser_inactive_subscription_ = browser.RegisterDidBecomeInactive(
      base::BindRepeating(&HotkeyHoldWatcher::OnBrowserDidBecomeInactive,
                          base::Unretained(this)));

  event_monitor_ = views::EventMonitor::CreateApplicationMonitor(
      this, browser_widget.GetNativeWindow(),
      {ui::EventType::kKeyPressed, ui::EventType::kKeyReleased});

  hold_threshold_timer_.Start(
      FROM_HERE, hold_threshold,
      base::BindOnce(&HotkeyHoldWatcher::OnHoldThresholdReached,
                     base::Unretained(this)));
}

HotkeyHoldWatcher::~HotkeyHoldWatcher() = default;

void HotkeyHoldWatcher::OnHoldThresholdReached() {
  is_press_and_hold_mode_ = true;
  if (on_hold_threshold_reached_) {
    std::move(on_hold_threshold_reached_).Run();
    // WARNING: `this` may be deleted by `on_hold_threshold_reached_`, do not
    // add code below here.
  }
}

void HotkeyHoldWatcher::OnEvent(const ui::Event& event) {
  if (!event.IsKeyEvent()) {
    return;
  }
  const ui::KeyEvent& key_event = *event.AsKeyEvent();

  if (key_event.type() == ui::EventType::kKeyReleased) {
    if (IsChordKey(hotkey_, key_event.key_code())) {
      if (is_press_and_hold_mode_) {
        FinishAndDestroyThis(Result::kReleased);
        return;
      }
      // Released before the hold threshold elapsed.
      FinishAndDestroyThis(Result::kTapped);
      return;
    }
    return;
  }

  if (key_event.type() == ui::EventType::kKeyPressed) {
    // Auto-repeats of the held hotkey or its modifiers (as well as synthetic
    // IME composition key events) are expected while holding. Any secondary key
    // press outside the chord terminates the hold.
    if (key_event.key_code() != ui::VKEY_PROCESSKEY &&
        !IsChordKey(hotkey_, key_event.key_code())) {
      AbortAndDestroyThis();
      return;
    }
  }
}

void HotkeyHoldWatcher::OnWidgetDestroying(views::Widget* widget) {
  event_monitor_.reset();
  widget_observation_.Reset();
  AbortAndDestroyThis();
}

void HotkeyHoldWatcher::OnBrowserDidBecomeInactive(
    BrowserWindowInterface* browser) {
  AbortAndDestroyThis();
}

void HotkeyHoldWatcher::AbortAndDestroyThis() {
  FinishAndDestroyThis(is_press_and_hold_mode_ ? Result::kAborted
                                               : Result::kTapped);
}

void HotkeyHoldWatcher::FinishAndDestroyThis(Result result) {
  if (!completion_callback_) {
    return;
  }
  hold_threshold_timer_.Stop();
  event_monitor_.reset();
  std::move(completion_callback_).Run(result);
  // WARNING: `this` is deleted by `completion_callback_`, do not add code below
  // here.
}

}  // namespace voice
