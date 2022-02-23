// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_SYNTHETIC_TRIALS_H_
#define COMPONENTS_VARIATIONS_SYNTHETIC_TRIALS_H_

#include <stdint.h>

#include <vector>

#include "base/compiler_specific.h"
#include "base/component_export.h"
#include "base/time/time.h"
#include "components/variations/active_field_trials.h"

namespace variations {

// Specifies when UMA reports should start being annotated with a synthetic
// field trial.
enum class SyntheticTrialAnnotationMode {
  // Start annotating UMA reports with this trial only after the next log opens.
  // The UMA report that will be generated from the log that is open at the time
  // of registration will not be annotated with this trial.
  kNextLog,
  // Start annotating UMA reports with this trial immediately, including the one
  // that will be generated from the log that is open at the time of
  // registration.
  kCurrentLog,
};

// A Field Trial and its selected group, which represent a particular
// Chrome configuration state. For example, the trial name could map to
// a preference name, and the group name could map to a preference value.
struct COMPONENT_EXPORT(VARIATIONS) SyntheticTrialGroup {
 public:
  SyntheticTrialGroup(uint32_t trial,
                      uint32_t group,
                      SyntheticTrialAnnotationMode annotation_mode);
  ~SyntheticTrialGroup();

  ActiveGroupId id;
  base::TimeTicks start_time;

  // Determines when UMA reports should start being annotated with this trial
  // group.
  SyntheticTrialAnnotationMode annotation_mode;

  // If this is an external experiment.
  bool is_external = false;
};

// Interface class to observe changes to synthetic trials in MetricsService.
class COMPONENT_EXPORT(VARIATIONS) SyntheticTrialObserver {
 public:
  // Called when the list of synthetic field trial groups has changed.
  virtual void OnSyntheticTrialsChanged(
      const std::vector<SyntheticTrialGroup>& groups) = 0;

 protected:
  virtual ~SyntheticTrialObserver() {}
};

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_SYNTHETIC_TRIALS_H_
