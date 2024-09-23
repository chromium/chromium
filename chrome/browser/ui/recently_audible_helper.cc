// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/recently_audible_helper.h"

#include "base/no_destructor.h"
#include "base/time/default_tick_clock.h"

namespace {

const base::TickClock* GetDefaultTickClock() {
  static base::NoDestructor<base::DefaultTickClock> default_tick_clock;
  return default_tick_clock.get();
}

}  // namespace

// static
constexpr base::TimeDelta RecentlyAudibleHelper::kRecentlyAudibleTimeout;

RecentlyAudibleHelper::~RecentlyAudibleHelper() = default;

bool RecentlyAudibleHelper::WasEverAudible() const {
  return !last_audible_time_.is_null();
}

bool RecentlyAudibleHelper::IsCurrentlyAudible() const {
  return last_audible_time_.is_max();
}

bool RecentlyAudibleHelper::WasRecentlyAudible() const {
  if (last_audible_time_.is_max())
    return true;
  if (last_audible_time_.is_null())
    return false;
  base::TimeTicks recently_audible_time_limit =
      last_audible_time_ + kRecentlyAudibleTimeout;
  return tick_clock_->NowTicks() < recently_audible_time_limit;
}

base::CallbackListSubscription
RecentlyAudibleHelper::RegisterCallbackForTesting(const Callback& callback) {
  return callback_list_.Add(callback);
}

RecentlyAudibleHelper::RecentlyAudibleHelper(content::WebContents* contents)
    : content::WebContentsObserver(contents),
      content::WebContentsUserData<RecentlyAudibleHelper>(*contents),
      tick_clock_(GetDefaultTickClock()) {
  if (contents->IsCurrentlyAudible())
    last_audible_time_ = base::TimeTicks::Max();
}

void RecentlyAudibleHelper::OnAudioStateChanged(bool audible) {
  // Redundant notifications should never happen.
  DCHECK(audible != IsCurrentlyAudible());

  // If audio is stopping remember the time at which it stopped and set a timer
  // to fire the recently audible transition.
  if (!audible) {
    TransitionToNotCurrentlyAudible();
    return;
  }

  // If the tab was not recently audible prior to the audio starting then notify
  // that it has become recently audible again. Otherwise, swallow this
  // notification.
  bool was_recently_audible = WasRecentlyAudible();
  last_audible_time_ = base::TimeTicks::Max();
  recently_audible_timer_.Stop();
  if (!was_recently_audible)
    callback_list_.Notify(true);
}

void RecentlyAudibleHelper::OnRecentlyAudibleTimerFired() {
  DCHECK(last_audible_time_ + kRecentlyAudibleTimeout <=
         tick_clock_->NowTicks());
  // Notify of the transition to no longer being recently audible.
  callback_list_.Notify(false);

  // This notification is redundant in most cases, because WebContents is
  // notified by AudioStreamMonitor of changes due to audio in its own frames
  // (but not in inner contents) directly.
  //
  // TODO(crbug.com/41390955): Remove this once WebContents is notified
  // via |callback_list_| in this class instead.
  web_contents()->NotifyNavigationStateChanged(content::INVALIDATE_TYPE_AUDIO);
}

void RecentlyAudibleHelper::TransitionToNotCurrentlyAudible() {
  last_audible_time_ = tick_clock_->NowTicks();
  recently_audible_timer_.Start(
      FROM_HERE, kRecentlyAudibleTimeout, this,
      &RecentlyAudibleHelper::OnRecentlyAudibleTimerFired);
}

void RecentlyAudibleHelper::SetTickClockForTesting(
    const base::TickClock* tick_clock) {
  if (tick_clock) {
    tick_clock_ = tick_clock;
  } else {
    tick_clock_ = GetDefaultTickClock();
  }
}

void RecentlyAudibleHelper::SetCurrentlyAudibleForTesting() {
  recently_audible_timer_.Stop();
  last_audible_time_ = base::TimeTicks::Max();
}

void RecentlyAudibleHelper::SetRecentlyAudibleForTesting() {
  TransitionToNotCurrentlyAudible();
}

void RecentlyAudibleHelper::SetNotRecentlyAudibleForTesting() {
  last_audible_time_ = tick_clock_->NowTicks() - kRecentlyAudibleTimeout;
  recently_audible_timer_.Stop();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(RecentlyAudibleHelper);
