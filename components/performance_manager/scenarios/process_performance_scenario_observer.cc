// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/scenarios/process_performance_scenario_observer.h"

#include "base/sequence_checker.h"
#include "base/types/pass_key.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/performance_manager/scenario_api/performance_scenario_observer.h"
#include "components/performance_manager/scenarios/performance_scenario_data.h"

namespace performance_manager {

namespace {

using performance_scenarios::InputScenario;
using performance_scenarios::LoadingScenario;
using performance_scenarios::MatchingScenarioObserver;
using performance_scenarios::PerformanceScenarioObserver;
using performance_scenarios::ScenarioScope;

}  // namespace

// static
ProcessPerformanceScenarioObserverList&
ProcessPerformanceScenarioObserverList::GetForProcess(
    const ProcessNode* process) {
  return PerformanceScenarioData::GetOrCreate(process).process_observer_list();
}

ProcessPerformanceScenarioObserverList::ProcessPerformanceScenarioObserverList(
    base::PassKey<PerformanceScenarioData>) {}

ProcessPerformanceScenarioObserverList::
    ~ProcessPerformanceScenarioObserverList() = default;

void ProcessPerformanceScenarioObserverList::AddObserver(
    PerformanceScenarioObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(observer);
}

void ProcessPerformanceScenarioObserverList::RemoveObserver(
    PerformanceScenarioObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(observer);
}

void ProcessPerformanceScenarioObserverList::AddMatchingObserver(
    MatchingScenarioObserver* matching_observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  matching_observers_.AddObserver(matching_observer);
}

void ProcessPerformanceScenarioObserverList::RemoveMatchingObserver(
    MatchingScenarioObserver* matching_observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  matching_observers_.RemoveObserver(matching_observer);
}

void ProcessPerformanceScenarioObserverList::NotifyScenariosChanged(
    LoadingScenario old_loading_scenario,
    LoadingScenario new_loading_scenario,
    InputScenario old_input_scenario,
    InputScenario new_input_scenario) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (new_loading_scenario != old_loading_scenario) {
    observers_.Notify(&PerformanceScenarioObserver::OnLoadingScenarioChanged,
                      ScenarioScope::kCurrentProcess, old_loading_scenario,
                      new_loading_scenario);
  }
  if (new_input_scenario != old_input_scenario) {
    observers_.Notify(&PerformanceScenarioObserver::OnInputScenarioChanged,
                      ScenarioScope::kCurrentProcess, old_input_scenario,
                      new_input_scenario);
  }
  matching_observers_.Notify(
      &MatchingScenarioObserver::NotifyIfScenarioMatchChanged,
      ScenarioScope::kCurrentProcess, new_loading_scenario, new_input_scenario);
}

}  // namespace performance_manager
