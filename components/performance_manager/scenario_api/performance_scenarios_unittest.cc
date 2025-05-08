// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/scenario_api/performance_scenarios.h"

#include <atomic>
#include <ostream>
#include <utility>

#include "base/containers/enum_set.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/structured_shared_memory.h"
#include "components/performance_manager/scenario_api/performance_scenario_memory.h"
#include "components/performance_manager/scenario_api/performance_scenario_test_support.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_scenarios {

// Test printers.
std::ostream& operator<<(std::ostream& os, LoadingScenario loading_scenario) {
  switch (loading_scenario) {
    case LoadingScenario::kNoPageLoading:
      return os << "NoPageLoading";
    case LoadingScenario::kFocusedPageLoading:
      return os << "FocusedPageLoading";
    case LoadingScenario::kVisiblePageLoading:
      return os << "VisiblePageLoading";
    case LoadingScenario::kBackgroundPageLoading:
      return os << "BackgroundPageLoading";
  }
}

std::ostream& operator<<(std::ostream& os, InputScenario input_scenario) {
  switch (input_scenario) {
    case InputScenario::kNoInput:
      return os << "NoInput";
    case InputScenario::kTyping:
      return os << "TypingInput";
    case InputScenario::kTap:
      return os << "Tap";
    case InputScenario::kScroll:
      return os << "Scroll";
  }
}

