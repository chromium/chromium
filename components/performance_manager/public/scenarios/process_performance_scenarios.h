// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_SCENARIOS_PROCESS_PERFORMANCE_SCENARIOS_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_SCENARIOS_PROCESS_PERFORMANCE_SCENARIOS_H_

#include "base/observer_list.h"
#include "base/scoped_observation_traits.h"
#include "components/performance_manager/scenario_api/performance_scenarios.h"

namespace performance_scenarios {
class MatchingScenarioObserver;
class PerformanceScenarioObserver;
}  // namespace performance_scenarios

namespace performance_manager {

class ProcessNode;

// Functions to let the browser process query the performance scenarios for a
// child process. These are similar to functions in
// components/performance_manager/scenario_api/performance_scenarios.h, but have
// extra performance_manager dependencies.
//
// These functions all take a ProcessNode, and read the scenario values that are
// visible for ScenarioScope::kCurrentProcess in that process. Scenario values
// that are visible for ScenarioScope::kGlobal are the same in all processes, so
// can be read in the browser process using the API in performance_scenarios.h.
//
// All functions must be called on the UI thread. They return scenario values
// directly instead of `scenario_api::SharedAtomicRef` because the browser
// process updates scenario memory on the UI thread, so it won't change
// unexpectedly.

// Returns the current LoadingScenario for `process`.
performance_scenarios::LoadingScenario GetProcessLoadingScenario(
    const ProcessNode* process);

// Returns the current InputScenario for `process`.
performance_scenarios::InputScenario GetProcessInputScenario(
    const ProcessNode* process);

// Returns true if the current scenarios for `process` match `pattern`.
bool CurrentProcessScenariosMatch(
    const ProcessNode* process,
    performance_scenarios::ScenarioPattern pattern);

// Returns a list of PerformanceScenarioObservers for `process` that will be
// notified when the scenarios for that process change. The list is only valid
// as long as the ProcessNode exists.
base::ObserverList<performance_scenarios::PerformanceScenarioObserver>&
GetScenarioObserversForProcess(const ProcessNode* process);

// Returns a list of MatchingScenarioObservers for `process` that will be
// notified when the scenarios for that process change to start or stop
// matching a scenario pattern. The list is only valid as long as the
// ProcessNode exists.
base::ObserverList<performance_scenarios::MatchingScenarioObserver>&
GetMatchingScenarioObserversForProcess(const ProcessNode* process);

}  // namespace performance_manager

namespace base {

// Specialize ScopedObservation to look up the observer lists for a ProcessNode.
// These must be in the same namespace as base::ScopedObservationTraits.

template <>
struct ScopedObservationTraits<
    performance_manager::ProcessNode,
    performance_scenarios::PerformanceScenarioObserver> {
  static void AddObserver(
      const performance_manager::ProcessNode* source,
      performance_scenarios::PerformanceScenarioObserver* observer) {
    GetScenarioObserversForProcess(source).AddObserver(observer);
  }
  static void RemoveObserver(
      const performance_manager::ProcessNode* source,
      performance_scenarios::PerformanceScenarioObserver* observer) {
    GetScenarioObserversForProcess(source).RemoveObserver(observer);
  }
};

template <>
struct ScopedObservationTraits<
    performance_manager::ProcessNode,
    performance_scenarios::MatchingScenarioObserver> {
  static void AddObserver(
      const performance_manager::ProcessNode* source,
      performance_scenarios::MatchingScenarioObserver* observer) {
    GetMatchingScenarioObserversForProcess(source).AddObserver(observer);
  }
  static void RemoveObserver(
      const performance_manager::ProcessNode* source,
      performance_scenarios::MatchingScenarioObserver* observer) {
    GetMatchingScenarioObserversForProcess(source).RemoveObserver(observer);
  }
};

}  // namespace base

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_SCENARIOS_PROCESS_PERFORMANCE_SCENARIOS_H_
