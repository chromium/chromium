// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_USAGE_TRACKER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_USAGE_TRACKER_H_

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/sequence_checker.h"
#include "components/optimization_guide/core/model_execution/on_device_features.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom-shared.h"

class PrefService;

namespace optimization_guide {

// Tracks usage of on-device model eligible features.
class UsageTracker {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called when on-device eligible `use_case_name` was used.
    // `is_first_usage` is true if this use case is being used for the first
    // time and changes the "was recently used" from false to true.
    virtual void OnDeviceEligibleUseCaseUsed(const std::string& use_case_name,
                                             bool is_first_usage) {}
  };
  explicit UsageTracker(PrefService* local_state);
  ~UsageTracker();

  UsageTracker(const UsageTracker&) = delete;
  UsageTracker& operator=(const UsageTracker&) = delete;

  // Notifies the usage tracker that the `feature` was (attempted to be) used.
  void OnDeviceEligibleFeatureUsed(mojom::OnDeviceFeature feature);

  // Notifies the usage tracker that the `use_case_name` was (attempted to be)
  // used.
  void OnDeviceEligibleUseCaseUsed(const std::string& use_case_name);

  // Returns true if `feature` was recently used and is an on-device eligible
  // feature.
  bool WasOnDeviceEligibleFeatureRecentlyUsed(
      mojom::OnDeviceFeature feature) const;

  // Returns true if `use_case_name` was recently used.
  bool WasUseCaseRecentlyUsed(const std::string& use_case_name) const;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Set / Clear use case requested state.
  // Clear is supported for debug through chrome://on-device-internals.
  void SetUseCaseRequested(const std::string& use_case_name, bool requested);

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  raw_ptr<PrefService> local_state_ GUARDED_BY_CONTEXT(sequence_checker_);
  base::ObserverList<Observer> observers_ GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_USAGE_TRACKER_H_
