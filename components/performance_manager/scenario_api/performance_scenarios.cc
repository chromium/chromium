// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/scenario_api/performance_scenarios.h"

#include <optional>
#include <utility>

#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/structured_shared_memory.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/types/pass_key.h"
#include "components/performance_manager/scenario_api/performance_scenario_observer.h"

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
  PerformanceScenarioObserverList::CreateForScope(
      base::PassKey<ScopedReadOnlyScenarioMemory>(), scope_);
}

ScopedReadOnlyScenarioMemory::~ScopedReadOnlyScenarioMemory() {
  PerformanceScenarioObserverList::DestroyForScope(
      base::PassKey<ScopedReadOnlyScenarioMemory>(), scope_);
  MappingPtrForScope(scope_).reset();
}

// static
scoped_refptr<RefCountedScenarioMapping>
ScopedReadOnlyScenarioMemory::GetMappingForTesting(ScenarioScope scope) {
  return MappingPtrForScope(scope);
}

SharedAtomicRef<LoadingScenario> GetLoadingScenario(ScenarioScope scope) {
  scoped_refptr<RefCountedScenarioMapping> mapping = MappingPtrForScope(scope);
  return SharedAtomicRef<LoadingScenario>(
      mapping, GetScenarioStateFromMapping(mapping.get()).loading);
}

SharedAtomicRef<InputScenario> GetInputScenario(ScenarioScope scope) {
  scoped_refptr<RefCountedScenarioMapping> mapping = MappingPtrForScope(scope);
  return SharedAtomicRef<InputScenario>(
      mapping, GetScenarioStateFromMapping(mapping.get()).input);
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
