// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_SCENARIO_API_PERFORMANCE_SCENARIO_TEST_SUPPORT_H_
#define COMPONENTS_PERFORMANCE_MANAGER_SCENARIO_API_PERFORMANCE_SCENARIO_TEST_SUPPORT_H_

#include <memory>
#include <optional>

#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/structured_shared_memory.h"
#include "components/performance_manager/scenario_api/performance_scenario_memory.h"
#include "components/performance_manager/scenario_api/performance_scenarios.h"

namespace performance_scenarios {

// Helper to create and access performance scenario state from unit tests.
// In production the writable state is created automatically in the browser
// process, and read-only state is created automatically in child processes when
// they're launched.
//
// Usage:
//
// TEST(SomeTestSuite, TestWithNoScenarios) {
//   // Observers not available.
//   EXPECT_FALSE(PerformanceScenarioObserverList::GetForScope(
//       ScenarioScope::kGlobal));
//   // Scenarios all return default values.
//   EXPECT_EQ(GetInputScenario(ScenarioScope::kGlobal).load(
//       std::memory_order_relaxed), InputScenario::kNoInput);
// }
//
// TEST(SomeTestSuite, TestWithScenarios) {
//   auto test_helper = PerformanceScenarioTestHelper::Create();
//   ASSERT_TRUE(test_helper);
//
//   // Observers are available.
//   EXPECT_TRUE(PerformanceScenarioObserverList::GetForScope(
//       ScenarioScope::kGlobal));
//
//   // Scenarios can be updated.
//   test_helper->SetInputScenario(ScenarioScope::kGlobal,
//       InputScenario::kTyping);
//   EXPECT_EQ(GetInputScenario(ScenarioScope::kGlobal, InputScenario::kTyping);
// }
//
// TEST(SomeTestSuite, TestWithManualMapping) {
//   auto test_helper = PerformanceScenarioTestHelper::CreateWithoutMapping();
//   ASSERT_TRUE(test_helper);
//   test_helper->SetInputScenario(ScenarioScope::kGlobal,
//       InputScenario::kTyping);
//
//   // Observers aren't available until read-only memory is mapped.
//   EXPECT_FALSE(PerformanceScenarioObserverList::GetForScope(
//       ScenarioScope::kGlobal));
//
//   // Scenarios all return default values until read-only memory is mapped.
//   EXPECT_EQ(GetInputScenario(ScenarioScope::kGlobal).load(
//       std::memory_order_relaxed), InputScenario::kNoInput);
//
//   ScopedReadOnlyScenarioMemory global_memory(ScenarioScope::kGlobal,
//       test_helper->GetReadOnlyScenarioRegion(ScenarioScope::kGlobal));
//
//   EXPECT_TRUE(PerformanceScenarioObserverList::GetForScope(
//       ScenarioScope::kGlobal));
//   EXPECT_EQ(GetInputScenario(ScenarioScope::kGlobal).load(
//       std::memory_order_relaxed), InputScenario::kTyping);
// }
class PerformanceScenarioTestHelper {
 public:
  // Creates a PerformanceScenarioTestHelper holding writable performance
  // scenario memory, along with read-only memory and
  // PerformanceScenarioObserverLists for all ScenarioScopes. Returns null if
  // the shared memory couldn't be created or mapped.
  static std::unique_ptr<PerformanceScenarioTestHelper> Create();

  // Creates a PerformanceScenarioTestHelper holding writable performance
  // scenario memory, but doesn't map any read-only memory or create
  // PerformanceScenarioObserverLists. Tests can map read-only memory by passing
  // the result of GetReadOnlyScenarioRegion() to a ScopedReadOnlyScenarioMemory
  // object, and create PerformanceScenarioObserverLists by instantiating a
  // ScopedScenarioObserverList. Returns null if the shared memory couldn't be
  // created.
  static std::unique_ptr<PerformanceScenarioTestHelper> CreateWithoutMapping();

  ~PerformanceScenarioTestHelper();

  PerformanceScenarioTestHelper(const PerformanceScenarioTestHelper&) = delete;
  PerformanceScenarioTestHelper& operator=(
      const PerformanceScenarioTestHelper&) = delete;

  // Returns a read-only memory handle for the given `scope`, for tests that
  // create ScopedReadOnlyScenarioMemory manually.
  base::ReadOnlySharedMemoryRegion GetReadOnlyScenarioRegion(
      ScenarioScope scope) const;

  // Updates the LoadingScenario for the given `scope`, and notifies all
  // observers if `notify` is true.
  void SetLoadingScenario(ScenarioScope scope,
                          LoadingScenario scenario,
                          bool notify = true);

  // Updates the InputScenario for the given `scope`, and notifies all
  // observers if `notify` is true.
  void SetInputScenario(ScenarioScope scope,
                        InputScenario scenario,
                        bool notify = true);

 private:
  PerformanceScenarioTestHelper(
      base::StructuredSharedMemory<ScenarioState> global_state,
      base::StructuredSharedMemory<ScenarioState> process_state);

  base::StructuredSharedMemory<ScenarioState>& ScenarioStateForScope(
      ScenarioScope scope);
  const base::StructuredSharedMemory<ScenarioState>& ScenarioStateForScope(
      ScenarioScope scope) const;

  base::StructuredSharedMemory<ScenarioState> global_state_;
  base::StructuredSharedMemory<ScenarioState> process_state_;

  std::optional<ScopedReadOnlyScenarioMemory> global_read_only_memory_;
  std::optional<ScopedReadOnlyScenarioMemory> process_read_only_memory_;

  std::optional<ScopedScenarioObserverList> observer_list_;
};

}  // namespace performance_scenarios

#endif  // COMPONENTS_PERFORMANCE_MANAGER_SCENARIO_API_PERFORMANCE_SCENARIO_TEST_SUPPORT_H_
