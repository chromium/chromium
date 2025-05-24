// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/scenarios/performance_scenario_data.h"

#include <memory>
#include <utility>

#include "base/memory/shared_memory_mapper.h"
#include "base/memory/structured_shared_memory.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/public/tracing_support.h"
#include "components/performance_manager/scenario_api/performance_scenario_memory.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"

namespace performance_manager {

namespace {

perfetto::NamedTrack CreateTracingTrack(const ProcessNode* process_node,
                                        perfetto::StaticString name,
                                        uint64_t track_id) {
  if (process_node) {
    return CreateProcessTracingTrack(process_node, name, track_id);
  } else {
    return perfetto::NamedTrack(name, track_id, perfetto::Track::Global(0));
  }
}

}  // namespace

// static
PerformanceScenarioData& PerformanceScenarioData::GetOrCreate(
    const ProcessNode* process_node,
    base::SharedMemoryMapper* mapper) {
  auto* process_node_impl = ProcessNodeImpl::FromNode(process_node);
  if (Exists(process_node_impl)) {
    return Get(process_node_impl);
  }
  return Create(process_node_impl, mapper);
}

PerformanceScenarioData::PerformanceScenarioData(
    base::SharedMemoryMapper* mapper)
    : shared_state_(mapper ? SharedScenarioState::CreateWithCustomMapper(mapper)
                           : SharedScenarioState::Create()) {}

PerformanceScenarioData::~PerformanceScenarioData() = default;

PerformanceScenarioData::PerformanceScenarioData(PerformanceScenarioData&&) =
    default;

PerformanceScenarioData& PerformanceScenarioData::operator=(
    PerformanceScenarioData&&) = default;

void PerformanceScenarioData::EnsureTracingTracks(
    const ProcessNode* process_node) {
  if (process_node && !HasProcessTracingTrack(process_node)) {
    return;
  }
  uint64_t track_id = reinterpret_cast<uint64_t>(tracing_tracks_.get());
  if (!tracing_tracks_->loading_track.has_value()) {
    tracing_tracks_->loading_track.emplace(CreateTracingTrack(
        process_node, "LoadingPerformanceScenario", track_id));
  }
  if (!tracing_tracks_->input_track.has_value()) {
    tracing_tracks_->input_track.emplace(
        CreateTracingTrack(process_node, "InputPerformanceScenario", track_id));
  }
}

}  // namespace performance_manager
