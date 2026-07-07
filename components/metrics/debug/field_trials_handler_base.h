// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_DEBUG_FIELD_TRIALS_HANDLER_BASE_H_
#define COMPONENTS_METRICS_DEBUG_FIELD_TRIALS_HANDLER_BASE_H_

#include <string_view>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"

class PrefService;

namespace variations {
struct StudyGroupNames;
class VariationsService;
}  // namespace variations

namespace metrics {

// Platform-agnostic logic for the Field Trials tab of
// chrome://metrics-internals.
class FieldTrialsHandlerBase {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;

    virtual void ResolvePageCallback(const base::ValueView callback_id,
                                     const base::ValueView response) = 0;
  };

  FieldTrialsHandlerBase(Delegate* delegate,
                         variations::VariationsService* variations_service,
                         PrefService* local_state);

  FieldTrialsHandlerBase(const FieldTrialsHandlerBase&) = delete;
  FieldTrialsHandlerBase& operator=(const FieldTrialsHandlerBase&) = delete;

  ~FieldTrialsHandlerBase();

  // fetchTrialState() grabs the state of studies and calls populateState() with
  // the result.
  void HandleFetchState(const base::Value& callback_id, bool show_names);

  // setTrialEnrollState(callback, trial, group, enabled) overrides the enroll
  // state of a field trial which will be realized after a restart.
  void HandleSetEnrollState(const base::Value& callback_id,
                            std::string_view trial_hash,
                            std::string_view group_hash,
                            bool enabled);

  // lookupTrialOrGroupName(name) is called when the user types in a a study or
  // experiment name. If the name matches a known study or experiment, this
  // provides the page a mapping from hash to name for presentation.
  void HandleLookupTrialOrGroupName(const base::Value& callback_id,
                                    std::string_view name);

 private:
  struct ExperimentOverride {
    std::string trial_hash;
    std::string group_hash;
  };

  // Returns the state of all field trials. Returns a `FieldTrialState` from
  // components/metrics/debug/browser_proxy.ts.
  base::DictValue GetFieldTrialStateValue();

  // One-time initialization for this class.
  void InitializeFieldTrials(
      base::OnceCallback<void(base::ValueView)> done_callback,
      bool show_names);

  // Refreshes the field trial overrides with the given `studies`.
  void RefreshFieldTrialOverrides(
      base::OnceCallback<void(base::ValueView)> done_callback,
      std::vector<variations::StudyGroupNames> studies);

  // Turns on or off an experiment override, which will be realized after a
  // restart.
  bool SetOverride(const ExperimentOverride& override, bool enabled);

  void ResolveJsCallbackHelper(base::Value callback_id, base::ValueView result);

  const raw_ptr<Delegate> delegate_;
  const raw_ptr<variations::VariationsService> variations_service_;
  const raw_ptr<PrefService> local_state_;

  bool show_names_ = false;
  bool restart_required_ = false;

  // The studies available to force. This is only populated after the first
  // call to `InitializeFieldTrials()`.
  std::optional<std::vector<variations::StudyGroupNames>> studies_;

  base::flat_map<std::string, std::string> overrides_;

  base::WeakPtrFactory<FieldTrialsHandlerBase> weak_ptr_factory_{this};
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_DEBUG_FIELD_TRIALS_HANDLER_BASE_H_
