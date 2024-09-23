// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_SYNTHETIC_TRIAL_REGISTRY_H_
#define COMPONENTS_VARIATIONS_SYNTHETIC_TRIAL_REGISTRY_H_

#include <vector>
#include <string_view>

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/gtest_prod_util.h"
#include "base/metrics/field_trial_params.h"
#include "base/observer_list.h"
#include "components/variations/synthetic_trials.h"

class SingleClientNigoriSyncTest;

namespace metrics {
class MetricsServiceAccessor;
}  // namespace metrics

namespace content {
class SyntheticTrialSyncer;
}  // namespace content

namespace tpcd::experiment {
class ExperimentManagerImplBrowserTest;
}  // namespace tpcd::experiment

namespace variations {

struct ActiveGroupId;
class FieldTrialsProvider;
class FieldTrialsProviderTest;
class LimitedEntropySyntheticTrial;
class SyntheticTrialRegistryTest;
class LimitedEntropyRandomizationBrowserTest;

namespace internal {
COMPONENT_EXPORT(VARIATIONS) BASE_DECLARE_FEATURE(kExternalExperimentAllowlist);
}  // namespace internal

class COMPONENT_EXPORT(VARIATIONS) SyntheticTrialRegistry {
 public:
  SyntheticTrialRegistry();
  ~SyntheticTrialRegistry();

  // Adds an observer to be notified when the synthetic trials list changes.
  void AddObserver(SyntheticTrialObserver* observer);

  // Removes an existing observer of synthetic trials list changes.
  void RemoveObserver(SyntheticTrialObserver* observer);

  // Specifies the mode of RegisterExternalExperiments() operation.
  enum OverrideMode {
    // Previously-registered external experiment ids are overridden (replaced)
    // with the new list.
    kOverrideExistingIds,
    // Previously-registered external experiment ids are not overridden, but
    // new experiment ids may be added.
    kDoNotOverrideExistingIds,
  };

  // Registers a list of experiment ids coming from an external application.
  // The input ids are in the VariationID format.
  //
  // The supplied ids must have corresponding entries in the
  // "ExternalExperimentAllowlist" (coming via a feature param) to be applied.
  // The allowlist also supplies the corresponding trial name that should be
  // used for reporting to UMA.
  //
  // If |mode| is kOverrideExistingIds, this API clears previously-registered
  // external experiment ids, replacing them with the new list (which may be
  // empty). If |mode| is kDoNotOverrideExistingIds, any new ids that are not
  // already registered will be added, but existing ones will not be replaced.
  void RegisterExternalExperiments(const std::vector<int>& experiment_ids,
                                   OverrideMode mode);

 private:
  friend metrics::MetricsServiceAccessor;
  friend LimitedEntropySyntheticTrial;
  friend FieldTrialsProvider;
  friend FieldTrialsProviderTest;
  friend ::SingleClientNigoriSyncTest;
  friend SyntheticTrialRegistryTest;
  friend ::tpcd::experiment::ExperimentManagerImplBrowserTest;
  friend content::SyntheticTrialSyncer;
  friend LimitedEntropyRandomizationBrowserTest;
  FRIEND_TEST_ALL_PREFIXES(SyntheticTrialRegistryTest, RegisterSyntheticTrial);
  FRIEND_TEST_ALL_PREFIXES(SyntheticTrialRegistryTest,
                           GetSyntheticFieldTrialsOlderThanSuffix);
  FRIEND_TEST_ALL_PREFIXES(SyntheticTrialRegistryTest,
                           GetSyntheticFieldTrialActiveGroups);
  FRIEND_TEST_ALL_PREFIXES(SyntheticTrialRegistryTest, NotifyObserver);
  FRIEND_TEST_ALL_PREFIXES(VariationsCrashKeysTest, BasicFunctionality);
  FRIEND_TEST_ALL_PREFIXES(LimitedEntropyRandomizationBrowserTest,
                           MANUAL_SyntheticTrialAndStudyRegistrationSubTest);

  // Registers a field trial name and group to be used to annotate UMA and UKM
  // reports with a particular Chrome configuration state.
  //
  // If the |trial_group|'s |annotation_mode| is set to |kNextLog|, then reports
  // will be annotated with this trial group if and only if all events in the
  // report were created after the trial's registration. If the
  // |annotation_mode| is set to |kCurrentLog|, then reports will be annotated
  // with this trial group even if there are events in the report that were
  // created before this trial's registration.
  //
  // Only one group name may be registered at a time for a given trial name.
  // Only the last group name that is registered for a given trial name will be
  // recorded. The values passed in must not correspond to any real field trial
  // in the code.
  //
  // Synthetic trials are not automatically re-registered after a restart.
  //
  // Note: Should not be used to replace trials that were registered with
  // RegisterExternalExperiments().
  void RegisterSyntheticFieldTrial(const SyntheticTrialGroup& trial_group);

  // Returns the study name corresponding to |experiment_id| from the allowlist
  // contained in |params|. An empty string view is returned when the
  // experiment is not in the allowlist.
  std::string_view GetStudyNameForExpId(const base::FieldTrialParams& params,
                                        const std::string& experiment_id);

  // Returns a list of synthetic field trials that are either (1) older than
  // |time|, or (2) specify |kCurrentLog| as |annotation_mode|. The trial and
  // group names are suffixed with |suffix| before being hashed.
  void GetSyntheticFieldTrialsOlderThan(
      base::TimeTicks time,
      std::vector<ActiveGroupId>* synthetic_trials,
      std::string_view suffix = "") const;

  // SyntheticTrialSyncer needs to know all current synthetic trial
  // groups after launching new child processes.
  const std::vector<SyntheticTrialGroup>& GetSyntheticTrialGroups() const {
    return synthetic_trial_groups_;
  }

  // Notifies observers on a synthetic trial list change.
  void NotifySyntheticTrialObservers(
      const std::vector<SyntheticTrialGroup>& trials_updated,
      const std::vector<SyntheticTrialGroup>& trials_removed);

  // Field trial groups that map to Chrome configuration states.
  std::vector<SyntheticTrialGroup> synthetic_trial_groups_;

  // List of observers of |synthetic_trial_groups_| changes.
  base::ObserverList<SyntheticTrialObserver>::Unchecked
      synthetic_trial_observer_list_;
};

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_SYNTHETIC_TRIAL_REGISTRY_H_
