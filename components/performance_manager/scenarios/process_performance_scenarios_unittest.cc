// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/scenarios/process_performance_scenarios.h"

#include <optional>

#include "base/containers/span.h"
#include "base/memory/platform_shared_memory_handle.h"
#include "base/memory/shared_memory_mapper.h"
#include "base/scoped_observation.h"
#include "components/performance_manager/embedder/scoped_global_scenario_memory.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/scenario_api/performance_scenario_observer.h"
#include "components/performance_manager/scenario_api/performance_scenarios.h"
#include "components/performance_manager/scenarios/browser_performance_scenarios.h"
#include "components/performance_manager/scenarios/performance_scenario_data.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/test_support/mock_graphs.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

namespace {

using performance_scenarios::InputScenario;
using performance_scenarios::LoadingScenario;
using performance_scenarios::MatchingScenarioObserver;
using performance_scenarios::PerformanceScenarioObserver;
using performance_scenarios::ScenarioScope;

class LenientMockPerformanceScenarioObserver
    : public PerformanceScenarioObserver {
 public:
  MOCK_METHOD(void,
              OnLoadingScenarioChanged,
              (ScenarioScope scope,
               LoadingScenario old_scenario,
               LoadingScenario new_scenario),
              (override));
  MOCK_METHOD(void,
              OnInputScenarioChanged,
              (ScenarioScope scope,
               InputScenario old_scenario,
               InputScenario new_scenario),
              (override));
};

using MockPerformanceScenarioObserver =
    ::testing::StrictMock<LenientMockPerformanceScenarioObserver>;

class LenientMockMatchingScenarioObserver : public MatchingScenarioObserver {
 public:
  explicit LenientMockMatchingScenarioObserver(
      performance_scenarios::ScenarioPattern pattern)
      : MatchingScenarioObserver(pattern) {}

  MOCK_METHOD(void,
              OnScenarioMatchChanged,
              (ScenarioScope scope, bool matches_pattern),
              (override));
};

using MockMatchingScenarioObserver =
    ::testing::StrictMock<LenientMockMatchingScenarioObserver>;

// A fake SharedMemoryMapper that fails to map in memory.
class FailingSharedMemoryMapper final : public base::SharedMemoryMapper {
 public:
  FailingSharedMemoryMapper() = default;
  ~FailingSharedMemoryMapper() = default;

  std::optional<base::span<uint8_t>> Map(
      base::subtle::PlatformSharedMemoryHandle handle,
      bool write_allowed,
      uint64_t offset,
      size_t size) final {
    return std::nullopt;
  }

  void Unmap(base::span<uint8_t> mapping) final {}
};

class ProcessPerformanceScenariosTest : public GraphTestHarness {
 private:
  // Always map global memory. Per-process memory will be mapped automatically
  // as process nodes are created.
  ScopedGlobalScenarioMemory scoped_global_scenario_memory_;
};

}  // namespace

