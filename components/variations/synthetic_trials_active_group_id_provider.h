// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_SYNTHETIC_TRIALS_ACTIVE_GROUP_ID_PROVIDER_H_
#define COMPONENTS_VARIATIONS_SYNTHETIC_TRIALS_ACTIVE_GROUP_ID_PROVIDER_H_

#include <vector>

#include "base/component_export.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "components/variations/active_field_trials.h"
#include "components/variations/synthetic_trials.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

namespace variations {

// This is a helper class which can observe the creation of SyntheticTrialGroups
// and later provide a list of active group IDs to be included in the crash
// reports. This class is a thread-safe singleton.
class COMPONENT_EXPORT(VARIATIONS) SyntheticTrialsActiveGroupIdProvider
    : public SyntheticTrialObserver {
 public:
  static SyntheticTrialsActiveGroupIdProvider* GetInstance();

  SyntheticTrialsActiveGroupIdProvider(
      const SyntheticTrialsActiveGroupIdProvider&) = delete;
  SyntheticTrialsActiveGroupIdProvider& operator=(
      const SyntheticTrialsActiveGroupIdProvider&) = delete;

  // Returns currently active synthetic trial group IDs.
  std::vector<ActiveGroupId> GetActiveGroupIds();

#if !defined(NDEBUG)
  // In debug mode, not only the group IDs are tracked but also the full group
  // info, to display the names unhashed in chrome://version.
  std::vector<SyntheticTrialGroup> GetGroups();
#endif  // !defined(NDEBUG)

  // Clears state for testing.
  void ResetForTesting();

 private:
  friend struct base::DefaultSingletonTraits<
      SyntheticTrialsActiveGroupIdProvider>;

  SyntheticTrialsActiveGroupIdProvider();
  ~SyntheticTrialsActiveGroupIdProvider() override;

  // metrics::SyntheticTrialObserver:
  void OnSyntheticTrialsChanged(
      const std::vector<SyntheticTrialGroup>& trials_updated,
      const std::vector<SyntheticTrialGroup>& trials_removed,
      const std::vector<SyntheticTrialGroup>& groups) override;

  base::Lock lock_;
  std::vector<ActiveGroupId> group_ids_;  // GUARDED_BY(lock_);
#if !defined(NDEBUG)
  // In debug builds, keep the full group information to be able to display it
  // in chrome://version.
  std::vector<SyntheticTrialGroup> groups_;  // GUARDED_BY(lock_);
#endif                                       // !defined(NDEBUG)
};

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_SYNTHETIC_TRIALS_ACTIVE_GROUP_ID_PROVIDER_H_
