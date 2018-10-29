// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_SYNTHETIC_TRIAL_REGISTRY_H_
#define COMPONENTS_VARIATIONS_SYNTHETIC_TRIAL_REGISTRY_H_

#include <vector>

#include "base/observer_list.h"
#include "components/variations/synthetic_trials.h"

namespace metrics {
class MetricsServiceAccessor;
}  // namespace metrics

namespace variations {

struct ActiveGroupId;
class FieldTrialsProvider;
class FieldTrialsProviderTest;

class SyntheticTrialRegistry {
 public:
  SyntheticTrialRegistry();
  ~SyntheticTrialRegistry();

  // Adds an observer to be notified when the synthetic trials list changes.
  void AddSyntheticTrialObserver(SyntheticTrialObserver* observer);

  // Removes an existing observer of synthetic trials list changes.
  void RemoveSyntheticTrialObserver(SyntheticTrialObserver* observer);

 private:
  friend metrics::MetricsServiceAccessor;
  friend FieldTrialsProvider;
  friend FieldTrialsProviderTest;
  FRIEND_TEST_ALL_PREFIXES(SyntheticTrialRegistryTest, RegisterSyntheticTrial);
  FRIEND_TEST_ALL_PREFIXES(SyntheticTrialRegistryTest,
                           RegisterSyntheticMultiGroupFieldTrial);
  FRIEND_TEST_ALL_PREFIXES(SyntheticTrialRegistryTest,
                           GetSyntheticFieldTrialActiveGroups);
  FRIEND_TEST_ALL_PREFIXES(VariationsCrashKeysTest, BasicFunctionality);

  // Registers a field trial name and group to be used to annotate a UMA report
  // with a particular Chrome configuration state. A UMA report will be
  // annotated with this trial group if and only if all events in the report
  // were created after the trial is registered. Only one group name may be
  // registered at a time for a given trial_name. Only the last group name that
  // is registered for a given trial name will be recorded. The values passed
  // in must not correspond to any real field trial in the code.
  // Note: Should not be used to replace trials that were registered with
  // RegisterSyntheticMultiGroupFieldTrial().
  void RegisterSyntheticFieldTrial(const SyntheticTrialGroup& trial_group);

  // Similar to RegisterSyntheticFieldTrial(), but registers a synthetic trial
  // that has multiple active groups for a given trial name hash. Any previous
  // groups registered for |trial_name_hash| will be replaced.
  void RegisterSyntheticMultiGroupFieldTrial(
      uint32_t trial_name_hash,
      const std::vector<uint32_t>& group_name_hashes);

  // Returns a list of synthetic field trials that are older than |time|.
  void GetSyntheticFieldTrialsOlderThan(
      base::TimeTicks time,
      std::vector<ActiveGroupId>* synthetic_trials);

  // Notifies observers on a synthetic trial list change.
  void NotifySyntheticTrialObservers();

  // Field trial groups that map to Chrome configuration states.
  std::vector<SyntheticTrialGroup> synthetic_trial_groups_;

  // List of observers of |synthetic_trial_groups_| changes.
  base::ObserverList<SyntheticTrialObserver>::Unchecked
      synthetic_trial_observer_list_;
};

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_SYNTHETIC_TRIAL_REGISTRY_H_
