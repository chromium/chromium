// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_list_notifier_impl.h"

#include "ash/public/cpp/app_list/app_list_controller.h"
#include "base/check.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/display/types/display_constants.h"

namespace {

// TODO(crbug.com/1076270): Finalize a value for this, and possibly use
// different values for different UI surfaces.
constexpr base::TimeDelta kImpressionTimer = base::TimeDelta::FromSeconds(1);

}  // namespace

AppListNotifierImpl::AppListNotifierImpl(
    ash::AppListController* app_list_controller)
    : app_list_controller_(app_list_controller) {
  DCHECK(app_list_controller_);
  app_list_controller_->AddObserver(this);
  OnAppListVisibilityWillChange(app_list_controller_->IsVisible(base::nullopt),
                                display::kInvalidDisplayId);
}

AppListNotifierImpl::~AppListNotifierImpl() {
  app_list_controller_->RemoveObserver(this);
}

void AppListNotifierImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void AppListNotifierImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void AppListNotifierImpl::NotifyLaunched(Location location,
                                         const Result& result) {
  launched_result_ = result;

  // Only two UI views appear at once: the app tiles and results list. If a
  // launch occurs in one, mark the other as 'ignored' rather than abandoned.
  if (location == Location::kList) {
    DoStateTransition(Location::kTile, State::kIgnored);
  } else if (location == Location::kTile) {
    DoStateTransition(Location::kList, State::kIgnored);
  }

  DoStateTransition(location, State::kLaunched);
}

void AppListNotifierImpl::NotifyResultsUpdated(
    Location location,
    const std::vector<Result>& results) {
  results_[location] = results;
}

void AppListNotifierImpl::NotifySearchQueryChanged(
    const std::u16string& query) {
  // In some cases the query can change after the launcher is closed, in
  // particular this happens when abandoning the launcher with a non-empty
  // query. Only do a state transition if the launcher is open.
  if (view_ != ash::AppListViewState::kClosed) {
    DoStateTransition(Location::kList, State::kShown);
    DoStateTransition(Location::kTile, State::kShown);
  }

  // Update the stored |query_| after performing the state transitions, so that
  // an abandon triggered by the query change correctly uses the pre-abandon
  // query.
  query_ = query;

  for (auto& observer : observers_) {
    observer.OnQueryChanged(query);
  }
}

void AppListNotifierImpl::NotifyUIStateChanged(ash::AppListViewState view) {
  // We should ignore certain view state changes entirely:
  //
  //  1. noop transitions from and to the same view. These are caused by some
  //     UI actions (like dragging the launcher around) that end up back in
  //     the same location.
  //
  //  2. kHalf to kFullscreenSearch. This doesn't change the displayed tile and
  //     list results.
  //
  //  3. kPeeking to kFullscreenAllApps. This doesn't change the displayed
  //     chip results.
  //
  //  We should also ignore this if the call comes while the launcher is not
  //  shown at all. This happens, for example, in the transition between
  //  clamshell and tablet modes.
  if (!shown_ || view_ == view ||
      (view_ == ash::AppListViewState::kHalf &&
       view == ash::AppListViewState::kFullscreenSearch) ||
      (view_ == ash::AppListViewState::kPeeking &&
       view == ash::AppListViewState::kFullscreenAllApps)) {
    return;
  }
  view_ = view;

  if (view == ash::AppListViewState::kHalf ||
      view == ash::AppListViewState::kFullscreenSearch) {
    DoStateTransition(Location::kList, State::kShown);
    DoStateTransition(Location::kTile, State::kShown);
  } else {
    DoStateTransition(Location::kList, State::kNone);
    DoStateTransition(Location::kTile, State::kNone);
  }

  if (view == ash::AppListViewState::kPeeking ||
      view == ash::AppListViewState::kFullscreenAllApps) {
    DoStateTransition(Location::kChip, State::kShown);
  } else {
    DoStateTransition(Location::kChip, State::kNone);
  }
}

void AppListNotifierImpl::OnAppListVisibilityWillChange(bool shown,
                                                        int64_t display_id) {
  if (shown_ == shown)
    return;
  shown_ = shown;

  if (shown) {
    DoStateTransition(Location::kChip, State::kShown);
  } else {
    DoStateTransition(Location::kChip, State::kNone);
    DoStateTransition(Location::kList, State::kNone);
    DoStateTransition(Location::kTile, State::kNone);
  }
}

void AppListNotifierImpl::RestartTimer(Location location) {
  if (timers_.find(location) == timers_.end()) {
    timers_[location] = std::make_unique<base::OneShotTimer>();
  }

  auto& timer = timers_[location];
  if (timer->IsRunning()) {
    timer->Stop();
  }
  // base::Unretained is safe here because the timer is a member of |this|, and
  // OneShotTimer cancels its timer on destruction.
  timer->Start(FROM_HERE, kImpressionTimer,
               base::BindOnce(&AppListNotifierImpl::OnTimerFinished,
                              base::Unretained(this), location));
}

void AppListNotifierImpl::StopTimer(Location location) {
  const auto it = timers_.find(location);
  if (it != timers_.end() && it->second->IsRunning()) {
    it->second->Stop();
  }
}

void AppListNotifierImpl::OnTimerFinished(Location location) {
  DoStateTransition(location, State::kSeen);
}

void AppListNotifierImpl::DoStateTransition(Location location,
                                            State new_state) {
  const State old_state = states_[location];

  // Update most recent state. We special-case kLaunched and kIgnored, which are
  // temporary states reflecting a launch either in |location| or another view.
  // They immediately transition to kNone because the launcher closes after a
  // launch.
  if (new_state == State::kLaunched || new_state == State::kIgnored) {
    states_[location] = State::kNone;
  } else {
    states_[location] = new_state;
  }

  // These overlapping cases are equivalent to the explicit cases in the header
  // comment.

  // Restart timer on * -> kShown
  if (new_state == State::kShown) {
    RestartTimer(location);
  }

  // Stop timer on kShown -> {kNone, kLaunch}.
  if (old_state == State::kShown &&
      (new_state == State::kNone || new_state == State::kLaunched)) {
    StopTimer(location);
  }

  // Notify of impression on kShown -> {kSeen, kIgnored, kLaunched}.
  if (old_state == State::kShown &&
      (new_state == State::kSeen || new_state == State::kLaunched ||
       new_state == State::kIgnored) &&
      !results_[location].empty()) {
    for (auto& observer : observers_) {
      observer.OnImpression(location, results_[location], query_);
    }
  }

  // Notify of launch on * -> kLaunched.
  if (new_state == State::kLaunched && launched_result_.has_value()) {
    for (auto& observer : observers_) {
      observer.OnLaunch(location, launched_result_.value(), results_[location],
                        query_);
    }
  }

  // Notify of ignore on * -> kIgnored.
  if (new_state == State::kIgnored && !results_[location].empty()) {
    for (auto& observer : observers_) {
      observer.OnIgnore(location, results_[location], query_);
    }
  }

  // Notify of abandon on kSeen -> {kNone, kShown}.
  if (old_state == State::kSeen &&
      (new_state == State::kNone || new_state == State::kShown) &&
      !results_[location].empty()) {
    for (auto& observer : observers_) {
      observer.OnAbandon(location, results_[location], query_);
    }
  }
}