TEST_F(ProcessPerformanceScenariosTest, LoadingScenario) {
  MockSinglePageWithMultipleProcessesGraph mock_graph(graph());

  EXPECT_EQ(GetProcessLoadingScenario(mock_graph.process.get()),
            LoadingScenario::kNoPageLoading);
  EXPECT_EQ(GetProcessLoadingScenario(mock_graph.other_process.get()),
            LoadingScenario::kNoPageLoading);

  MockPerformanceScenarioObserver mock_observer;
  base::ScopedObservation<ProcessNode, PerformanceScenarioObserver> observation(
      &mock_observer);
  observation.Observe(mock_graph.process.get());

  MockMatchingScenarioObserver mock_matching_observer(
      performance_scenarios::kDefaultIdleScenarios);
  base::ScopedObservation<ProcessNode, MatchingScenarioObserver>
      matching_observation(&mock_matching_observer);
  matching_observation.Observe(mock_graph.process.get());

  // Changing to kBackgroundPageLoading should invoke the
  // PerformanceScenarioObserver but not the MatchingScenarioObserver, since it
  // still matches kDefaultIdleScenarios.
  EXPECT_CALL(mock_observer, OnLoadingScenarioChanged(
                                 ScenarioScope::kCurrentProcess,
                                 LoadingScenario::kNoPageLoading,
                                 LoadingScenario::kBackgroundPageLoading));

  SetLoadingScenarioForProcessNode(LoadingScenario::kBackgroundPageLoading,
                                   mock_graph.process.get());
  EXPECT_EQ(GetProcessLoadingScenario(mock_graph.process.get()),
            LoadingScenario::kBackgroundPageLoading);
  ::testing::Mock::VerifyAndClearExpectations(&mock_observer);
  ::testing::Mock::VerifyAndClearExpectations(&mock_matching_observer);

  // Observers shouldn't be notified if the scenario doesn't change.
  SetLoadingScenarioForProcessNode(LoadingScenario::kBackgroundPageLoading,
                                   mock_graph.process.get());
  EXPECT_EQ(GetProcessLoadingScenario(mock_graph.process.get()),
            LoadingScenario::kBackgroundPageLoading);
  ::testing::Mock::VerifyAndClearExpectations(&mock_observer);
  ::testing::Mock::VerifyAndClearExpectations(&mock_matching_observer);

  // Observer shouldn't be notified for changes to a different process, or for
  // global changes.
  SetLoadingScenarioForProcessNode(LoadingScenario::kVisiblePageLoading,
                                   mock_graph.other_process.get());
  SetGlobalLoadingScenario(LoadingScenario::kFocusedPageLoading);
  EXPECT_EQ(GetProcessLoadingScenario(mock_graph.other_process.get()),
            LoadingScenario::kVisiblePageLoading);
  ::testing::Mock::VerifyAndClearExpectations(&mock_observer);
  ::testing::Mock::VerifyAndClearExpectations(&mock_matching_observer);

  // Changing to kVisiblePageLoading should notify both observers since the
  // scenario no longer matches kDefaultIdleScenarios.
  EXPECT_CALL(mock_observer,
              OnLoadingScenarioChanged(ScenarioScope::kCurrentProcess,
                                       LoadingScenario::kBackgroundPageLoading,
                                       LoadingScenario::kVisiblePageLoading));
  EXPECT_CALL(mock_matching_observer,
              OnScenarioMatchChanged(ScenarioScope::kCurrentProcess, false));

  SetLoadingScenarioForProcessNode(LoadingScenario::kVisiblePageLoading,
                                   mock_graph.process.get());
  EXPECT_EQ(GetProcessLoadingScenario(mock_graph.process.get()),
            LoadingScenario::kVisiblePageLoading);
  ::testing::Mock::VerifyAndClearExpectations(&mock_observer);
  ::testing::Mock::VerifyAndClearExpectations(&mock_matching_observer);
}

