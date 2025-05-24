// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_SCENARIOS_BROWSER_PERFORMANCE_SCENARIOS_H_
#define COMPONENTS_PERFORMANCE_MANAGER_SCENARIOS_BROWSER_PERFORMANCE_SCENARIOS_H_

#include <memory>

#include "base/memory/read_only_shared_memory_region.h"
#include "base/types/pass_key.h"
#include "components/performance_manager/scenario_api/performance_scenarios.h"

namespace content {
class RenderProcessHost;
}  // namespace content

namespace performance_manager {

class PerformanceScenarioData;
class ProcessNode;
class ScopedGlobalScenarioMemory;

// Convenience aliases.
using LoadingScenario = performance_scenarios::LoadingScenario;
using InputScenario = performance_scenarios::InputScenario;

// Functions to manage the browser side of ScenarioState memory regions that
// communicate state to child processes. The state can be read from any process
// using functions in
// //components/performance_manager/scenario_api/performance_scenarios.h.
//
// All functions must be called from the UI thread.

// Lets ScopedGlobalScenarioMemory set the global scenario state, or clear it if
// `state` is nullptr.
void SetGlobalSharedScenarioState(
    base::PassKey<ScopedGlobalScenarioMemory>,
    std::unique_ptr<PerformanceScenarioData> state);

// Returns a copy of the shared memory handle for the scenario state of
// `process_node`, or an invalid handle if there is none. The handle can be
// passed to the child process to map in the scenario state.
base::ReadOnlySharedMemoryRegion GetSharedScenarioRegionForProcessNode(
    const ProcessNode* process_node);

// Returns a copy of the shared memory handle for the global scenario state, or
// an invalid handle if there is none. The handle can be passed to child
// processes to map in the scenario state.
base::ReadOnlySharedMemoryRegion GetGlobalSharedScenarioRegion();

// Functions to set the current performance scenarios. This can only be done
// from the browser process.

// Atomically updates the loading scenario for a renderer process.
void SetLoadingScenarioForProcess(LoadingScenario scenario,
                                  content::RenderProcessHost* host);
void SetLoadingScenarioForProcessNode(LoadingScenario scenario,
                                      const ProcessNode* process_node);

// Atomically updates the global loading scenario.
void SetGlobalLoadingScenario(LoadingScenario scenario);

// Atomically updates the input scenario for a renderer process.
void SetInputScenarioForProcess(InputScenario scenario,
                                content::RenderProcessHost* host);
void SetInputScenarioForProcessNode(InputScenario scenario,
                                    const ProcessNode* process_node);

// Atomically updates the global input scenario.
void SetGlobalInputScenario(InputScenario scenario);

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_SCENARIOS_BROWSER_PERFORMANCE_SCENARIOS_H_
