// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/scenarios/performance_scenario_data.h"

#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/memory/structured_shared_memory.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/public/tracing_support.h"
#include "third_party/blink/public/common/performance/performance_scenarios.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"

namespace performance_manager {

namespace {

perfetto::NamedTrack CreateTracingTrack(const ProcessNode* process_node,
                                        perfetto::StaticString name,
                                        uint64_t track_id) {
  if (process_node) {
    return CreateProcessTracingTrack(process_node, name, track_id);
  } else {
    return perfetto::NamedTrack(name, track_id,
                                perfetto::ProcessTrack::Current());
  }
}

}  // namespace

// static
scoped_refptr<RefCountedScenarioState> RefCountedScenarioState::Create() {
  auto shared_state = base::StructuredSharedMemory<ScenarioState>::Create();
  if (shared_state.has_value()) {
    return base::WrapRefCounted(
        new RefCountedScenarioState(std::move(shared_state.value())));
  }
  return nullptr;
}

RefCountedScenarioState::RefCountedScenarioState(
    base::StructuredSharedMemory<ScenarioState> shared_state)
    : shared_state_(std::move(shared_state)) {}

RefCountedScenarioState::~RefCountedScenarioState() = default;

void RefCountedScenarioState::EnsureTracingTracks(
    const ProcessNode* process_node) {
  uint64_t track_id = reinterpret_cast<uint64_t>(this);
  if (process_node && !HasProcessTracingTrack(process_node)) {
    return;
  }
  if (!loading_tracing_track_.has_value()) {
    loading_tracing_track_.emplace(CreateTracingTrack(
        process_node, "LoadingPerformanceScenario", track_id));
  }
  if (!input_tracing_track_.has_value()) {
    input_tracing_track_.emplace(
        CreateTracingTrack(process_node, "InputPerformanceScenario", track_id));
  }
}

// static
PerformanceScenarioMemoryData& PerformanceScenarioMemoryData::GetOrCreate(
    const ProcessNode* process_node) {
  auto* process_node_impl = ProcessNodeImpl::FromNode(process_node);
  if (Exists(process_node_impl)) {
    return Get(process_node_impl);
  }
  return Create(process_node_impl);
}

PerformanceScenarioMemoryData::PerformanceScenarioMemoryData() = default;

PerformanceScenarioMemoryData::~PerformanceScenarioMemoryData() = default;

PerformanceScenarioMemoryData::PerformanceScenarioMemoryData(
    PerformanceScenarioMemoryData&&) = default;

PerformanceScenarioMemoryData& PerformanceScenarioMemoryData::operator=(
    PerformanceScenarioMemoryData&&) = default;

}  // namespace performance_manager
