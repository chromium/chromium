// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_RECENTLY_AUDIBLE_HELPER_H_
#define CHROME_BROWSER_UI_RECENTLY_AUDIBLE_HELPER_H_

#include <optional>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace base {
class TickClock;
}

// A helper that observes WebContents audibility and calculates whether or not a
// WebContents is recently audible. This is used to make the "audio playing"
// icon persist for a short period after audio stops. Supports attachment to a
// `tabs::TabInterface` (via `ui::ScopedUnownedUserData`) or directly to a
// non-tab `content::WebContents` (via `CreateForWebContents()` /
// `content::WebContentsUserData`). This class is only safe to use from the UI
// thread.
class RecentlyAudibleHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<RecentlyAudibleHelper> {
 public:
  DECLARE_USER_DATA(RecentlyAudibleHelper);

  // This corresponds to the amount of time that the "audio playing" icon will
  // persist in the tab strip after audio has stopped playing.
  static constexpr base::TimeDelta kRecentlyAudibleTimeout = base::Seconds(2);

  using CallbackList =
      base::RepeatingCallbackList<void(bool was_recently_audible)>;
  using Callback = CallbackList::CallbackType;

  RecentlyAudibleHelper(tabs::TabInterface& tab,
                        content::WebContents* contents);
  RecentlyAudibleHelper(const RecentlyAudibleHelper&) = delete;
  RecentlyAudibleHelper& operator=(const RecentlyAudibleHelper&) = delete;

  ~RecentlyAudibleHelper() override;

  // Creates `RecentlyAudibleHelper` and attaches it to `contents` for non-tab
  // `WebContents` that do not have an external owner.
  static void CreateForWebContents(content::WebContents* contents);
  static RecentlyAudibleHelper* GetOrCreateForWebContents(
      content::WebContents* contents);
  static RecentlyAudibleHelper* From(tabs::TabInterface* tab);

  // Retrieves the helper for `contents`, checking `tabs::TabInterface` first
  // and falling back to `content::WebContentsUserData`.
  static RecentlyAudibleHelper* FromWebContents(content::WebContents* contents);

  // Returns true if the WebContents was ever audible over its lifetime.
  bool WasEverAudible() const;

  // Returns true if the WebContents is currently audible.
  bool IsCurrentlyAudible() const;

  // Returns true if the WebContents is currently audible, or was audible
  // recently.
  bool WasRecentlyAudible() const;

  // Registers the provided repeating callback for notifications. Destroying
  // the returned subscription will unregister the callback. This is safe to do
  // while in the context of the callback itself.
  base::CallbackListSubscription RegisterRecentlyAudibleChangedCallback(
      const Callback& callback);

  // Allows replacing the tick clock that is used by this class. Setting it back
  // to nullptr will restore the default tick clock.
  void SetTickClockForTesting(const base::TickClock* tick_clock);

  // State transition functions for testing. These do not invoke callbacks but
  // modify state such that WasEverAudible/IsCurrentlyAudible/WasRecentlyAudible
  // will return as expected. They also ensure the internal state of the timer
  // is as expected.
  void SetCurrentlyAudibleForTesting();
  void SetRecentlyAudibleForTesting();
  void SetNotRecentlyAudibleForTesting();

  // Simulates the recently audible timer firing.
  void FireRecentlyAudibleTimerForTesting();

 private:
  friend class RecentlyAudibleHelperTest;
  friend class content::WebContentsUserData<RecentlyAudibleHelper>;

  explicit RecentlyAudibleHelper(content::WebContents* contents);
  RecentlyAudibleHelper(tabs::TabInterface* tab,
                        content::WebContents* contents);

  // contents::WebContentsObserver implementation:
  void OnAudioStateChanged(bool audible) override;

  // The callback that is invoked by the |recently_audible_timer_|.
  void OnRecentlyAudibleTimerFired();

  // Transitions to not being audible and starts the timer.
  void TransitionToNotCurrentlyAudible();

  // is_null() if the tab has never been audible, and is_max() if audio is
  // currently playing. Otherwise, corresponds to the last time the tab was
  // audible.
  base::TimeTicks last_audible_time_;

  // Timer for determining when "recently audible" transitions to false. This
  // starts running when a tab stops being audible, and is canceled if it starts
  // being audible again before it fires.
  base::OneShotTimer recently_audible_timer_;

  // List of callbacks observing this helper.
  CallbackList callback_list_;

  // The tick clock this object is using.
  raw_ptr<const base::TickClock> tick_clock_;

  std::optional<ui::ScopedUnownedUserData<RecentlyAudibleHelper>>
      scoped_unowned_user_data_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_UI_RECENTLY_AUDIBLE_HELPER_H_
