// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/synthetic_trials_active_group_id_provider.h"

#include "base/memory/singleton.h"
#include "components/variations/variations_associated_data.h"
#include "components/variations/variations_crash_keys.h"

namespace variations {

SyntheticTrialsActiveGroupIdProvider*
SyntheticTrialsActiveGroupIdProvider::GetInstance() {
  return base::Singleton<SyntheticTrialsActiveGroupIdProvider>::get();
}

SyntheticTrialsActiveGroupIdProvider::SyntheticTrialsActiveGroupIdProvider() =
    default;

SyntheticTrialsActiveGroupIdProvider::~SyntheticTrialsActiveGroupIdProvider() =
    default;

void SyntheticTrialsActiveGroupIdProvider::GetActiveGroupIds(
    std::vector<ActiveGroupId>* output) {
  base::AutoLock scoped_lock(lock_);
  for (const auto& group_id : synthetic_trials_)
    output->push_back(group_id);
}

void SyntheticTrialsActiveGroupIdProvider::ResetForTesting() {
  base::AutoLock scoped_lock(lock_);
  synthetic_trials_.clear();
}

void SyntheticTrialsActiveGroupIdProvider::OnSyntheticTrialsChanged(
    const std::vector<SyntheticTrialGroup>& trials_updated,
    const std::vector<SyntheticTrialGroup>& trials_removed,
    const std::vector<SyntheticTrialGroup>& groups) {
  {
    base::AutoLock scoped_lock(lock_);
    synthetic_trials_.clear();
    for (const auto& group : groups)
      synthetic_trials_.push_back(group.id());
  }

  // Update the experiments list for crash reports.
  UpdateCrashKeysWithSyntheticTrials(groups);
}

}  // namespace variations
