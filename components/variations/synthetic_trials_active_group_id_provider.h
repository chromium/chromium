// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_SYNTHETIC_TRIALS_ACTIVE_GROUP_ID_PROVIDER_H_
#define COMPONENTS_VARIATIONS_SYNTHETIC_TRIALS_ACTIVE_GROUP_ID_PROVIDER_H_

#include <vector>

#include "base/macros.h"
#include "base/synchronization/lock.h"
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
class SyntheticTrialsActiveGroupIdProvider : public SyntheticTrialObserver {
 public:
  static SyntheticTrialsActiveGroupIdProvider* GetInstance();

  // Populates |output| with currently active synthetic trial groups. |output|
  // cannot be nullptr.
  void GetActiveGroupIds(std::vector<ActiveGroupId>* output);

  // Clears state for testing.
  void ResetForTesting();

 private:
  friend struct base::DefaultSingletonTraits<
      SyntheticTrialsActiveGroupIdProvider>;

  SyntheticTrialsActiveGroupIdProvider();
  ~SyntheticTrialsActiveGroupIdProvider() override;

  // metrics::SyntheticTrialObserver:
  void OnSyntheticTrialsChanged(
      const std::vector<SyntheticTrialGroup>& groups) override;

  std::vector<ActiveGroupId> synthetic_trials_;

  base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(SyntheticTrialsActiveGroupIdProvider);
};

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_SYNTHETIC_TRIALS_ACTIVE_GROUP_ID_PROVIDER_H_
