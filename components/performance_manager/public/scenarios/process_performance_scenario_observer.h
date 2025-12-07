// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_SCENARIOS_PROCESS_PERFORMANCE_SCENARIO_OBSERVER_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_SCENARIOS_PROCESS_PERFORMANCE_SCENARIO_OBSERVER_H_

#include "base/observer_list.h"
#include "base/scoped_observation_traits.h"
#include "base/sequence_checker.h"
#include "base/types/pass_key.h"
#include "components/performance_manager/scenario_api/performance_scenario_observer.h"
#include "components/performance_manager/scenario_api/performance_scenarios.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace performance_manager {

class PerformanceScenarioData;
class ProcessNode;

// Allows the browser process to register observers for the performance
// scenarios of a child process. This is similar to
// PerformanceScenarioObserverList from
// components/performance_manager/scenario_api/performance_scenario_observer.h,
// but has extra performance_manager dependencies.
//
// Observers are registered for a ProcessNode, and monitor the scenario values
// that are visible for ScenarioScope::kCurrentProcess in that process. Scenario
// values that are visible for ScenarioScope::kGlobal are the same in all
// processes, so can be monitored in the browser process using the API in
// performance_scenario_observer.h.
//
// All methods must be called on the UI thread, and the observers will be
// notified there.
//
// GetForProcess() can be used with base::ScopedObservation, which takes a
// pointer to a source object, as:
//
//   base::ScopedObservation<ProcessPerformanceScenarioObserverList,
//                           performance_scenarios::PerformanceScenarioObserver>
//       observation{&observer};
//   observation.Observe(
//       &ProcessPerformanceScenarioObserverList::GetForProcess(process_node));
//
// Or to have ScopedObservation call Add/RemoveMatchingObserver:
//
//   base::ScopedObservation<ProcessPerformanceScenarioObserverList,
//                           performance_scenarios::MatchingScenarioObserver>
//       observation{&observer};
//   observation.Observe(
//       &ProcessPerformanceScenarioObserverList::GetForProcess(process_node));
class ProcessPerformanceScenarioObserverList {
 public:
  // Returns the object that notifies observers for `process_node`.
  static ProcessPerformanceScenarioObserverList& GetForProcess(
      const ProcessNode* process);

  // Only PerformanceScenarioData can create the
  // ProcessPerformanceScenarioObserverList for the process.
  explicit ProcessPerformanceScenarioObserverList(
      base::PassKey<PerformanceScenarioData>);

  ~ProcessPerformanceScenarioObserverList();

  // Adds `observer` to the list.
  void AddObserver(
      performance_scenarios::PerformanceScenarioObserver* observer);

  // Removes `observer` from the list.
  void RemoveObserver(
      performance_scenarios::PerformanceScenarioObserver* observer);

  // Adds `matching_observer` to the list.
  void AddMatchingObserver(
      performance_scenarios::MatchingScenarioObserver* matching_observer);

  // Removes `matching_observer` from the list.
  void RemoveMatchingObserver(
      performance_scenarios::MatchingScenarioObserver* matching_observer);

  // Notifies observers that the kCurrentProcess scenarios for this ProcessNode
  // changed.
  void NotifyScenariosChanged(
      performance_scenarios::LoadingScenario old_loading_scenario,
      performance_scenarios::LoadingScenario new_loading_scenario,
      performance_scenarios::InputScenario old_input_scenario,
      performance_scenarios::InputScenario new_input_scenario);

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  base::ObserverList<performance_scenarios::PerformanceScenarioObserver>
      observers_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Lists of MatchingScenarioObservers that share the same ScenarioPattern,
  // along with the last `matches_pattern` value that was sent to
  // MatchingScenarioObserver::OnScenarioMatchChanged.
  struct MatchingScenarioObservers {
    bool last_matches_pattern = false;

    std::unique_ptr<
        base::ObserverList<performance_scenarios::MatchingScenarioObserver>>
        observer_list = std::make_unique<base::ObserverList<
            performance_scenarios::MatchingScenarioObserver>>();

    MatchingScenarioObservers();
    ~MatchingScenarioObservers();

    // Move-only.
    MatchingScenarioObservers(const MatchingScenarioObservers&) = delete;
    MatchingScenarioObservers& operator=(const MatchingScenarioObservers&) =
        delete;
    MatchingScenarioObservers(MatchingScenarioObservers&&);
    MatchingScenarioObservers& operator=(MatchingScenarioObservers&&);
  };

  absl::flat_hash_map<performance_scenarios::ScenarioPattern,
                      MatchingScenarioObservers>
      matching_observers_by_pattern_ GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace performance_manager

namespace base {

// Specialize ScopedObservation to invoke the correct add and remove methods for
// MatchingScenarioObserver. These must be in the same namespace as
// base::ScopedObservationTraits.

template <>
struct ScopedObservationTraits<
    performance_manager::ProcessPerformanceScenarioObserverList,
    performance_scenarios::MatchingScenarioObserver> {
  static void AddObserver(
      performance_manager::ProcessPerformanceScenarioObserverList* source,
      performance_scenarios::MatchingScenarioObserver* observer) {
    source->AddMatchingObserver(observer);
  }
  static void RemoveObserver(
      performance_manager::ProcessPerformanceScenarioObserverList* source,
      performance_scenarios::MatchingScenarioObserver* observer) {
    source->RemoveMatchingObserver(observer);
  }
};

}  // namespace base

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_SCENARIOS_PROCESS_PERFORMANCE_SCENARIO_OBSERVER_H_
