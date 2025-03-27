// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/scenario_api/performance_scenario_memory.h"

#include <optional>
#include <utility>

#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/structured_shared_memory.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "components/performance_manager/scenario_api/performance_scenario_observer.h"
#include "components/performance_manager/scenario_api/performance_scenarios.h"

namespace performance_scenarios {

namespace {

// Global pointers to the shared memory mappings.
scoped_refptr<RefCountedScenarioMapping>& MappingPtrForScope(
    ScenarioScope scope) {
  static base::NoDestructor<scoped_refptr<RefCountedScenarioMapping>>
      current_process_mapping;
  static base::NoDestructor<scoped_refptr<RefCountedScenarioMapping>>
      global_mapping;
  switch (scope) {
    case ScenarioScope::kCurrentProcess:
      return *current_process_mapping;
    case ScenarioScope::kGlobal:
      return *global_mapping;
  }
  NOTREACHED();
}

}  // namespace

// TODO(crbug.com/365586676): Currently these are only mapped into browser and
// renderer processes. The global scenarios should also be mapped into utility
// processes.

ScopedReadOnlyScenarioMemory::ScopedReadOnlyScenarioMemory(
    ScenarioScope scope,
    base::ReadOnlySharedMemoryRegion region)
    : scope_(scope) {
  using SharedScenarioState = base::StructuredSharedMemory<ScenarioState>;
  std::optional<SharedScenarioState::ReadOnlyMapping> mapping =
      SharedScenarioState::MapReadOnlyRegion(std::move(region));
  if (mapping.has_value()) {
    MappingPtrForScope(scope_) =
        base::MakeRefCounted<RefCountedScenarioMapping>(
            std::move(mapping.value()));
  }

  // The ObserverList must be created after mapping the memory, because it reads
  // the scenario state in its constructor.
  PerformanceScenarioObserverList::CreateForScope(PassKey(), scope_);
}

ScopedReadOnlyScenarioMemory::~ScopedReadOnlyScenarioMemory() {
  PerformanceScenarioObserverList::DestroyForScope(PassKey(), scope_);
  MappingPtrForScope(scope_).reset();
}

scoped_refptr<RefCountedScenarioMapping> GetScenarioMappingForScope(
    ScenarioScope scope) {
  return MappingPtrForScope(scope);
}

}  // namespace performance_scenarios
