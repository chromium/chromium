// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_SCENARIOS_PERFORMANCE_SCENARIO_OBSERVER_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_SCENARIOS_PERFORMANCE_SCENARIO_OBSERVER_H_

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/sequence_checker.h"
#include "components/performance_manager/public/graph/graph_registered.h"
#include "third_party/blink/public/common/performance/performance_scenarios.h"

namespace performance_manager {

class ProcessNode;

// Convenience aliases.
using LoadingScenario = blink::performance_scenarios::LoadingScenario;
using InputScenario = blink::performance_scenarios::InputScenario;

class PerformanceScenarioObserver : public base::CheckedObserver {
 public:
  // Invoked whenever the given scenario changes for all pages.
  virtual void OnGlobalLoadingScenarioChanged(LoadingScenario old_scenario,
                                              LoadingScenario new_scenario) {}
  virtual void OnGlobalInputScenarioChanged(InputScenario old_scenario,
                                            InputScenario new_scenario) {}

  // Invoked whenever the given scenario changes for pages hosted partially in
  // `process_node`.
  virtual void OnProcessLoadingScenarioChanged(const ProcessNode* process_node,
                                               LoadingScenario old_scenario,
                                               LoadingScenario new_scenario) {}
  virtual void OnProcessInputScenarioChanged(const ProcessNode* process_node,
                                             InputScenario old_scenario,
                                             InputScenario new_scenario) {}
};

class PerformanceScenarioNotifier final
    : public GraphOwnedAndRegistered<PerformanceScenarioNotifier> {
 public:
  PerformanceScenarioNotifier();
  ~PerformanceScenarioNotifier() final;

  PerformanceScenarioNotifier(const PerformanceScenarioNotifier&) = delete;
  PerformanceScenarioNotifier& operator=(const PerformanceScenarioNotifier&) =
      delete;

  void AddGlobalObserver(PerformanceScenarioObserver* observer);
  void RemoveGlobalObserver(const PerformanceScenarioObserver* observer);

  void AddObserverForProcess(const ProcessNode* process_node,
                             PerformanceScenarioObserver* observer);
  void RemoveObserverForProcess(const ProcessNode* process_node,
                                const PerformanceScenarioObserver* observer);

 private:
  // Allow PerformanceScenarioNotifierAccessor to read the observer lists.
  friend class PerformanceScenarioNotifierAccessor;

  using ObserverList = base::ObserverList<PerformanceScenarioObserver>;

  ObserverList* GetGlobalObservers();
  ObserverList* GetProcessObservers(const ProcessNode* process_node);

  SEQUENCE_CHECKER(sequence_checker_);

  ObserverList global_observers_ GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_SCENARIOS_PERFORMANCE_SCENARIO_OBSERVER_H_
