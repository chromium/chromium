// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_VOICE_HOTKEYS_HOTKEY_HOLD_WATCHER_H_
#define CHROME_BROWSER_UI_VIEWS_VOICE_HOTKEYS_HOTKEY_HOLD_WATCHER_H_

#include <memory>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/event_observer.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/widget/widget_observer.h"

class BrowserWindowInterface;

namespace views {
class EventMonitor;
class Widget;
}  // namespace views

namespace voice {

// Temporary, observe-only watcher created when a voice feature is triggered by
// a press-and-hold capable hotkey. Releasing the primary key or any modifier
// key within `hold_threshold` of the press reports `Result::kTapped`. Once
// held for at least `hold_threshold`, releasing it reports `Result::kReleased`,
// and losing track of the hold (blur, tab switch, secondary key press) reports
// `Result::kAborted`.
class HotkeyHoldWatcher : public ui::EventObserver,
                          public views::WidgetObserver {
 public:
  enum class Result {
    // The hotkey was released, or the watch was aborted, before
    // `hold_threshold`.
    kTapped,
    // The hotkey was held for at least `hold_threshold`, then released.
    kReleased,
    // The hotkey was held for at least `hold_threshold`, then the watch was
    // aborted, e.g. because the browser window became inactive.
    kAborted,
  };
  using CompletionCallback = base::OnceCallback<void(Result)>;

  // `on_hold_threshold_reached`, if set, runs once the hotkey has been held for
  // `hold_threshold`. Both callbacks run at most once, and may destroy `this`.
  HotkeyHoldWatcher(BrowserWindowInterface& browser,
                    tabs::TabInterface& tab,
                    views::Widget& browser_widget,
                    const ui::Accelerator& hotkey,
                    base::TimeDelta hold_threshold,
                    base::OnceClosure on_hold_threshold_reached,
                    CompletionCallback completion_callback);
  HotkeyHoldWatcher(const HotkeyHoldWatcher&) = delete;
  HotkeyHoldWatcher& operator=(const HotkeyHoldWatcher&) = delete;
  ~HotkeyHoldWatcher() override;

  // Returns true if `key_code` is the main key or one of the required
  // modifier keys of `hotkey`.
  static bool IsChordKey(const ui::Accelerator& hotkey,
                         ui::KeyboardCode key_code);

  bool is_press_and_hold_mode() const { return is_press_and_hold_mode_; }
  base::TimeDelta hold_threshold_for_testing() const {
    return hold_threshold_timer_.GetCurrentDelay();
  }

  // ui::EventObserver:
  void OnEvent(const ui::Event& event) override;

 private:
  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  void OnHoldThresholdReached();
  void OnBrowserDidBecomeInactive(BrowserWindowInterface* browser);
  void OnTabWillDeactivate(tabs::TabInterface* tab);
  void OnTabWillDetach(tabs::TabInterface* tab,
                       tabs::TabInterface::DetachReason reason);
  void AbortAndDestroyThis();
  void FinishAndDestroyThis(Result result);

  const ui::Accelerator hotkey_;
  base::OnceClosure on_hold_threshold_reached_;
  CompletionCallback completion_callback_;
  bool is_press_and_hold_mode_ = false;
  base::OneShotTimer hold_threshold_timer_;

  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};
  base::CallbackListSubscription browser_inactive_subscription_;
  base::CallbackListSubscription tab_deactivate_subscription_;
  base::CallbackListSubscription tab_detach_subscription_;

  std::unique_ptr<views::EventMonitor> event_monitor_;
};

}  // namespace voice

#endif  // CHROME_BROWSER_UI_VIEWS_VOICE_HOTKEYS_HOTKEY_HOLD_WATCHER_H_
