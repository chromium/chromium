// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_STICKY_ACTIVATION_MANAGER_H_
#define COMPONENTS_VARIATIONS_STICKY_ACTIVATION_MANAGER_H_

#include <map>
#include <string>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/sequence_checker.h"
#include "base/types/pass_key.h"

class PrefService;
class PrefRegistrySimple;

namespace variations {

// Manages the set of field trials marked with the activation type
// STICKY_AFTER_QUERY. Responsible for persisting information about activated
// trials to Local State and using that information to determine if a trial
// should be activated on the next startup.
//
// A sticky trial should be activated on startup if it was active in the
// previous session (including from stickiness on startup) and its group
// selection did not change (i.e. due to a change to its config or something
// external like the client's randomization inputs).
class COMPONENT_EXPORT(VARIATIONS) StickyActivationManager {
 public:
  // Map from trial name to group name. We use std::map since sorted order is
  // useful for deterministic serialization to string.
  //
  // It's possible that base::flat_map would be a better type here, but this
  // does get updated many times and current docs are unclear whether its size
  // (O(100) entries) qualifies as small.
  using TrialNameToGroupNameMap = std::map<std::string, std::string>;

  // `local_state` may be null for tests, in which case no prior stickiness
  // information will be loaded and none will be saved.
  explicit StickyActivationManager(PrefService* local_state);

  StickyActivationManager(const StickyActivationManager&) = delete;
  StickyActivationManager& operator=(const StickyActivationManager&) = delete;

  ~StickyActivationManager();

  // Registers the prefs used by this class.
  static void RegisterPrefs(PrefRegistrySimple& registry);

  // Checks if the specified trial should be activated on startup, based on the
  // persisted information for sticky trials. Should only be called prior to
  // StartMonitoring() and should only be called on sticky activation trials.
  // Internally, this marks the trial as sticky for the purpose of monitoring
  // its activation state.
  bool ShouldActivate(const std::string& trial_name,
                      const std::string& group_name);

  // Starts monitoring field trial activations. May not be called more than
  // once.
  void StartMonitoring();

  // Called when a field trial group is finalized to update the internal state
  // of active sticky trials. This is intended to be called only by the
  // internal Observer class.
  void OnFieldTrialGroupFinalized(base::PassKey<StickyActivationManager>,
                                  const std::string& trial_name,
                                  const std::string& group_name);

 private:
  class Observer;
  friend class Observer;

  // Updates the pref based on `active_sticky_trials_`.
  void UpdatePref();

  SEQUENCE_CHECKER(sequence_checker_);

  raw_ptr<PrefService> local_state_;

  // Whether StartMonitoring() has been called.
  bool monitoring_started_ = false;

  // The field trials loaded at startup from prefs.
  // Note: This is cleared upon calling StartMonitoring(), as it is no longer
  // needed after that.
  TrialNameToGroupNameMap loaded_sticky_trials_;

  // The currently active trials, for persistence to prefs. Updated via calls to
  // ShouldActivate() and observer callbacks to OnFieldTrialGroupFinalized().
  TrialNameToGroupNameMap active_sticky_trials_;

  // The observer is stored as an inner class to hide the implementation details
  // of the observer from the header.
  scoped_refptr<Observer> observer_;

  // Weak pointer factory for this instance. Must be the last member.
  base::WeakPtrFactory<StickyActivationManager> weak_factory_{this};
};

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_STICKY_ACTIVATION_MANAGER_H_
