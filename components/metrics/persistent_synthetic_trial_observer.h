// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_PERSISTENT_SYNTHETIC_TRIAL_OBSERVER_H_
#define COMPONENTS_METRICS_PERSISTENT_SYNTHETIC_TRIAL_OBSERVER_H_

#include "base/task/sequenced_task_runner.h"
#include "components/variations/synthetic_trials.h"

namespace metrics {

// Stores synthetic field trials to persistent storage when they are updated.
class PersistentSyntheticTrialObserver
    : public variations::SyntheticTrialObserver {
 public:
  PersistentSyntheticTrialObserver();
  ~PersistentSyntheticTrialObserver() override;

  PersistentSyntheticTrialObserver(const PersistentSyntheticTrialObserver&) =
      delete;
  PersistentSyntheticTrialObserver& operator=(
      const PersistentSyntheticTrialObserver&) = delete;

  // SyntheticTrialObserver impl:
  void OnSyntheticTrialsChanged(
      const std::vector<variations::SyntheticTrialGroup>& trials_updated,
      const std::vector<variations::SyntheticTrialGroup>& trials_removed,
      const std::vector<variations::SyntheticTrialGroup>& groups) override;

 private:
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_PERSISTENT_SYNTHETIC_TRIAL_OBSERVER_H_
