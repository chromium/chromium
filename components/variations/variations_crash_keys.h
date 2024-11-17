// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_VARIATIONS_CRASH_KEYS_H_
#define COMPONENTS_VARIATIONS_VARIATIONS_CRASH_KEYS_H_

#include <string>
#include <vector>

#include "base/component_export.h"

namespace variations {

class SyntheticTrialGroup;
struct ActiveGroupId;

// The key used in crash reports to indicate the number of active experiments.
// Should match the number of entries in kExperimentListKey.
COMPONENT_EXPORT(VARIATIONS) extern const char kNumExperimentsKey[];

// The key used in crash reports to list all the active experiments. Each
// experiment is listed as two hex numbers: trial ID and group ID, separated by
// a dash. The experiments are separated by a comma.
COMPONENT_EXPORT(VARIATIONS) extern const char kExperimentListKey[];

COMPONENT_EXPORT(VARIATIONS) extern const char kVariationsSeedVersionKey[];

// Initializes crash keys that report the current set of active FieldTrial
// groups (aka variations) for crash reports. After initialization, an observer
// will be registered on FieldTrialList that will keep the crash keys up-to-date
// with newly-activated trials. Synthetic trials must be manually updated using
// the API below.
COMPONENT_EXPORT(VARIATIONS) void InitCrashKeys();

// Updates variations crash keys by replacing the list of synthetic trials with
// the specified list. Does not affect non-synthetic trials.
COMPONENT_EXPORT(VARIATIONS)
void UpdateCrashKeysWithSyntheticTrials(
    const std::vector<SyntheticTrialGroup>& synthetic_trials);

// Sets the crash key for the variations seed version.
COMPONENT_EXPORT(VARIATIONS)
void SetVariationsSeedVersionCrashKey(const std::string& seed_version);

// Clears the internal instance, for testing.
COMPONENT_EXPORT(VARIATIONS) void ClearCrashKeysInstanceForTesting();

// The list of experiments, in the format needed by the crash keys. The
// |num_experiments| goes into the |kNumExperimentsKey| crash key, and the
// |experiment_list| goes into the |kExperimentListKey| crash key.
struct COMPONENT_EXPORT(VARIATIONS) ExperimentListInfo {
  int num_experiments = 0;
  std::string experiment_list;
};

// Gets the variation information that we encode in the crash keys.
// Specifically, returns the string used for representing the active experiment
// groups + the synthetic trials in |experiment_list| and the number of elements
// in that list in |num_experiments|. Must be called on the UI thread.
COMPONENT_EXPORT(VARIATIONS) ExperimentListInfo GetExperimentListInfo();

// Gets the hash code of the experiment.
COMPONENT_EXPORT(VARIATIONS)
std::string ActiveGroupToString(const ActiveGroupId& active_group);

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_VARIATIONS_CRASH_KEYS_H_
