// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/scenarios/process_performance_scenarios.h"

#include <atomic>

#include "components/performance_manager/public/graph/process_node.h"
#include "components/performance_manager/scenario_api/performance_scenario_memory.h"
#include "components/performance_manager/scenarios/performance_scenario_data.h"

namespace performance_manager {

namespace {

using performance_scenarios::InputScenario;
using performance_scenarios::LoadingScenario;
using performance_scenarios::ScenarioState;

const ScenarioState& GetScenarioStateForProcess(const ProcessNode* process) {
  static constinit ScenarioState kDummyScenarioState;
  const auto& data = PerformanceScenarioData::GetOrCreate(process);
  return data.HasSharedState() ? data.shared_state().ReadOnlyRef()
                               : kDummyScenarioState;
}

}  // namespace

LoadingScenario GetProcessLoadingScenario(const ProcessNode* process) {
  return GetScenarioStateForProcess(process).loading.load(
      std::memory_order_relaxed);
}

InputScenario GetProcessInputScenario(const ProcessNode* process) {
  return GetScenarioStateForProcess(process).input.load(
      std::memory_order_relaxed);
}

bool CurrentProcessScenariosMatch(
    const ProcessNode* process,
    performance_scenarios::ScenarioPattern pattern) {
  return performance_scenarios::ScenariosMatch(
      GetProcessLoadingScenario(process), GetProcessInputScenario(process),
      pattern);
}

}  // namespace performance_manager
