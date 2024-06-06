// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/persistent_synthetic_trial_observer.h"

#include "base/task/sequenced_task_runner.h"
#include "components/metrics/persistent_system_profile.h"
#include "components/variations/synthetic_trials.h"

namespace metrics {
namespace {

void PersistSyntheticFieldTrials(
    const std::vector<variations::SyntheticTrialGroup>& trials_updated,
    const std::vector<variations::SyntheticTrialGroup>& trials_removed) {
  // Mark all the updated synthetic field trials in the persistent profile,
  // update the removed trials first, then add the updated trials.
  // TODO: crbug.com/345445618 - Handle kNextLog annotation mode properly, and
  // when the annotation mode changes.
  for (const auto& removed : trials_removed) {
    metrics::GlobalPersistentSystemProfile::GetInstance()->RemoveFieldTrial(
        removed.trial_name());
  }
  for (const auto& updated : trials_updated) {
    if (updated.annotation_mode() ==
        variations::SyntheticTrialAnnotationMode::kCurrentLog) {
      metrics::GlobalPersistentSystemProfile::GetInstance()->AddFieldTrial(
          updated.trial_name(), updated.group_name());
    }
  }
}

}  // namespace

PersistentSyntheticTrialObserver::PersistentSyntheticTrialObserver()
    : task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {}

PersistentSyntheticTrialObserver::~PersistentSyntheticTrialObserver() = default;

void PersistentSyntheticTrialObserver::OnSyntheticTrialsChanged(
    const std::vector<variations::SyntheticTrialGroup>& trials_updated,
    const std::vector<variations::SyntheticTrialGroup>& trials_removed,
    const std::vector<variations::SyntheticTrialGroup>& groups) {
  // Synthetic trials may be registered in any task runner, so switch to correct
  // task runner before storing.
  if (task_runner_->RunsTasksInCurrentSequence()) {
    PersistSyntheticFieldTrials(trials_updated, trials_removed);
  } else {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&PersistSyntheticFieldTrials, trials_updated,
                                  trials_removed));
  }
}

}  // namespace metrics
