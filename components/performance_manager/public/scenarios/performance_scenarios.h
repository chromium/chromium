// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_SCENARIOS_PERFORMANCE_SCENARIOS_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_SCENARIOS_PERFORMANCE_SCENARIOS_H_

#include <optional>

#include "base/memory/read_only_shared_memory_region.h"
#include "third_party/blink/public/common/performance/performance_scenarios.h"

namespace content {
class RenderProcessHost;
}  // namespace content

namespace performance_manager {

// Convenience aliases.
using LoadingScenario = blink::performance_scenarios::LoadingScenario;
using InputScenario = blink::performance_scenarios::InputScenario;

// Functions to manage the browser side of SharedScenarioState memory regions
// that communicate state to child processes. The state can be read from any
// process using functions in
// //third_party/blink/public/common/performance/performance_scenarios.h.

// A scoped object that maps in writable shared memory for the global scenario
// state as long as it exists.
class ScopedGlobalScenarioMemory {
 public:
  ScopedGlobalScenarioMemory();
  ~ScopedGlobalScenarioMemory();

  ScopedGlobalScenarioMemory(const ScopedGlobalScenarioMemory&) = delete;
  ScopedGlobalScenarioMemory& operator=(const ScopedGlobalScenarioMemory&) =
      delete;

 private:
  // The browser process also maps in the global scenario memory read-only. The
  // //third_party/blink/public/common/performance/performance_scenarios.h query
  // functions will read from this for Scope::kGlobal.
  std::optional<blink::performance_scenarios::ScopedReadOnlyScenarioMemory>
      read_only_mapping_;
};

// Returns a copy of the shared memory handle for the scenario state of a
// renderer process, or an invalid handle if there is none. The handle can be
// passed to child processes to map in the scenario state.
base::ReadOnlySharedMemoryRegion GetSharedScenarioRegionForProcess(
    content::RenderProcessHost& host);

// Returns a copy of the shared memory handle for the global scenario state, or
// an invalid handle if there is none. The handle can be passed to child
// processes to map in the scenario state.
base::ReadOnlySharedMemoryRegion GetGlobalSharedScenarioRegion();

// Functions to set the current performance scenarios. This can only be done
// from the browser process.

// Atomically updates the loading scenario for a renderer process.
void SetLoadingScenarioForProcess(content::RenderProcessHost& host,
                                  LoadingScenario scenario);

// Atomically updates the global loading scenario.
void SetGlobalLoadingScenario(LoadingScenario scenario);

// Atomically updates the input scenario for a renderer process.
void SetInputScenarioForProcess(content::RenderProcessHost& host,
                                InputScenario scenario);

// Atomically updates the global input scenario.
void SetGlobalInputScenario(InputScenario scenario);

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_SCENARIOS_PERFORMANCE_SCENARIOS_H_
