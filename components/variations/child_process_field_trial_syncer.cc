// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/child_process_field_trial_syncer.h"

#include <utility>

#include "base/auto_reset.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/metrics/field_trial_list_including_low_anonymity.h"
#include "components/variations/variations_crash_keys.h"

namespace variations {

namespace {

ChildProcessFieldTrialSyncer* g_instance = nullptr;
constinit thread_local bool in_set_field_trial_group_from_browser = false;

}  // namespace

// static
ChildProcessFieldTrialSyncer* ChildProcessFieldTrialSyncer::CreateInstance(
    FieldTrialActivatedCallback activated_callback) {
  CHECK(!g_instance);
  g_instance = new ChildProcessFieldTrialSyncer(std::move(activated_callback));
  g_instance->Init(base::FieldTrialList::GetActiveTrialsOfParentProcess());
  return g_instance;
}

// static
ChildProcessFieldTrialSyncer*
ChildProcessFieldTrialSyncer::CreateInstanceForTesting(
    const std::set<std::string>& initially_active_trials,
    FieldTrialActivatedCallback activated_callback) {
  CHECK(!g_instance);
  g_instance = new ChildProcessFieldTrialSyncer(std::move(activated_callback));
  g_instance->Init(initially_active_trials);
  return g_instance;
}

// static
void ChildProcessFieldTrialSyncer::DeleteInstanceForTesting() {
  CHECK(g_instance);
  delete g_instance;
  g_instance = nullptr;
  // Revert the effect of calling variations::InitCrashKeys() in Init().
  ClearCrashKeysInstanceForTesting();  // IN-TEST
}

ChildProcessFieldTrialSyncer::ChildProcessFieldTrialSyncer(
    FieldTrialActivatedCallback activated_callback)
    : activated_callback_(std::move(activated_callback)) {}

ChildProcessFieldTrialSyncer::~ChildProcessFieldTrialSyncer() = default;

void ChildProcessFieldTrialSyncer::Init(
    const std::set<std::string>& initially_active_trials) {
  // Set up initial set of crash dump data for field trials in this process.
  variations::InitCrashKeys();

  // Listen for field trial activations to report them to the browser.
  // All trials (including low anonymity ones) are required so that the browser
  // and child processes have a consistent view.
  //
  // See also TODO(crbug.com/40263398) at
  // |FieldTrialSynchronizer::FieldTrialSynchronizer()|.
  base::FieldTrialListIncludingLowAnonymity::AddObserver(this);

  // Some field trials may have been activated before this point. Notify the
  // browser of these activations now. To detect these, take the set difference
  // of currently active trials with the initially active trials.
  base::FieldTrial::ActiveGroups current_active_trials;
  base::FieldTrialListIncludingLowAnonymity::GetActiveFieldTrialGroups(
      &current_active_trials);
  for (const auto& trial : current_active_trials) {
    if (!base::Contains(initially_active_trials, trial.trial_name)) {
      activated_callback_.Run(trial.trial_name);
    }
  }
}

void ChildProcessFieldTrialSyncer::SetFieldTrialGroupFromBrowser(
    const std::string& trial_name,
    const std::string& group_name) {
  const base::AutoReset<bool> resetter(&in_set_field_trial_group_from_browser,
                                       true, false);

  base::FieldTrial* trial =
      base::FieldTrialList::CreateFieldTrial(trial_name, group_name);
  // Ensure the trial is marked as "used" by calling Activate() on it if it is
  // marked as activated.
  trial->Activate();
}

void ChildProcessFieldTrialSyncer::OnFieldTrialGroupFinalized(
    const base::FieldTrial& trial,
    const std::string& group_name) {
  // It is not necessary to notify the browser if this is invoked from
  // SetFieldTrialGroupFromBrowser().
  if (!in_set_field_trial_group_from_browser) {
    activated_callback_.Run(trial.trial_name());
  }
}

}  // namespace variations
