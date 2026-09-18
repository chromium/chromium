// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_USAGE_TRACKER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_USAGE_TRACKER_H_

#include <string>

#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "components/optimization_guide/core/model_execution/on_device_features.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom-shared.h"

class PrefService;

namespace optimization_guide {

BASE_DECLARE_FEATURE(kOnDeviceModelUsageTracking);

// Controls whether use cases not used within `kRetentionPeriod` are marked
// `Priority::kEvictable`, or if all use cases are `Priority::kRetain` at
// minimum.
BASE_DECLARE_FEATURE(kOnDeviceModelEviction);

// The amount of grace period from the last time a use case was used to
// consider it as recently used. Recent usage is one of the criteria for the
// base and adaptation on-device models to be downloaded.
extern const base::FeatureParam<base::TimeDelta> kRecentUsePeriod;

// The amount of time since last usage that an on-device model is retained
// before it is eligible for eviction.
extern const base::FeatureParam<base::TimeDelta> kRetentionPeriod;

// Tracks usage of on-device model eligible features.
class UsageTracker {
 public:
  enum class Priority {
    // Use case has not been used within `kRetentionPeriod`.
    kEvictable = 0,
    // Use case was used within `kRetentionPeriod`, but not within
    // `kRecentUsePeriod`. Cannot be passed to RaisePriority() or SetPriority();
    // use cases transition to this priority only when `kRecentUsePeriod`
    // elapses since their last usage, or as the minimum priority when
    // `kOnDeviceModelEviction` is disabled.
    kRetain = 1,
    kBestEffort = 2,
    kUserBlocking = 3,
  };

  class Observer : public base::CheckedObserver {
   public:
    // Called when priority for `use_case_name` increases.
    // `previous_priority` is the priority before the increase.
    virtual void OnPriorityIncrease(const std::string& use_case_name,
                                    Priority previous_priority) {}
  };
  explicit UsageTracker(PrefService* local_state);
  ~UsageTracker();

  UsageTracker(const UsageTracker&) = delete;
  UsageTracker& operator=(const UsageTracker&) = delete;

  // Notifies the usage tracker that the `use_case_name` was used with
  // `priority`, raising the tracked priority if it was previously lower.
  void RaisePriority(const std::string& use_case_name, Priority priority);

  // Returns the priority at which `use_case_name` was used.
  Priority GetPriority(const std::string& use_case_name) const;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Set / Clear use case requested priority.
  // Setting Priority::kEvictable clears the use case priority.
  // Supported for debug through chrome://on-device-internals.
  void SetPriority(const std::string& use_case_name, Priority priority);

  // Clears all use case usages both in-memory and in prefs.
  void ClearAllUseCaseUsages();

 private:
  void PruneOldUsagePrefs();
  void RecordUseCaseUsage(const std::string& use_case_name);
  void ClearUseCaseUsage(const std::string& use_case_name);
  bool WasUseCaseUsedWithin(const std::string& use_case_name,
                            base::TimeDelta period) const;

  SEQUENCE_CHECKER(sequence_checker_);

  raw_ptr<PrefService> local_state_ GUARDED_BY_CONTEXT(sequence_checker_);
  base::flat_set<std::string> user_blocking_use_cases_
      GUARDED_BY_CONTEXT(sequence_checker_);
  base::ObserverList<Observer> observers_ GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_USAGE_TRACKER_H_
