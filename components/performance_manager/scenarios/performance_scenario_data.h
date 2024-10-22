// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_SCENARIOS_PERFORMANCE_SCENARIO_DATA_H_
#define COMPONENTS_PERFORMANCE_MANAGER_SCENARIOS_PERFORMANCE_SCENARIO_DATA_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "components/performance_manager/graph/node_inline_data.h"
#include "third_party/blink/public/common/performance/performance_scenarios.h"

namespace performance_manager {

// Pointers to the mapped shared memory are held in thread-safe scoped_refptr's.
// The memory will be unmapped when the final reference is dropped. Functions
// that write to the shared memory must hold a reference to it so that it's not
// unmapped while writing.
using RefCountedScenarioMemory =
    base::RefCountedData<blink::performance_scenarios::SharedScenarioState>;

// Holds the browser's scenario state handle for a child's scenario state.
class PerformanceScenarioMemoryData final
    : public NodeInlineData<PerformanceScenarioMemoryData> {
 public:
  PerformanceScenarioMemoryData();
  ~PerformanceScenarioMemoryData();

  // Move-only.
  PerformanceScenarioMemoryData(const PerformanceScenarioMemoryData&) = delete;
  PerformanceScenarioMemoryData& operator=(
      const PerformanceScenarioMemoryData&) = delete;
  PerformanceScenarioMemoryData(PerformanceScenarioMemoryData&&);
  PerformanceScenarioMemoryData& operator=(PerformanceScenarioMemoryData&&);

  scoped_refptr<RefCountedScenarioMemory> shared_mem;
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_SCENARIOS_PERFORMANCE_SCENARIO_DATA_H_
