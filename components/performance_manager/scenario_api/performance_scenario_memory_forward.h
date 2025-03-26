// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_SCENARIO_API_PERFORMANCE_SCENARIO_MEMORY_FORWARD_H_
#define COMPONENTS_PERFORMANCE_MANAGER_SCENARIO_API_PERFORMANCE_SCENARIO_MEMORY_FORWARD_H_

#include "base/memory/ref_counted.h"
#include "base/memory/structured_shared_memory.h"

namespace performance_scenarios {

// The full scenario state to copy over shared memory.
struct ScenarioState;

// A scoped object that maps shared memory for the scenario state into the
// current process as long as it exists.
class ScopedReadOnlyScenarioMemory;

// Pointers to the mapped shared memory are held in thread-safe scoped_refptr's.
// The memory will be unmapped when the final reference is dropped. Functions
// that copy values out of the shared memory must hold a reference to it so that
// it's not unmapped while reading.
using RefCountedScenarioMapping = base::RefCountedData<
    base::StructuredSharedMemory<ScenarioState>::ReadOnlyMapping>;

}  // namespace performance_scenarios

#endif  // COMPONENTS_PERFORMANCE_MANAGER_SCENARIO_API_PERFORMANCE_SCENARIO_MEMORY_FORWARD_H_
