// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_SCENARIO_API_PERFORMANCE_SCENARIO_MEMORY_H_
#define COMPONENTS_PERFORMANCE_MANAGER_SCENARIO_API_PERFORMANCE_SCENARIO_MEMORY_H_

#include "base/component_export.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/shared_memory_safety_checker.h"
#include "base/types/pass_key.h"
#include "components/performance_manager/scenario_api/performance_scenario_memory_forward.h"  // IWYU pragma: export
#include "components/performance_manager/scenario_api/performance_scenarios.h"

namespace performance_scenarios {

// The full scenario state to copy over shared memory.
#pragma clang diagnostic push
#pragma clang diagnostic error "-Wpadded"
struct COMPONENT_EXPORT(SCENARIO_API) ScenarioState {
  base::subtle::SharedAtomic<LoadingScenario> loading;
  base::subtle::SharedAtomic<InputScenario> input;
};
#pragma clang diagnostic pop

// A scoped object that maps shared memory for the scenario state into the
// current process as long as it exists.
class COMPONENT_EXPORT(SCENARIO_API) ScopedReadOnlyScenarioMemory {
 public:
  // Maps `region` into the current process, as a read-only view of the memory
  // holding the scenario state for `scope`.
  ScopedReadOnlyScenarioMemory(ScenarioScope scope,
                               base::ReadOnlySharedMemoryRegion region);
  ~ScopedReadOnlyScenarioMemory();

  ScopedReadOnlyScenarioMemory(const ScopedReadOnlyScenarioMemory&) = delete;
  ScopedReadOnlyScenarioMemory& operator=(const ScopedReadOnlyScenarioMemory&) =
      delete;

  static void OnScenarioObserverListCreated(
      base::PassKey<ScopedScenarioObserverList>,
      ScenarioScope scope);

 private:
  using PassKey = base::PassKey<ScopedReadOnlyScenarioMemory>;
  ScenarioScope scope_;
};

// A scoped object that owns the PerformanceScenarioObserverList to notify when
// the scenario state in shared memory changes. This should be created near the
// start of any process where PerformanceScenarioObserverList is supported so
// callers can register observers before ScopedReadOnlyScenarioMemory is mapped.
//
// As long as this exists PerformanceScenarioObserverList::GetForScope() will
// return an object. Otherwise GetForScope() will return nullptr.
class COMPONENT_EXPORT(SCENARIO_API) ScopedScenarioObserverList {
 public:
  ScopedScenarioObserverList();
  ~ScopedScenarioObserverList();

  ScopedScenarioObserverList(const ScopedScenarioObserverList&) = delete;
  ScopedScenarioObserverList& operator=(const ScopedScenarioObserverList&) =
      delete;

 private:
  using PassKey = base::PassKey<ScopedScenarioObserverList>;
};

// Returns a pointer to the shared memory mapping registered for `scope`, or
// nullptr if there isn't any.
COMPONENT_EXPORT(SCENARIO_API)
scoped_refptr<RefCountedScenarioMapping> GetScenarioMappingForScope(
    ScenarioScope scope);

}  // namespace performance_scenarios

#endif  // COMPONENTS_PERFORMANCE_MANAGER_SCENARIO_API_PERFORMANCE_SCENARIO_MEMORY_H_
