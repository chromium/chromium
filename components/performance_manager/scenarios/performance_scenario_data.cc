// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/scenarios/performance_scenario_data.h"

#include <utility>

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/public/common/performance/performance_scenarios.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"

namespace performance_manager {

// static
scoped_refptr<RefCountedScenarioState> RefCountedScenarioState::Create() {
  auto shared_state =
      blink::performance_scenarios::SharedScenarioState::Create();
  if (shared_state.has_value()) {
    return base::WrapRefCounted(
        new RefCountedScenarioState(std::move(shared_state.value())));
  }
  return nullptr;
}

RefCountedScenarioState::RefCountedScenarioState(
    blink::performance_scenarios::SharedScenarioState shared_state)
    : shared_state_(std::move(shared_state)) {}

RefCountedScenarioState::~RefCountedScenarioState() = default;

void RefCountedScenarioState::RegisterTracingTracks(
    perfetto::Track parent_track) {
  if (parent_tracing_track_.has_value()) {
    // Already registered.
    return;
  }
  parent_tracing_track_.emplace(parent_track);

  uint64_t track_id = reinterpret_cast<uint64_t>(this);
  loading_tracing_track_.emplace(perfetto::NamedTrack(
      "LoadingPerformanceScenario", track_id, parent_track));
  input_tracing_track_.emplace(
      perfetto::NamedTrack("InputPerformanceScenario", track_id, parent_track));
}

PerformanceScenarioMemoryData::PerformanceScenarioMemoryData() = default;

PerformanceScenarioMemoryData::~PerformanceScenarioMemoryData() = default;

PerformanceScenarioMemoryData::PerformanceScenarioMemoryData(
    PerformanceScenarioMemoryData&&) = default;

PerformanceScenarioMemoryData& PerformanceScenarioMemoryData::operator=(
    PerformanceScenarioMemoryData&&) = default;

}  // namespace performance_manager
