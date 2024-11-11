// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_SCENARIOS_PERFORMANCE_SCENARIO_DATA_H_
#define COMPONENTS_PERFORMANCE_MANAGER_SCENARIOS_PERFORMANCE_SCENARIO_DATA_H_

#include <memory>
#include <optional>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/structured_shared_memory.h"
#include "base/observer_list.h"
#include "base/types/optional_util.h"
#include "components/performance_manager/graph/node_inline_data.h"
#include "components/performance_manager/public/scenarios/performance_scenario_observer.h"
#include "third_party/blink/public/common/performance/performance_scenarios.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"

namespace performance_manager {

class ProcessNode;

// Pointers to the mapped shared memory are held in thread-safe scoped_refptr's.
// The memory will be unmapped when the final reference is dropped. Functions
// that write to the shared memory must hold a reference to it so that it's not
// unmapped while writing.
class RefCountedScenarioState
    : public base::RefCountedThreadSafe<RefCountedScenarioState> {
 public:
  using ScenarioState = blink::performance_scenarios::ScenarioState;

  // Creates a new ScenarioState memory region and returns a ref-counted pointer
  // to it, or nullptr if mapping fails.
  static scoped_refptr<RefCountedScenarioState> Create();

  base::StructuredSharedMemory<ScenarioState>& shared_state() {
    return shared_state_;
  }
  const base::StructuredSharedMemory<ScenarioState>& shared_state() const {
    return shared_state_;
  }

  // Returns tracing tracks to log trace events when updating scenarios in the
  // shared memory region, or nullptr if RegisterTracingTracks() wasn't called.
  const perfetto::NamedTrack* loading_tracing_track() const {
    return base::OptionalToPtr(loading_tracing_track_);
  }
  const perfetto::NamedTrack* input_tracing_track() const {
    return base::OptionalToPtr(input_tracing_track_);
  }

  // Creates tracing tracks under the ProcessTrack for `process_node`. The
  // tracks will log trace events when updating scenarios in the shared memory
  // region. If `process_node` is null, tracks for the global scenario state
  // will be created under the current ProcessTrack.
  void EnsureTracingTracks(const ProcessNode* process_node = nullptr);

 private:
  friend class base::RefCountedThreadSafe<RefCountedScenarioState>;

  explicit RefCountedScenarioState(
      base::StructuredSharedMemory<ScenarioState> shared_state);
  ~RefCountedScenarioState();

  // Shared scenario memory region.
  base::StructuredSharedMemory<ScenarioState> shared_state_;

  // Additional data associated with the region.
  std::optional<perfetto::NamedTrack> loading_tracing_track_;
  std::optional<perfetto::NamedTrack> input_tracing_track_;
};

// Holds the browser's scenario state handle for a child's scenario state.
class PerformanceScenarioMemoryData final
    : public NodeInlineData<PerformanceScenarioMemoryData> {
 public:
  static PerformanceScenarioMemoryData& GetOrCreate(
      const ProcessNode* process_node);

  PerformanceScenarioMemoryData();
  ~PerformanceScenarioMemoryData();

  // Move-only.
  PerformanceScenarioMemoryData(const PerformanceScenarioMemoryData&) = delete;
  PerformanceScenarioMemoryData& operator=(
      const PerformanceScenarioMemoryData&) = delete;
  PerformanceScenarioMemoryData(PerformanceScenarioMemoryData&&);
  PerformanceScenarioMemoryData& operator=(PerformanceScenarioMemoryData&&);

  // The shared scenario memory region for the process.
  scoped_refptr<RefCountedScenarioState> state_ptr =
      RefCountedScenarioState::Create();

  // Observers to notify when a scenario in the shared memory changes.
  std::unique_ptr<base::ObserverList<PerformanceScenarioObserver>> observers;
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_SCENARIOS_PERFORMANCE_SCENARIO_DATA_H_