namespace {

// Tests that are repeated for all scenarios of a given type.
using PerformanceScenariosAllLoadingScenariosTest =
    ::testing::TestWithParam<LoadingScenario>;
using PerformanceScenariosAllInputScenariosTest =
    ::testing::TestWithParam<InputScenario>;

INSTANTIATE_TEST_SUITE_P(All,
                         PerformanceScenariosAllLoadingScenariosTest,
                         ::testing::ValuesIn(LoadingScenarios::All()));
INSTANTIATE_TEST_SUITE_P(All,
                         PerformanceScenariosAllInputScenariosTest,
                         ::testing::ValuesIn(InputScenarios::All()));

TEST(PerformanceScenariosTest, MappedScenarioState) {
  auto test_helper = PerformanceScenarioTestHelper::CreateWithoutMapping();
  ASSERT_TRUE(test_helper);

  // Before the shared memory is mapped in, GetLoadingScenario should return
  // default values.
  EXPECT_EQ(GetLoadingScenario(ScenarioScope::kCurrentProcess)
                ->load(std::memory_order_relaxed),
            LoadingScenario::kNoPageLoading);
  EXPECT_EQ(GetLoadingScenario(ScenarioScope::kGlobal)
                ->load(std::memory_order_relaxed),
            LoadingScenario::kNoPageLoading);

  {
    // Map the shared memory as the global state.
    ScopedReadOnlyScenarioMemory mapped_global_memory(
        ScenarioScope::kGlobal,
        test_helper->GetReadOnlyScenarioRegion(ScenarioScope::kGlobal));
    EXPECT_EQ(GetLoadingScenario(ScenarioScope::kGlobal)
                  ->load(std::memory_order_relaxed),
              LoadingScenario::kNoPageLoading);

    // Updates should be visible in the global state only.
    test_helper->SetLoadingScenario(ScenarioScope::kGlobal,
                                    LoadingScenario::kFocusedPageLoading);
    EXPECT_EQ(GetLoadingScenario(ScenarioScope::kGlobal)
                  ->load(std::memory_order_relaxed),
              LoadingScenario::kFocusedPageLoading);
    EXPECT_EQ(GetLoadingScenario(ScenarioScope::kCurrentProcess)
                  ->load(std::memory_order_relaxed),
              LoadingScenario::kNoPageLoading);

    // Map the same shared memory as the per-process state.
    ScopedReadOnlyScenarioMemory mapped_current_memory(
        ScenarioScope::kCurrentProcess,
        test_helper->GetReadOnlyScenarioRegion(ScenarioScope::kGlobal));
    EXPECT_EQ(GetLoadingScenario(ScenarioScope::kCurrentProcess)
                  ->load(std::memory_order_relaxed),
              LoadingScenario::kFocusedPageLoading);

    // Updates should be visible in both mappings.
    test_helper->SetLoadingScenario(ScenarioScope::kGlobal,
                                    LoadingScenario::kVisiblePageLoading);
    EXPECT_EQ(GetLoadingScenario(ScenarioScope::kGlobal)
                  ->load(std::memory_order_relaxed),
              LoadingScenario::kVisiblePageLoading);
    EXPECT_EQ(GetLoadingScenario(ScenarioScope::kCurrentProcess)
                  ->load(std::memory_order_relaxed),
              LoadingScenario::kVisiblePageLoading);
  }

  // After going out of scope, the memory is unmapped and GetLoadingScenario
  // should see default values again.
  EXPECT_EQ(GetLoadingScenario(ScenarioScope::kCurrentProcess)
                ->load(std::memory_order_relaxed),
            LoadingScenario::kNoPageLoading);
  EXPECT_EQ(GetLoadingScenario(ScenarioScope::kGlobal)
                ->load(std::memory_order_relaxed),
            LoadingScenario::kNoPageLoading);
}

TEST(PerformanceScenariosTest, SharedAtomicRef) {
  // Create and map shared memory.
  auto test_helper = PerformanceScenarioTestHelper::CreateWithoutMapping();
  ASSERT_TRUE(test_helper);
  auto read_only_mapping =
      base::StructuredSharedMemory<ScenarioState>::MapReadOnlyRegion(
          test_helper->GetReadOnlyScenarioRegion(ScenarioScope::kGlobal));
  ASSERT_TRUE(read_only_mapping.has_value());

  // Store pointers to the atomics in the shared memory for later comparison.
  const std::atomic<LoadingScenario>* loading_ptr =
      &(read_only_mapping->ReadOnlyRef().loading);
  const std::atomic<InputScenario>* input_ptr =
      &(read_only_mapping->ReadOnlyRef().input);

  // Transfer ownership of the mapping to a scoped_refptr.
  auto mapping_ptr = base::MakeRefCounted<RefCountedScenarioMapping>(
      std::move(read_only_mapping.value()));

  SharedAtomicRef<LoadingScenario> loading_ref(
      mapping_ptr, mapping_ptr->data.ReadOnlyRef().loading);
  SharedAtomicRef<InputScenario> input_ref(
      mapping_ptr, mapping_ptr->data.ReadOnlyRef().input);

  // The SharedAtomicRef's should keep the mapping alive.
  mapping_ptr.reset();
  test_helper->SetLoadingScenario(ScenarioScope::kGlobal,
                                  LoadingScenario::kBackgroundPageLoading);
  test_helper->SetInputScenario(ScenarioScope::kGlobal, InputScenario::kTyping);

  // get()
  EXPECT_EQ(loading_ref.get(), loading_ptr);
  EXPECT_EQ(input_ref.get(), input_ptr);

  // operator*
  EXPECT_EQ(*loading_ref, *loading_ptr);
  EXPECT_EQ(*input_ref, *input_ptr);

  // operator->
  EXPECT_EQ(loading_ref->load(std::memory_order_relaxed),
            LoadingScenario::kBackgroundPageLoading);
  EXPECT_EQ(input_ref->load(std::memory_order_relaxed), InputScenario::kTyping);
}

TEST_P(PerformanceScenariosAllLoadingScenariosTest, EmptyScenarioPattern) {
  // Nothing matches the pattern.
  static ScenarioPattern kEmptyScenarioPattern{};
  EXPECT_TRUE(ScenariosMatch(GetParam(), InputScenario::kNoInput,
                             kEmptyScenarioPattern));
  EXPECT_TRUE(ScenariosMatch(GetParam(), InputScenario::kTyping,
                             kEmptyScenarioPattern));
  EXPECT_TRUE(
      ScenariosMatch(GetParam(), InputScenario::kTap, kEmptyScenarioPattern));
  EXPECT_TRUE(ScenariosMatch(GetParam(), InputScenario::kScroll,
                             kEmptyScenarioPattern));
}

TEST_P(PerformanceScenariosAllLoadingScenariosTest, NoInputScenarioPattern) {
  // kNoInput matches the pattern, loading scenarios are ignored.
  static ScenarioPattern kInputScenarioPattern{
      .input = {InputScenario::kNoInput},
  };
  EXPECT_TRUE(ScenariosMatch(GetParam(), InputScenario::kNoInput,
                             kInputScenarioPattern));
  EXPECT_FALSE(ScenariosMatch(GetParam(), InputScenario::kTyping,
                              kInputScenarioPattern));
  EXPECT_FALSE(
      ScenariosMatch(GetParam(), InputScenario::kTap, kInputScenarioPattern));
  EXPECT_FALSE(ScenariosMatch(GetParam(), InputScenario::kScroll,
                              kInputScenarioPattern));
}

TEST_P(PerformanceScenariosAllLoadingScenariosTest, InputScenarioPattern) {
  // All except kNoInput matches the pattern, loading scenarios are ignored.
  static ScenarioPattern kInputScenarioPattern{
      .input = {InputScenario::kTyping, InputScenario::kTap,
                InputScenario::kScroll},
  };
  EXPECT_FALSE(ScenariosMatch(GetParam(), InputScenario::kNoInput,
                              kInputScenarioPattern));
  EXPECT_TRUE(ScenariosMatch(GetParam(), InputScenario::kTyping,
                             kInputScenarioPattern));
  EXPECT_TRUE(
      ScenariosMatch(GetParam(), InputScenario::kTap, kInputScenarioPattern));
  EXPECT_TRUE(ScenariosMatch(GetParam(), InputScenario::kScroll,
                             kInputScenarioPattern));
}

TEST_P(PerformanceScenariosAllInputScenariosTest, LoadingScenarioPattern) {
  // Only kNoPageLoading matches the pattern, input scenarios are ignored.
  static ScenarioPattern kNotLoadingScenarioPattern{
      .loading = {LoadingScenario::kNoPageLoading},
  };
  EXPECT_TRUE(ScenariosMatch(LoadingScenario::kNoPageLoading, GetParam(),
                             kNotLoadingScenarioPattern));
  EXPECT_FALSE(ScenariosMatch(LoadingScenario::kBackgroundPageLoading,
                              GetParam(), kNotLoadingScenarioPattern));
  EXPECT_FALSE(ScenariosMatch(LoadingScenario::kVisiblePageLoading, GetParam(),
                              kNotLoadingScenarioPattern));
  EXPECT_FALSE(ScenariosMatch(LoadingScenario::kFocusedPageLoading, GetParam(),
                              kNotLoadingScenarioPattern));

  // Loading background pages match the pattern, input scenarios are ignored.
  static ScenarioPattern kBackgroundLoadingScenarioPattern{
      .loading = {LoadingScenario::kNoPageLoading,
                  LoadingScenario::kBackgroundPageLoading},
  };
  EXPECT_TRUE(ScenariosMatch(LoadingScenario::kNoPageLoading, GetParam(),
                             kBackgroundLoadingScenarioPattern));
  EXPECT_TRUE(ScenariosMatch(LoadingScenario::kBackgroundPageLoading,
                             GetParam(), kBackgroundLoadingScenarioPattern));
  EXPECT_FALSE(ScenariosMatch(LoadingScenario::kVisiblePageLoading, GetParam(),
                              kBackgroundLoadingScenarioPattern));
  EXPECT_FALSE(ScenariosMatch(LoadingScenario::kFocusedPageLoading, GetParam(),
                              kBackgroundLoadingScenarioPattern));

  // Loading non-focused pages match the pattern, input scenarios are ignored.
  static ScenarioPattern kNonFocusedLoadingScenarioPattern{
      .loading = {LoadingScenario::kNoPageLoading,
                  LoadingScenario::kBackgroundPageLoading,
                  LoadingScenario::kVisiblePageLoading},
  };
  EXPECT_TRUE(ScenariosMatch(LoadingScenario::kNoPageLoading, GetParam(),
                             kNonFocusedLoadingScenarioPattern));
  EXPECT_TRUE(ScenariosMatch(LoadingScenario::kBackgroundPageLoading,
                             GetParam(), kNonFocusedLoadingScenarioPattern));
  EXPECT_TRUE(ScenariosMatch(LoadingScenario::kVisiblePageLoading, GetParam(),
                             kNonFocusedLoadingScenarioPattern));
  EXPECT_FALSE(ScenariosMatch(LoadingScenario::kFocusedPageLoading, GetParam(),
                              kNonFocusedLoadingScenarioPattern));
}

// By default, scenarios are only idle with no foreground loading and no input.
// If one of these tests fails, check if the definition of kDefaultIdleScenarios
// has changed.

TEST(PerformanceScenariosTest, DefaultIdleScenariosNoInput) {
  EXPECT_TRUE(ScenariosMatch(LoadingScenario::kNoPageLoading,
                             InputScenario::kNoInput, kDefaultIdleScenarios));
  EXPECT_TRUE(ScenariosMatch(LoadingScenario::kBackgroundPageLoading,
                             InputScenario::kNoInput, kDefaultIdleScenarios));
  EXPECT_FALSE(ScenariosMatch(LoadingScenario::kVisiblePageLoading,
                              InputScenario::kNoInput, kDefaultIdleScenarios));
  EXPECT_FALSE(ScenariosMatch(LoadingScenario::kFocusedPageLoading,
                              InputScenario::kNoInput, kDefaultIdleScenarios));
}

TEST_P(PerformanceScenariosAllLoadingScenariosTest,
       DefaultIdleScenariosWithInput) {
  EXPECT_FALSE(ScenariosMatch(GetParam(), InputScenario::kTyping,
                              kDefaultIdleScenarios));
}

// Ensure that all values of LoadingScenario are ordered correctly.
TEST_P(PerformanceScenariosAllLoadingScenariosTest, LoadingScenarioOrdering) {
  // When adding a new scenario to this switch, make sure that everything
  // comparing less than the new scenario is less performance-sensitive.
  switch (GetParam()) {
    case LoadingScenario::kFocusedPageLoading:
      EXPECT_LT(LoadingScenario::kVisiblePageLoading, GetParam());
      [[fallthrough]];
    case LoadingScenario::kVisiblePageLoading:
      EXPECT_LT(LoadingScenario::kBackgroundPageLoading, GetParam());
      [[fallthrough]];
    case LoadingScenario::kBackgroundPageLoading:
      EXPECT_LT(LoadingScenario::kNoPageLoading, GetParam());
      [[fallthrough]];
    case LoadingScenario::kNoPageLoading:
      // This is the lowest priority scenario.
      break;
  }
}

// Ensure that all values of InputScenario are ordered correctly.
TEST_P(PerformanceScenariosAllInputScenariosTest, InputScenarioOrdering) {
  // When adding a new scenario to this switch, make sure that everything
  // comparing less than the new scenario is less performance-sensitive.
  switch (GetParam()) {
    case InputScenario::kScroll:
      // Scrolling is the most performance-sensitive because scroll jank is
      // very obvious to the user.
      EXPECT_LT(InputScenario::kTap, GetParam());
      [[fallthrough]];
    case InputScenario::kTap:
      // Tap is more performance-sensitive than typing because there's a tactile
      // link: users expect immediate screen feedback when touching the screen.
      EXPECT_LT(InputScenario::kTyping, GetParam());
      [[fallthrough]];
    case InputScenario::kTyping:
      EXPECT_LT(InputScenario::kNoInput, GetParam());
      [[fallthrough]];
    case InputScenario::kNoInput:
      // This is the lowest priority scenario.
      break;
  }
}

}  // namespace

}  // namespace performance_scenarios
