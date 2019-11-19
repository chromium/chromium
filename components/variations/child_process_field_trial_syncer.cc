// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/child_process_field_trial_syncer.h"

#include <set>
#include <utility>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "components/variations/variations_crash_keys.h"

namespace variations {

ChildProcessFieldTrialSyncer::ChildProcessFieldTrialSyncer(
    base::FieldTrialList::Observer* observer)
    : observer_(observer) {}

ChildProcessFieldTrialSyncer::~ChildProcessFieldTrialSyncer() {}

void ChildProcessFieldTrialSyncer::InitFieldTrialObserving(
    const base::CommandLine& command_line) {
  // Set up initial set of crash dump data for field trials in this process.
  variations::InitCrashKeys();

  // Listen for field trial activations to report them to the browser.
  base::FieldTrialList::AddObserver(observer_);

  // Some field trials may have been activated before this point. Notify the
  // browser of these activations now. To detect these, take the set difference
  // of currently active trials with the initially active trials.
  base::FieldTrial::ActiveGroups initially_active_trials;
  base::FieldTrialList::GetInitiallyActiveFieldTrials(command_line,
                                                      &initially_active_trials);
  std::set<std::string> initially_active_trials_set;
  for (const auto& entry : initially_active_trials) {
    initially_active_trials_set.insert(std::move(entry.trial_name));
  }

  base::FieldTrial::ActiveGroups current_active_trials;
  base::FieldTrialList::GetActiveFieldTrialGroups(&current_active_trials);
  for (const auto& trial : current_active_trials) {
    if (!base::Contains(initially_active_trials_set, trial.trial_name))
      observer_->OnFieldTrialGroupFinalized(trial.trial_name, trial.group_name);
  }
}

void ChildProcessFieldTrialSyncer::OnSetFieldTrialGroup(
    const std::string& trial_name,
    const std::string& group_name) {
  base::FieldTrial* trial =
      base::FieldTrialList::CreateFieldTrial(trial_name, group_name);
  // Ensure the trial is marked as "used" by calling group() on it if it is
  // marked as activated.
  trial->group();
}

}  // namespace variations
