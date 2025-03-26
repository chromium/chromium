// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/scenario_api/performance_scenarios.h"

#include <atomic>
#include <utility>

#include "base/containers/enum_set.h"
#include "base/memory/scoped_refptr.h"
#include "components/performance_manager/scenario_api/performance_scenario_memory.h"

namespace performance_scenarios {

namespace {

// Returns the scenario state from `mapping`, or a default empty state if
// `mapping` is null (which can happen if no ScopedReadOnlyScenarioMemory exists
// or if the mapping failed). Takes a raw pointer instead of a scoped_ptr to
// avoid refcount churn.
const ScenarioState& GetScenarioStateFromMapping(
    const RefCountedScenarioMapping* mapping) {
  static constinit ScenarioState kDummyScenarioState;
  return mapping ? mapping->data.ReadOnlyRef() : kDummyScenarioState;
}

}  // namespace

SharedAtomicRef<LoadingScenario> GetLoadingScenario(ScenarioScope scope) {
  scoped_refptr<RefCountedScenarioMapping> mapping =
      GetScenarioMappingForScope(scope);
  const std::atomic<LoadingScenario>& scenario =
      GetScenarioStateFromMapping(mapping.get()).loading;
  return SharedAtomicRef<LoadingScenario>(std::move(mapping), scenario);
}

SharedAtomicRef<InputScenario> GetInputScenario(ScenarioScope scope) {
  scoped_refptr<RefCountedScenarioMapping> mapping =
      GetScenarioMappingForScope(scope);
  const std::atomic<InputScenario>& scenario =
      GetScenarioStateFromMapping(mapping.get()).input;
  return SharedAtomicRef<InputScenario>(std::move(mapping), scenario);
}

bool CurrentScenariosMatch(ScenarioScope scope, ScenarioPattern pattern) {
  return ScenariosMatch(
      GetLoadingScenario(scope)->load(std::memory_order_relaxed),
      GetInputScenario(scope)->load(std::memory_order_relaxed), pattern);
}

bool ScenariosMatch(LoadingScenario loading_scenario,
                    InputScenario input_scenario,
                    ScenarioPattern pattern) {
  bool loading_matches =
      pattern.loading.empty() || pattern.loading.Has(loading_scenario);
  bool input_matches =
      pattern.input.empty() || pattern.input.Has(input_scenario);
  return loading_matches && input_matches;
}

}  // namespace performance_scenarios
