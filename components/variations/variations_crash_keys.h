// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_VARIATIONS_CRASH_KEYS_H_
#define COMPONENTS_VARIATIONS_VARIATIONS_CRASH_KEYS_H_

#include <string>
#include <vector>

namespace variations {

struct SyntheticTrialGroup;

// The key used in crash reports to indicate the number of active experiments.
// Should match the number of entries in kExperimentListKey.
extern const char kNumExperimentsKey[];

// The key used in crash reports to list all the active experiments. Each
// experiment is listed as two hex numbers: trial ID and group ID, separated by
// a dash. The experiments are separated by a comma.
extern const char kExperimentListKey[];

// Initializes crash keys that report the current set of active FieldTrial
// groups (aka variations) for crash reports. After initialization, an observer
// will be registered on FieldTrialList that will keep the crash keys up-to-date
// with newly-activated trials. Synthetic trials must be manually updated using
// the API below.
void InitCrashKeys();

// Updates variations crash keys by replacing the list of synthetic trials with
// the specified list. Does not affect non-synthetic trials.
void UpdateCrashKeysWithSyntheticTrials(
    const std::vector<SyntheticTrialGroup>& synthetic_trials);

// Clears the internal instance, for testing.
void ClearCrashKeysInstanceForTesting();

// The list of experiments, in the format needed by the crash keys. The
// |num_experiments| goes into the |kNumExperimentsKey| crash key, and the
// |experiment_list| goes into the |kExperimentListKey| crash key.
struct ExperimentListInfo {
  int num_experiments = 0;
  std::string experiment_list;
};

// Gets the variation information that we encode in the crash keys.
// Specifically, returns the string used for representing the active experiment
// groups + the synthetic trials in |experiment_list| and the number of elements
// in that list in |num_experiments|. Must be called on the UI thread.
ExperimentListInfo GetExperimentListInfo();

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_VARIATIONS_CRASH_KEYS_H_
