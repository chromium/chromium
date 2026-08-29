// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_USAGE_TRACKER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_USAGE_TRACKER_H_

#include <optional>
#include <string>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "components/optimization_guide/core/model_execution/on_device_features.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom-shared.h"

class PrefService;

namespace optimization_guide {

// Tracks usage of on-device model eligible features.
class UsageTracker {
 public:
  enum class Priority {
    kBestEffort = 0,
    kUserBlocking = 1,
  };

  class Observer : public base::CheckedObserver {
   public:
    // Called when priority for `use_case_name` increases.
    // `previous_priority` is the priority before the increase, or
    // std::nullopt if the use case was not recently used.
    virtual void OnPriorityIncrease(
        const std::string& use_case_name,
        std::optional<Priority> previous_priority) {}
  };
  explicit UsageTracker(PrefService* local_state);
  ~UsageTracker();

  UsageTracker(const UsageTracker&) = delete;
  UsageTracker& operator=(const UsageTracker&) = delete;

  // Notifies the usage tracker that the `use_case_name` was used with
  // `priority`, raising the tracked priority if it was previously lower.
  void RaisePriority(const std::string& use_case_name, Priority priority);

  // Returns the priority at which `use_case_name` was used, or std::nullopt if
  // it was not recently used.
  std::optional<Priority> GetPriority(const std::string& use_case_name) const;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Set / Clear use case requested priority.
  // Setting std::nullopt clears the use case priority.
  // Supported for debug through chrome://on-device-internals.
  void SetPriority(const std::string& use_case_name,
                   std::optional<Priority> priority);

  // Clears all use case usages both in-memory and in prefs.
  void ClearAllUseCaseUsages();

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  raw_ptr<PrefService> local_state_ GUARDED_BY_CONTEXT(sequence_checker_);
  base::flat_set<std::string> user_blocking_use_cases_
      GUARDED_BY_CONTEXT(sequence_checker_);
  base::ObserverList<Observer> observers_ GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_USAGE_TRACKER_H_
