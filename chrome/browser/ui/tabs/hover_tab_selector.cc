// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/hover_tab_selector.h"

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_user_gesture_details.h"

HoverTabSelector::HoverTabSelector(TabStripModel* tab_strip_model)
    : tab_strip_model_(tab_strip_model), tab_transition_tab_index_(-1) {
  DCHECK(tab_strip_model_);
}

HoverTabSelector::~HoverTabSelector() {
}

void HoverTabSelector::StartTabTransition(int index) {
  // If there is a transition underway already, only start a new
  // transition (canceling the old one) if the target tab differs.
  if (weak_factory_.HasWeakPtrs()) {
    if (index == tab_transition_tab_index_)
      return;
    CancelTabTransition();
  }
  // Start a new transition if the target isn't active already.
  if (index != tab_strip_model_->active_index()) {
    // The delay between beginning to hover over a tab and the transition
    // to that tab taking place.
    const base::TimeDelta kHoverTransitionDelay = base::Milliseconds(500);
    tab_transition_tab_index_ = index;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&HoverTabSelector::PerformTabTransition,
                       weak_factory_.GetWeakPtr()),
        kHoverTransitionDelay);
  }
}

void HoverTabSelector::CancelTabTransition() {
  weak_factory_.InvalidateWeakPtrs();
}

void HoverTabSelector::PerformTabTransition() {
  DCHECK(tab_transition_tab_index_ >= 0 &&
         tab_transition_tab_index_ < tab_strip_model_->count());
  tab_strip_model_->ActivateTabAt(
      tab_transition_tab_index_,
      TabStripUserGestureDetails(
          TabStripUserGestureDetails::GestureType::kOther));
}
