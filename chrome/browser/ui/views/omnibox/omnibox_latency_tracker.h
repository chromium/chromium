// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_LATENCY_TRACKER_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_LATENCY_TRACKER_H_

#include <optional>

#include "base/auto_reset.h"
#include "base/memory/raw_ref.h"
#include "base/time/time.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class BrowserWindowInterface;

namespace content {
class WebContents;
}  // namespace content

namespace ui {
class UnownedUserDataHost;
}  // namespace ui

// Measures the omnibox latency journeys of one browser window. Each normal
// browser window has one, owned by its `LocationBarView` and found with
// `From()`. Popup and app windows have none.
//
// Callers only notify the tracker of what happens to the window and its
// omnibox, with `Notify()` and `NotifyOmniboxFocused()`. The tracker decides
// when each journey starts, ends or is dropped. The only journey so far is
// StartupToReady:
//  - It starts at the application start, on the first show of the first
//    browser window shown in the process.
//  - It ends when the omnibox is auto-focused for an NTP, which only happens
//    while the window's first activation restores the focus, or while the
//    Views omnibox hands its focus off to the Full WebUI omnibox once that's
//    ready. When the Full WebUI omnibox is enabled, the Views omnibox only
//    holds the focus until then, so its focus doesn't end the journey.
//  - It's dropped if the user focuses the omnibox or moves the focus first, or
//    if the window is shown minimized or deactivated before the omnibox is
//    ready.
class OmniboxLatencyTracker {
 public:
  DECLARE_USER_DATA(OmniboxLatencyTracker);

  // The measured journeys. Each one's name is its histogram name token.
  enum class Cuj {
    // From the application start until the startup window's omnibox is ready.
    kStartupToReady,
  };

  // The UI presenting the omnibox.
  enum class Ui {
    kViews,
    kFullWebUI,
  };

  // What happens to the window or its omnibox.
  enum class Event {
    // The window is about to be shown. Sent for every show.
    kWindowShowing,
    // The window was shown minimized.
    kWindowShownMinimized,
    kWindowDeactivated,
    // An activation of the window starts and ends restoring the focus, e.g. on
    // its first activation. This can focus the omnibox.
    kFocusRestoreStarted,
    kFocusRestoreEnded,
    // The Views omnibox starts and ends handing its focus off to the Full WebUI
    // omnibox, once that's ready.
    kFullWebUIHandoffStarted,
    kFullWebUIHandoffEnded,
    // The Full WebUI omnibox is ready, but the Views omnibox has no focus to
    // hand off.
    kFullWebUIReadyWithoutFocus,
    // The user focused the omnibox, e.g. with a click or a keyboard shortcut.
    kUserFocusedOmnibox,
  };

  // A journey that ends on the next presented frame that shows the omnibox:
  // the histogram that records its latency, and when it started.
  struct EndingJourney {
    const char* histogram_name = nullptr;
    base::TimeTicks start_time;
  };

  // The window and omnibox state that some events depend on.
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Whether the Views omnibox holds the focus until the Full WebUI omnibox is
    // ready to take it over.
    virtual bool IsAwaitingFullWebUIFocusHandoff() const = 0;
    virtual bool IsWindowActive() const = 0;
  };

  // Registers the tracker in `host`, the user data host of its browser window.
  OmniboxLatencyTracker(ui::UnownedUserDataHost& host,
                        const Delegate& delegate);
  OmniboxLatencyTracker(const OmniboxLatencyTracker&) = delete;
  OmniboxLatencyTracker& operator=(const OmniboxLatencyTracker&) = delete;
  ~OmniboxLatencyTracker();

  // Returns the tracker of `browser`, or null if it has none, e.g. for popup
  // and app windows.
  static OmniboxLatencyTracker* From(BrowserWindowInterface* browser);

  // Notifies `browser`'s tracker of `event`. Windows without a tracker can
  // still be the startup window, so this also handles `kWindowShowing` for
  // them.
  static void Notify(BrowserWindowInterface* browser, Event event);
  void Notify(Event event);

  // Notifies the tracker that the omnibox shows its focused state, with its
  // caret, for `active_contents`. Returns the journey that this ends, if any,
  // for `ui` to record with the presentation time of the next frame that
  // shows it.
  static std::optional<EndingJourney> NotifyOmniboxFocused(
      BrowserWindowInterface* browser,
      Ui ui,
      content::WebContents* active_contents);
  std::optional<EndingJourney> NotifyOmniboxFocused(
      Ui ui,
      content::WebContents* active_contents);

  bool HasOngoingJourneyForTesting() const {
    return ongoing_journey_.has_value();
  }
  // Sets or clears the ongoing StartupToReady journey.
  void SetOngoingJourneyForTesting(std::optional<base::TimeTicks> start_time);
  static void ResetStartupWindowForTesting();
  // Overrides the application start, which unit tests don't have, until the
  // returned object is destroyed.
  [[nodiscard]] static base::AutoReset<std::optional<base::TimeTicks>>
  SetApplicationStartForTesting(base::TimeTicks application_start);

 private:
  // A journey that started and hasn't ended yet: which journey, and when it
  // started (the application start for StartupToReady).
  struct OngoingJourney {
    Cuj cuj = Cuj::kStartupToReady;
    base::TimeTicks start_time;
  };

  void OnWindowShowing();
  // Called after a focus change that can auto-focus the omnibox. Drops the
  // ongoing journey once no such change is in progress, unless `keep_journey`.
  void EndAutoFocus(bool keep_journey);
  // Drops the ongoing journey, if any, and traces `reason`.
  void DiscardOngoingJourney(const char* reason);

  ui::ScopedUnownedUserData<OmniboxLatencyTracker> scoped_data_holder_;
  const raw_ref<const Delegate> delegate_;

  bool window_shown_ = false;
  std::optional<OngoingJourney> ongoing_journey_;
  // How many focus changes that can auto-focus the omnibox are in progress.
  int auto_focus_depth_ = 0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_LATENCY_TRACKER_H_
