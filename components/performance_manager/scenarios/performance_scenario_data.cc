// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/scenarios/performance_scenario_data.h"

namespace performance_manager {

PerformanceScenarioMemoryData::PerformanceScenarioMemoryData() = default;

PerformanceScenarioMemoryData::~PerformanceScenarioMemoryData() = default;

PerformanceScenarioMemoryData::PerformanceScenarioMemoryData(
    PerformanceScenarioMemoryData&&) = default;

PerformanceScenarioMemoryData& PerformanceScenarioMemoryData::operator=(
    PerformanceScenarioMemoryData&&) = default;

}  // namespace performance_manager
