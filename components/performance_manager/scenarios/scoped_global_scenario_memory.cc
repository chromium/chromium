// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/embedder/scoped_global_scenario_memory.h"

#include <optional>
#include <utility>

#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/structured_shared_memory.h"
#include "base/types/pass_key.h"
#include "components/performance_manager/scenario_api/performance_scenarios.h"
#include "components/performance_manager/scenarios/browser_performance_scenarios.h"
#include "components/performance_manager/scenarios/performance_scenario_data.h"

namespace performance_manager {

ScopedGlobalScenarioMemory::ScopedGlobalScenarioMemory() {
  auto state_ptr = RefCountedScenarioState::Create();
  if (state_ptr) {
    state_ptr->EnsureTracingTracks();
    read_only_mapping_.emplace(
        performance_scenarios::ScenarioScope::kGlobal,
        state_ptr->shared_state().DuplicateReadOnlyRegion());
    SetGlobalSharedScenarioState(base::PassKey<ScopedGlobalScenarioMemory>(),
                                 std::move(state_ptr));
  }
}

ScopedGlobalScenarioMemory::~ScopedGlobalScenarioMemory() {
  SetGlobalSharedScenarioState(base::PassKey<ScopedGlobalScenarioMemory>(),
                               nullptr);
}

}  // namespace performance_manager
