// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/scenario_api/performance_scenario_test_support.h"

#include <atomic>
#include <memory>
#include <optional>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/structured_shared_memory.h"
#include "base/notreached.h"
#include "components/performance_manager/scenario_api/performance_scenario_memory.h"
#include "components/performance_manager/scenario_api/performance_scenario_observer.h"
#include "components/performance_manager/scenario_api/performance_scenarios.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_scenarios {

// static
std::unique_ptr<PerformanceScenarioTestHelper>
PerformanceScenarioTestHelper::Create() {
  auto test_helper = CreateWithoutMapping();
  if (!test_helper) {
    return nullptr;
  }
  auto global_region =
      test_helper->GetReadOnlyScenarioRegion(ScenarioScope::kGlobal);
  auto process_region =
      test_helper->GetReadOnlyScenarioRegion(ScenarioScope::kCurrentProcess);
  EXPECT_TRUE(global_region.IsValid());
  EXPECT_TRUE(process_region.IsValid());
  if (!global_region.IsValid() || !process_region.IsValid()) {
    return nullptr;
  }
  test_helper->global_read_only_memory_.emplace(ScenarioScope::kGlobal,
                                                std::move(global_region));
  test_helper->process_read_only_memory_.emplace(ScenarioScope::kCurrentProcess,
                                                 std::move(process_region));
  test_helper->observer_list_.emplace();
  return test_helper;
}

// static
std::unique_ptr<PerformanceScenarioTestHelper>
PerformanceScenarioTestHelper::CreateWithoutMapping() {
  auto global_state = base::StructuredSharedMemory<ScenarioState>::Create();
  auto process_state = base::StructuredSharedMemory<ScenarioState>::Create();
  EXPECT_TRUE(global_state.has_value());
  EXPECT_TRUE(process_state.has_value());
  if (!global_state.has_value() || !process_state.has_value()) {
    return nullptr;
  }
  return base::WrapUnique(new PerformanceScenarioTestHelper(
      std::move(global_state.value()), std::move(process_state.value())));
}

PerformanceScenarioTestHelper::PerformanceScenarioTestHelper(
    base::StructuredSharedMemory<ScenarioState> global_state,
    base::StructuredSharedMemory<ScenarioState> process_state)
    : global_state_(std::move(global_state)),
      process_state_(std::move(process_state)) {}

PerformanceScenarioTestHelper::~PerformanceScenarioTestHelper() = default;

base::ReadOnlySharedMemoryRegion
PerformanceScenarioTestHelper::GetReadOnlyScenarioRegion(
    ScenarioScope scope) const {
  return ScenarioStateForScope(scope).DuplicateReadOnlyRegion();
}

void PerformanceScenarioTestHelper::SetLoadingScenario(ScenarioScope scope,
                                                       LoadingScenario scenario,
                                                       bool notify) {
  ScenarioStateForScope(scope).WritableRef().loading.store(
      scenario, std::memory_order_relaxed);
  if (notify) {
    if (auto observer_list =
            PerformanceScenarioObserverList::GetForScope(scope)) {
      observer_list->NotifyIfScenarioChanged();
    }
  }
}

void PerformanceScenarioTestHelper::SetInputScenario(ScenarioScope scope,
                                                     InputScenario scenario,
                                                     bool notify) {
  ScenarioStateForScope(scope).WritableRef().input.store(
      scenario, std::memory_order_relaxed);
  if (notify) {
    if (auto observer_list =
            PerformanceScenarioObserverList::GetForScope(scope)) {
      observer_list->NotifyIfScenarioChanged();
    }
  }
}

base::StructuredSharedMemory<ScenarioState>&
PerformanceScenarioTestHelper::ScenarioStateForScope(ScenarioScope scope) {
  switch (scope) {
    case ScenarioScope::kGlobal:
      return global_state_;
    case ScenarioScope::kCurrentProcess:
      return process_state_;
  }
  NOTREACHED();
}

const base::StructuredSharedMemory<ScenarioState>&
PerformanceScenarioTestHelper::ScenarioStateForScope(
    ScenarioScope scope) const {
  return const_cast<PerformanceScenarioTestHelper*>(this)
      ->ScenarioStateForScope(scope);
}

}  // namespace performance_scenarios