TEST_F(ProcessPerformanceScenariosTest, InputScenario) {
  MockSinglePageWithMultipleProcessesGraph mock_graph(graph());

  EXPECT_EQ(GetProcessInputScenario(mock_graph.process.get()),
            InputScenario::kNoInput);
  EXPECT_EQ(GetProcessInputScenario(mock_graph.other_process.get()),
            InputScenario::kNoInput);

  MockPerformanceScenarioObserver mock_observer;
  base::ScopedObservation<ProcessNode, PerformanceScenarioObserver> observation(
      &mock_observer);
  observation.Observe(mock_graph.process.get());

  MockMatchingScenarioObserver mock_matching_observer(
      performance_scenarios::kDefaultIdleScenarios);
  base::ScopedObservation<ProcessNode, MatchingScenarioObserver>
      matching_observation(&mock_matching_observer);
  matching_observation.Observe(mock_graph.process.get());

  // Changing to kTyping should notify both observers since the scenario no
  // longer matches kDefaultIdleScenarios.
  EXPECT_CALL(
      mock_observer,
      OnInputScenarioChanged(ScenarioScope::kCurrentProcess,
                             InputScenario::kNoInput, InputScenario::kTyping));
  EXPECT_CALL(mock_matching_observer,
              OnScenarioMatchChanged(ScenarioScope::kCurrentProcess, false));

  SetInputScenarioForProcessNode(InputScenario::kTyping,
                                 mock_graph.process.get());
  EXPECT_EQ(GetProcessInputScenario(mock_graph.process.get()),
            InputScenario::kTyping);
  ::testing::Mock::VerifyAndClearExpectations(&mock_observer);
  ::testing::Mock::VerifyAndClearExpectations(&mock_matching_observer);

  // Observers shouldn't be notified if the scenario doesn't change.
  SetInputScenarioForProcessNode(InputScenario::kTyping,
                                 mock_graph.process.get());
  EXPECT_EQ(GetProcessInputScenario(mock_graph.process.get()),
            InputScenario::kTyping);
  ::testing::Mock::VerifyAndClearExpectations(&mock_observer);
  ::testing::Mock::VerifyAndClearExpectations(&mock_matching_observer);

  // Observer shouldn't be notified for changes to a different process, or for
  // global changes.
  SetInputScenarioForProcessNode(InputScenario::kTap,
                                 mock_graph.other_process.get());
  SetGlobalInputScenario(InputScenario::kScroll);
  EXPECT_EQ(GetProcessInputScenario(mock_graph.other_process.get()),
            InputScenario::kTap);
  ::testing::Mock::VerifyAndClearExpectations(&mock_observer);
  ::testing::Mock::VerifyAndClearExpectations(&mock_matching_observer);

  // Changing to kTap should invoke the PerformanceScenarioObserver but not the
  // MatchingScenarioObserver, since it still doesn't match
  // kDefaultIdleScenarios.
  EXPECT_CALL(mock_observer, OnInputScenarioChanged(
                                 ScenarioScope::kCurrentProcess,
                                 InputScenario::kTyping, InputScenario::kTap));

  SetInputScenarioForProcessNode(InputScenario::kTap, mock_graph.process.get());
  EXPECT_EQ(GetProcessInputScenario(mock_graph.process.get()),
            InputScenario::kTap);
  ::testing::Mock::VerifyAndClearExpectations(&mock_observer);
  ::testing::Mock::VerifyAndClearExpectations(&mock_matching_observer);
}

TEST_F(ProcessPerformanceScenariosTest, NoMemory) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());

  // First access to the PerformanceScenarioData uses a failing mapper.
  FailingSharedMemoryMapper failing_mapper;
  EXPECT_FALSE(PerformanceScenarioData::Exists(mock_graph.process.get()));
  auto& data = PerformanceScenarioData::GetOrCreate(mock_graph.process.get(),
                                                    &failing_mapper);
  EXPECT_FALSE(data.HasSharedState());

  // Reading the process scenario should return default values when memory
  // mapping failed.
  EXPECT_EQ(GetProcessLoadingScenario(mock_graph.process.get()),
            LoadingScenario::kNoPageLoading);
  EXPECT_EQ(GetProcessInputScenario(mock_graph.process.get()),
            InputScenario::kNoInput);

  // Writing to process state should do nothing. Observers should not be
  // notified.
  MockPerformanceScenarioObserver mock_observer;
  base::ScopedObservation<ProcessNode, PerformanceScenarioObserver> observation(
      &mock_observer);
  observation.Observe(mock_graph.process.get());

  MockMatchingScenarioObserver mock_matching_observer(
      performance_scenarios::kDefaultIdleScenarios);
  base::ScopedObservation<ProcessNode, MatchingScenarioObserver>
      matching_observation(&mock_matching_observer);
  matching_observation.Observe(mock_graph.process.get());

  SetLoadingScenarioForProcessNode(LoadingScenario::kVisiblePageLoading,
                                   mock_graph.process.get());
  SetInputScenarioForProcessNode(InputScenario::kTyping,
                                 mock_graph.process.get());
  EXPECT_EQ(GetProcessLoadingScenario(mock_graph.process.get()),
            LoadingScenario::kNoPageLoading);
  EXPECT_EQ(GetProcessInputScenario(mock_graph.process.get()),
            InputScenario::kNoInput);
}

}  // namespace performance_manager
