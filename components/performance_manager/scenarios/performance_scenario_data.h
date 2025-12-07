// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_SCENARIOS_PERFORMANCE_SCENARIO_DATA_H_
#define COMPONENTS_PERFORMANCE_MANAGER_SCENARIOS_PERFORMANCE_SCENARIO_DATA_H_

#include <memory>
#include <optional>

#include "base/memory/structured_shared_memory.h"
#include "base/types/pass_key.h"
#include "components/performance_manager/graph/node_inline_data.h"
#include "components/performance_manager/public/scenarios/process_performance_scenario_observer.h"
#include "components/performance_manager/scenario_api/performance_scenario_memory_forward.h"
#include "components/performance_manager/scenario_api/performance_scenario_observer.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"

namespace base {
class SharedMemoryMapper;
}

namespace performance_manager {

class ProcessNode;

// Holds the browser's scenario state handle for a child's scenario state and
// associated data. An instance will be stored with each ProcessNode using
// NodeInlineData, and another instance will be stored in
// ScopedGlobalScenarioMemory for the global state.
class PerformanceScenarioData final
    : public NodeInlineData<PerformanceScenarioData> {
 public:
  using SharedScenarioState =
      base::StructuredSharedMemory<performance_scenarios::ScenarioState>;

  // Returns the data for `process_node`, creating it if it doesn't already
  // exist. If the data is newly created and `mapper` is non-null, it will be
  // used to map the shared scenario memory region. This is useful for testing.
  static PerformanceScenarioData& GetOrCreate(
      const ProcessNode* process_node,
      base::SharedMemoryMapper* mapper = nullptr);

  // If `mapper` is non-null, it will be used to map the shared scenario memory
  // region. This is useful for testing.
  explicit PerformanceScenarioData(base::SharedMemoryMapper* mapper = nullptr);

  ~PerformanceScenarioData();

  // Move-only.
  PerformanceScenarioData(const PerformanceScenarioData&) = delete;
  PerformanceScenarioData& operator=(const PerformanceScenarioData&) = delete;
  PerformanceScenarioData(PerformanceScenarioData&&);
  PerformanceScenarioData& operator=(PerformanceScenarioData&&);

  // Returns true if the shared scenario memory region was mapped successfully.
  bool HasSharedState() const { return shared_state_.has_value(); }

  // Returns the shared scenario memory region. Asserts that HasSharedState() is
  // true.
  SharedScenarioState& shared_state() { return shared_state_.value(); }
  const SharedScenarioState& shared_state() const {
    return shared_state_.value();
  }

  // Returns an accessor for observers of changes to the scenario state of the
  // ProcessNode.
  ProcessPerformanceScenarioObserverList& process_observer_list() {
    return *process_observer_list_;
  }

  // Returns tracing tracks to log trace events when updating scenarios in the
  // shared memory region, or nullptr if EnsureTracingTracks() wasn't called.
  const perfetto::NamedTrack* loading_tracing_track() const;
  const perfetto::NamedTrack* input_tracing_track() const;

  // Creates tracing tracks under the ProcessTrack for `process_node`. The
  // tracks will log trace events when updating scenarios in the shared memory
  // region. If `process_node` is null, creates tracks for the global scenario
  // state.
  void EnsureTracingTracks(const ProcessNode* process_node = nullptr);

 private:
  struct TracingTracks {
    std::optional<perfetto::NamedTrack> loading_track;
    std::optional<perfetto::NamedTrack> input_track;
  };

  // Shared scenario memory region.
  std::optional<SharedScenarioState> shared_state_;

  // Additional data associated with the region.

  // Tracing tracks must be stored in a heap-allocated struct because NamedTrack
  // is non-copyable and non-movable.
  std::unique_ptr<TracingTracks> tracing_tracks_ =
      std::make_unique<TracingTracks>();

  // Observers for changes to the scenario state of a specific ProcessNode. Held
  // in unique_ptr because it contains ObserverLists that aren't movable.
  std::unique_ptr<ProcessPerformanceScenarioObserverList>
      process_observer_list_ =
          std::make_unique<ProcessPerformanceScenarioObserverList>(
              base::PassKey<PerformanceScenarioData>());
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_SCENARIOS_PERFORMANCE_SCENARIO_DATA_H_
