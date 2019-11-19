// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/in_product_help/active_tab_tracker.h"

#include <utility>

#include "base/time/tick_clock.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"

ActiveTabTracker::ActiveTabTracker(const base::TickClock* clock,
                                   ActiveTabClosedCallback callback)
    : clock_(clock), active_tab_closed_callback_(std::move(callback)) {
  DCHECK(active_tab_closed_callback_);
}

ActiveTabTracker::~ActiveTabTracker() {
  // All tab strip models should have been removed before destruction.
  DCHECK(active_tab_changed_times_.empty());
}

void ActiveTabTracker::AddTabStripModel(TabStripModel* tab_strip_model) {
  active_tab_changed_times_[tab_strip_model] = clock_->NowTicks();
  tab_strip_model->AddObserver(this);
}

void ActiveTabTracker::RemoveTabStripModel(TabStripModel* tab_strip_model) {
  // Get |std::map| iterator in |active_tab_changed_times_|.
  auto it = active_tab_changed_times_.find(tab_strip_model);
  DCHECK(it != active_tab_changed_times_.end());

  // Stop observing and remove map element.
  tab_strip_model->RemoveObserver(this);
  active_tab_changed_times_.erase(it);
}

void ActiveTabTracker::OnTabStripModelChanged(
    TabStripModel* model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  DCHECK(active_tab_changed_times_.find(model) !=
         active_tab_changed_times_.end());

  const int prev_active_tab_index = selection.old_model.active();

  if (change.type() == TabStripModelChange::Type::kRemoved) {
    auto* remove = change.GetRemove();
    // If the closing tab was the active tab, call the callback.
    // Ignore if the tab isn't being closed (this would happen if it were
    // dragged to a different tab strip).
    if (remove->will_be_deleted) {
      for (const auto& contents : remove->contents) {
        if (contents.index == prev_active_tab_index) {
          active_tab_closed_callback_.Run(
              model, clock_->NowTicks() - active_tab_changed_times_[model]);
        }
      }
    }
  }

  if (selection.active_tab_changed())
    active_tab_changed_times_[model] = clock_->NowTicks();
}
