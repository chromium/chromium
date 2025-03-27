// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/embedder/scoped_global_scenario_memory.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/shared_memory_mapper.h"
#include "base/memory/structured_shared_memory.h"
#include "base/types/pass_key.h"
#include "components/performance_manager/scenario_api/performance_scenario_memory.h"
#include "components/performance_manager/scenario_api/performance_scenarios.h"
#include "components/performance_manager/scenarios/browser_performance_scenarios.h"
#include "components/performance_manager/scenarios/performance_scenario_data.h"

namespace performance_manager {

ScopedGlobalScenarioMemory::ScopedGlobalScenarioMemory(
    base::SharedMemoryMapper* mapper) {
  auto state_ptr = std::make_unique<PerformanceScenarioData>(mapper);
  if (!state_ptr->HasSharedState()) {
    // Mapping failed. Create an invalid `read_only_mapping_` so that the
    // PerformanceScenarioObserverList exists but does nothing.
    read_only_mapping_.emplace(performance_scenarios::ScenarioScope::kGlobal,
                               base::ReadOnlySharedMemoryRegion());
    return;
  }
  state_ptr->EnsureTracingTracks();
  read_only_mapping_.emplace(
      performance_scenarios::ScenarioScope::kGlobal,
      state_ptr->shared_state().DuplicateReadOnlyRegion());
  SetGlobalSharedScenarioState(base::PassKey<ScopedGlobalScenarioMemory>(),
                               std::move(state_ptr));
  writable_global_memory_installed_ = true;
}

ScopedGlobalScenarioMemory::~ScopedGlobalScenarioMemory() {
  if (writable_global_memory_installed_) {
    SetGlobalSharedScenarioState(base::PassKey<ScopedGlobalScenarioMemory>(),
                                 nullptr);
  }
}

}  // namespace performance_manager
