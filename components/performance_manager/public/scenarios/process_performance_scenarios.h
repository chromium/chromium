// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_SCENARIOS_PROCESS_PERFORMANCE_SCENARIOS_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_SCENARIOS_PROCESS_PERFORMANCE_SCENARIOS_H_

#include "components/performance_manager/scenario_api/performance_scenarios.h"

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

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_SCENARIOS_PROCESS_PERFORMANCE_SCENARIOS_H_
