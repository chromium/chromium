// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/scenarios/performance_scenario_data.h"

#include <utility>

#include "base/memory/scoped_refptr.h"

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

PerformanceScenarioMemoryData::PerformanceScenarioMemoryData() = default;

PerformanceScenarioMemoryData::~PerformanceScenarioMemoryData() = default;

PerformanceScenarioMemoryData::PerformanceScenarioMemoryData(
    PerformanceScenarioMemoryData&&) = default;

PerformanceScenarioMemoryData& PerformanceScenarioMemoryData::operator=(
    PerformanceScenarioMemoryData&&) = default;

}  // namespace performance_manager
