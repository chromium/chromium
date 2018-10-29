// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/in_product_help/reopen_tab_in_product_help_trigger.h"

#include <utility>

#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"

namespace in_product_help {

// static
const base::TimeDelta ReopenTabInProductHelpTrigger::kTabMinimumActiveDuration =
    base::TimeDelta::FromSeconds(10);
// static
const base::TimeDelta ReopenTabInProductHelpTrigger::kNewTabOpenedTimeout =
    base::TimeDelta::FromSeconds(10);
// static
const base::TimeDelta ReopenTabInProductHelpTrigger::kOmniboxFocusedTimeout =
    base::TimeDelta::FromSeconds(10);

ReopenTabInProductHelpTrigger::ReopenTabInProductHelpTrigger(
    feature_engagement::Tracker* tracker,
    const base::TickClock* clock)
    : tracker_(tracker), clock_(clock), trigger_state_(NO_ACTIONS_SEEN) {
  DCHECK(tracker);
  DCHECK(clock);

  // Timeouts must be non-zero.
  DCHECK(!kNewTabOpenedTimeout.is_zero());
  DCHECK(!kOmniboxFocusedTimeout.is_zero());
}

ReopenTabInProductHelpTrigger::~ReopenTabInProductHelpTrigger() = default;

void ReopenTabInProductHelpTrigger::SetShowHelpCallback(
    ShowHelpCallback callback) {
  DCHECK(callback);
  cb_ = std::move(callback);
}

void ReopenTabInProductHelpTrigger::ActiveTabClosed(
    base::TimeDelta active_duration) {
  // Reset all flags at this point. We should only trigger IPH if the events
  // happen in the prescribed order.
  ResetTriggerState();

  DCHECK(active_duration >= base::TimeDelta());
  // We only go to the next state if the closing tab was active for long enough.
  if (active_duration >= kTabMinimumActiveDuration) {
    trigger_state_ = ACTIVE_TAB_CLOSED;
    time_of_last_step_ = clock_->NowTicks();
  }
}

void ReopenTabInProductHelpTrigger::NewTabOpened() {
  if (trigger_state_ != ACTIVE_TAB_CLOSED)
    return;

  const base::TimeDelta elapsed_time = clock_->NowTicks() - time_of_last_step_;

  if (elapsed_time < kNewTabOpenedTimeout) {
    trigger_state_ = NEW_TAB_OPENED;
    time_of_last_step_ = clock_->NowTicks();
  } else {
    ResetTriggerState();
  }
}

void ReopenTabInProductHelpTrigger::OmniboxFocused() {
  if (trigger_state_ != NEW_TAB_OPENED)
    return;

  const base::TimeDelta elapsed_time = clock_->NowTicks() - time_of_last_step_;

  if (elapsed_time < kOmniboxFocusedTimeout) {
    tracker_->NotifyEvent(feature_engagement::events::kReopenTabConditionsMet);
    if (tracker_->ShouldTriggerHelpUI(
            feature_engagement::kIPHReopenTabFeature)) {
      DCHECK(cb_);
      cb_.Run();
    }
  }
}

void ReopenTabInProductHelpTrigger::HelpDismissed() {
  tracker_->Dismissed(feature_engagement::kIPHReopenTabFeature);
  ResetTriggerState();
}

void ReopenTabInProductHelpTrigger::ResetTriggerState() {
  time_of_last_step_ = base::TimeTicks();
  trigger_state_ = NO_ACTIONS_SEEN;
}

}  // namespace in_product_help
