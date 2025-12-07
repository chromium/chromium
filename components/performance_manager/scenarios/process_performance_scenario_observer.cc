// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/scenarios/process_performance_scenario_observer.h"

#include <utility>

#include "base/check.h"
#include "base/sequence_checker.h"
#include "base/types/pass_key.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/performance_manager/scenario_api/performance_scenario_observer.h"
#include "components/performance_manager/scenarios/performance_scenario_data.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

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
  auto [it, inserted] = matching_observers_by_pattern_.try_emplace(
      matching_observer->scenario_pattern());
  if (inserted) {
    // Initialize the new map entry.
    it->second.last_matches_pattern = CurrentScenariosMatch(
        ScenarioScope::kCurrentProcess, matching_observer->scenario_pattern());
  }
  it->second.observer_list->AddObserver(matching_observer);
}

void ProcessPerformanceScenarioObserverList::RemoveMatchingObserver(
    MatchingScenarioObserver* matching_observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = matching_observers_by_pattern_.find(
      matching_observer->scenario_pattern());
  CHECK(it != matching_observers_by_pattern_.end());
  it->second.observer_list->RemoveObserver(matching_observer);
  if (it->second.observer_list->empty()) {
    matching_observers_by_pattern_.erase(it);
  }
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
  for (auto& [pattern, matching_observers] : matching_observers_by_pattern_) {
    bool matches_pattern =
        ScenariosMatch(new_loading_scenario, new_input_scenario, pattern);
    bool last_matches_pattern =
        std::exchange(matching_observers.last_matches_pattern, matches_pattern);
    if (last_matches_pattern != matches_pattern) {
      matching_observers.observer_list->Notify(
          &MatchingScenarioObserver::OnScenarioMatchChanged,
          ScenarioScope::kCurrentProcess, matches_pattern);
    }
  }
}

ProcessPerformanceScenarioObserverList::MatchingScenarioObservers::
    MatchingScenarioObservers() = default;

ProcessPerformanceScenarioObserverList::MatchingScenarioObservers::
    ~MatchingScenarioObservers() = default;

ProcessPerformanceScenarioObserverList::MatchingScenarioObservers::
    MatchingScenarioObservers(MatchingScenarioObservers&&) = default;

ProcessPerformanceScenarioObserverList::MatchingScenarioObservers&
ProcessPerformanceScenarioObserverList::MatchingScenarioObservers::operator=(
    MatchingScenarioObservers&&) = default;

}  // namespace performance_manager
