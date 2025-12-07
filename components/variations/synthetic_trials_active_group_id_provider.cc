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

std::vector<ActiveGroupId>
SyntheticTrialsActiveGroupIdProvider::GetActiveGroupIds() {
  base::AutoLock scoped_lock(lock_);
  return group_ids_;
}

#if !defined(NDEBUG)
std::vector<SyntheticTrialGroup>
SyntheticTrialsActiveGroupIdProvider::GetGroups() {
  base::AutoLock scoped_lock(lock_);
  return groups_;
}
#endif  // !defined(NDEBUG)

void SyntheticTrialsActiveGroupIdProvider::ResetForTesting() {
  base::AutoLock scoped_lock(lock_);
  group_ids_.clear();
#if !defined(NDEBUG)
  groups_.clear();
#endif  // !defined(NDEBUG)
}

void SyntheticTrialsActiveGroupIdProvider::OnSyntheticTrialsChanged(
    const std::vector<SyntheticTrialGroup>& trials_updated,
    const std::vector<SyntheticTrialGroup>& trials_removed,
    const std::vector<SyntheticTrialGroup>& groups) {
  {
    base::AutoLock scoped_lock(lock_);
    group_ids_.clear();
    for (const auto& group : groups) {
      group_ids_.push_back(group.id());
    }
#if !defined(NDEBUG)
    groups_ = groups;
#endif  // !defined(NDEBUG)
  }

  // Update the experiments list for crash reports.
  UpdateCrashKeysWithSyntheticTrials(groups);
}

}  // namespace variations
