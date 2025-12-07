// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_USAGE_TRACKER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_USAGE_TRACKER_H_

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/sequence_checker.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom-shared.h"

class PrefService;

namespace optimization_guide {

// Tracks usage of on-device model eligible features.
class UsageTracker {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called when on-device eligible `feature` was used.
    // TODO(holte): Update ComponentStateManager metrics and remove this.
    virtual void OnDeviceEligibleFeatureUsed(mojom::OnDeviceFeature feature) {}

    // Called when on-device eligible `feature` was used and that usage will
    // change the "was recently used" state from false to true.
    virtual void OnDeviceEligibleFeatureFirstUsed(
        mojom::OnDeviceFeature feature) {}
  };

  explicit UsageTracker(PrefService* local_state);
  ~UsageTracker();

  UsageTracker(const UsageTracker&) = delete;
  UsageTracker& operator=(const UsageTracker&) = delete;

  // Notifies the usage tracker that the `feature` was (attempted to be) used.
  void OnDeviceEligibleFeatureUsed(mojom::OnDeviceFeature feature);

  // Returns true if `feature` was recently used and is an on-device eligible
  // feature.
  bool WasOnDeviceEligibleFeatureRecentlyUsed(
      mojom::OnDeviceFeature feature) const;

  // Returns whether any on-device eligible feature was recently used.
  bool WasAnyOnDeviceEligibleFeatureRecentlyUsed() const;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  raw_ptr<PrefService> local_state_ GUARDED_BY_CONTEXT(sequence_checker_);
  base::ObserverList<Observer> observers_ GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_USAGE_TRACKER_H_
