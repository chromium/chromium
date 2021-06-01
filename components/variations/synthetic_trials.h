// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_SYNTHETIC_TRIALS_H_
#define COMPONENTS_VARIATIONS_SYNTHETIC_TRIALS_H_

#include <stdint.h>

#include <vector>

#include "base/compiler_specific.h"
#include "base/time/time.h"
#include "components/variations/active_field_trials.h"

namespace variations {

// A Field Trial and its selected group, which represent a particular
// Chrome configuration state. For example, the trial name could map to
// a preference name, and the group name could map to a preference value.
struct SyntheticTrialGroup {
 public:
  SyntheticTrialGroup(uint32_t trial, uint32_t group);
  ~SyntheticTrialGroup();

  ActiveGroupId id;
  base::TimeTicks start_time;

  // If this is an external experiment.
  bool is_external = false;
};

// Interface class to observe changes to synthetic trials in MetricsService.
class SyntheticTrialObserver {
 public:
  // Called when the list of synthetic field trial groups has changed.
  virtual void OnSyntheticTrialsChanged(
      const std::vector<SyntheticTrialGroup>& groups) = 0;

 protected:
  virtual ~SyntheticTrialObserver() {}
};

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_SYNTHETIC_TRIALS_H_
