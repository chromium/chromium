// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/scenario_api/performance_scenario_observer.h"

#include <atomic>
#include <memory>

#include "base/barrier_closure.h"
#include "base/containers/enum_set.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/scoped_multi_source_observation.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "components/performance_manager/scenario_api/performance_scenario_memory.h"
#include "components/performance_manager/scenario_api/performance_scenario_test_support.h"
#include "components/performance_manager/scenario_api/performance_scenarios.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_scenarios {

namespace {

using ::testing::_;

class MockPerformanceScenarioObserver : public PerformanceScenarioObserver {
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
using StrictMockPerformanceScenarioObserver =
    ::testing::StrictMock<MockPerformanceScenarioObserver>;

class MockMatchingScenarioObserver : public MatchingScenarioObserver {
 public:
  using MatchingScenarioObserver::MatchingScenarioObserver;

  MOCK_METHOD(void,
              OnScenarioMatchChanged,
              (ScenarioScope scope, bool matches_pattern),
              (override));
};
using StrictMockMatchingScenarioObserver =
    ::testing::StrictMock<MockMatchingScenarioObserver>;

class PerformanceScenarioObserverTest : public ::testing::Test {
 public:
  void SetUp() override {
    test_helper_ = PerformanceScenarioTestHelper::CreateWithoutMapping();
    ASSERT_TRUE(test_helper_);
  }

 protected:
  std::unique_ptr<PerformanceScenarioTestHelper> test_helper_;

  base::test::TaskEnvironment task_env_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(PerformanceScenarioObserverTest, GetForScope) {
  // Map kCurrentProcess memory before creating the observer list.
  ScopedReadOnlyScenarioMemory scoped_process_memory(
      ScenarioScope::kCurrentProcess,
      test_helper_->GetReadOnlyScenarioRegion(ScenarioScope::kCurrentProcess));

  EXPECT_FALSE(PerformanceScenarioObserverList::GetForScope(
      ScenarioScope::kCurrentProcess));
  EXPECT_FALSE(
      PerformanceScenarioObserverList::GetForScope(ScenarioScope::kGlobal));

  {
    ScopedScenarioObserverList scoped_observer_list;

    auto process_list = PerformanceScenarioObserverList::GetForScope(
        ScenarioScope::kCurrentProcess);
    auto global_list =
        PerformanceScenarioObserverList::GetForScope(ScenarioScope::kGlobal);
    ASSERT_TRUE(process_list);
    EXPECT_TRUE(process_list->IsInitializedForTesting());
    ASSERT_TRUE(global_list);
    EXPECT_FALSE(global_list->IsInitializedForTesting());

    {
      // Map kGlobal memory after creating the observer list.
      ScopedReadOnlyScenarioMemory scoped_global_memory(
          ScenarioScope::kGlobal,
          test_helper_->GetReadOnlyScenarioRegion(ScenarioScope::kGlobal));
      EXPECT_TRUE(global_list->IsInitializedForTesting());
    }
  }

  EXPECT_FALSE(PerformanceScenarioObserverList::GetForScope(
      ScenarioScope::kCurrentProcess));
  EXPECT_FALSE(
      PerformanceScenarioObserverList::GetForScope(ScenarioScope::kGlobal));
}

TEST_F(PerformanceScenarioObserverTest, NotifyOnChange) {
  ScopedScenarioObserverList scoped_observer_list;

  // Create a PerformanceScenarioObserver and a MatchingScenarioObserver, and
  // have them observe both scopes before mapping in any memory.
  StrictMockPerformanceScenarioObserver mock_observer;
  base::ScopedMultiSourceObservation<PerformanceScenarioObserverList,
                                     PerformanceScenarioObserver>
      scoped_observation(&mock_observer);

  StrictMockMatchingScenarioObserver mock_idle_observer(kDefaultIdleScenarios);
  base::ScopedMultiSourceObservation<PerformanceScenarioObserverList,
                                     MatchingScenarioObserver>
      idle_observation(&mock_idle_observer);

  for (ScenarioScope scope : ScenarioScopes::All()) {
    auto observer_list = PerformanceScenarioObserverList::GetForScope(scope);
    EXPECT_FALSE(observer_list->IsInitializedForTesting());
    scoped_observation.AddObservation(observer_list.get());
    idle_observation.AddObservation(observer_list.get());
  }

  // Update the process scenario state before mapping in scenario memory, to
  // make sure the state tracking doesn't depend on the state starting at
  // kNoPageLoading.
  test_helper_->SetLoadingScenario(ScenarioScope::kCurrentProcess,
                                   LoadingScenario::kFocusedPageLoading);
  ScopedReadOnlyScenarioMemory scoped_process_memory(
      ScenarioScope::kCurrentProcess,
      test_helper_->GetReadOnlyScenarioRegion(ScenarioScope::kCurrentProcess));
  ScopedReadOnlyScenarioMemory scoped_global_memory(
      ScenarioScope::kGlobal,
      test_helper_->GetReadOnlyScenarioRegion(ScenarioScope::kGlobal));

  EXPECT_FALSE(CurrentScenariosMatch(ScenarioScope::kCurrentProcess,
                                     kDefaultIdleScenarios));
  EXPECT_TRUE(
      CurrentScenariosMatch(ScenarioScope::kGlobal, kDefaultIdleScenarios));

  // Create another PerformanceScenarioObserver and MatchingScenarioObserver.
  // These should have the same state as the first two even though they're added
  // after the state is already mapped.
  StrictMockPerformanceScenarioObserver mock_observer2;
  base::ScopedMultiSourceObservation<PerformanceScenarioObserverList,
                                     PerformanceScenarioObserver>
      scoped_observation2(&mock_observer2);

  StrictMockMatchingScenarioObserver mock_idle_observer2(kDefaultIdleScenarios);
  base::ScopedMultiSourceObservation<PerformanceScenarioObserverList,
                                     MatchingScenarioObserver>
      idle_observation2(&mock_idle_observer2);

  // This observer won't be notified on LoadingScenario changes because it only
  // watches the InputScenario.
  StrictMockMatchingScenarioObserver mock_input_only_observer(
      ScenarioPattern{.input = {InputScenario::kNoInput}});
  base::ScopedMultiSourceObservation<PerformanceScenarioObserverList,
                                     MatchingScenarioObserver>
      input_only_observation(&mock_input_only_observer);

  for (ScenarioScope scope : ScenarioScopes::All()) {
    auto observer_list = PerformanceScenarioObserverList::GetForScope(scope);
    EXPECT_TRUE(observer_list->IsInitializedForTesting());
    scoped_observation2.AddObservation(observer_list.get());
    idle_observation2.AddObservation(observer_list.get());
    input_only_observation.AddObservation(observer_list.get());
  }

  // Utility function that waits for all mock expectations to be filled. The
  // test should invoke `task_env_.QuitClosure()` when all expected observer
  // methods are called.
  auto wait_for_expectations = [&] {
    using ::testing::Mock;
    task_env_.RunUntilQuit();
    EXPECT_TRUE(Mock::VerifyAndClearExpectations(&mock_observer));
    EXPECT_TRUE(Mock::VerifyAndClearExpectations(&mock_idle_observer));
    EXPECT_TRUE(Mock::VerifyAndClearExpectations(&mock_observer2));
    EXPECT_TRUE(Mock::VerifyAndClearExpectations(&mock_idle_observer2));
    EXPECT_TRUE(Mock::VerifyAndClearExpectations(&mock_input_only_observer));
  };

  // Toggle process loading scenario, then global loading scenario. 4 observers
  // will fire for each of 2 scopes.
  auto quit_closure = base::BarrierClosure(8, task_env_.QuitClosure());

  // kCurrentProcess scope transitions from kFocusedPageLoading (non-idle) ->
  // kBackgroundPageLoading (idle).
  EXPECT_CALL(mock_observer,
              OnLoadingScenarioChanged(ScenarioScope::kCurrentProcess,
                                       LoadingScenario::kFocusedPageLoading,
                                       LoadingScenario::kBackgroundPageLoading))
      .WillOnce(base::test::RunClosure(quit_closure));
  EXPECT_CALL(mock_observer2,
              OnLoadingScenarioChanged(ScenarioScope::kCurrentProcess,
                                       LoadingScenario::kFocusedPageLoading,
                                       LoadingScenario::kBackgroundPageLoading))
      .WillOnce(base::test::RunClosure(quit_closure));
  EXPECT_CALL(mock_idle_observer,
              OnScenarioMatchChanged(ScenarioScope::kCurrentProcess, true))
      .WillOnce(base::test::RunClosure(quit_closure));
  EXPECT_CALL(mock_idle_observer2,
              OnScenarioMatchChanged(ScenarioScope::kCurrentProcess, true))
      .WillOnce(base::test::RunClosure(quit_closure));

  // kGlobal scope transitions from kNoPageLoading (idle) -> kVisiblePageLoading
  // (non-idle).
  EXPECT_CALL(mock_observer,
              OnLoadingScenarioChanged(ScenarioScope::kGlobal,
                                       LoadingScenario::kNoPageLoading,
                                       LoadingScenario::kVisiblePageLoading))
      .WillOnce(base::test::RunClosure(quit_closure));
  EXPECT_CALL(mock_observer2,
              OnLoadingScenarioChanged(ScenarioScope::kGlobal,
                                       LoadingScenario::kNoPageLoading,
                                       LoadingScenario::kVisiblePageLoading))
      .WillOnce(base::test::RunClosure(quit_closure));
  EXPECT_CALL(mock_idle_observer,
              OnScenarioMatchChanged(ScenarioScope::kGlobal, false))
      .WillOnce(base::test::RunClosure(quit_closure));
  EXPECT_CALL(mock_idle_observer2,
              OnScenarioMatchChanged(ScenarioScope::kGlobal, false))
      .WillOnce(base::test::RunClosure(quit_closure));

  test_helper_->SetLoadingScenario(ScenarioScope::kCurrentProcess,
                                   LoadingScenario::kBackgroundPageLoading);
  test_helper_->SetLoadingScenario(ScenarioScope::kGlobal,
                                   LoadingScenario::kVisiblePageLoading);
  wait_for_expectations();

  // Toggle process scenario again without changing global scenario.
  // kBackgroundPageLoading (idle) -> kFocusedPageLoading (non-idle).
  quit_closure = base::BarrierClosure(4, task_env_.QuitClosure());
  EXPECT_CALL(mock_observer,
              OnLoadingScenarioChanged(ScenarioScope::kCurrentProcess,
                                       LoadingScenario::kBackgroundPageLoading,
                                       LoadingScenario::kFocusedPageLoading))
      .WillOnce(base::test::RunClosure(quit_closure));
  EXPECT_CALL(mock_observer2,
              OnLoadingScenarioChanged(ScenarioScope::kCurrentProcess,
                                       LoadingScenario::kBackgroundPageLoading,
                                       LoadingScenario::kFocusedPageLoading))
      .WillOnce(base::test::RunClosure(quit_closure));
  EXPECT_CALL(mock_idle_observer,
              OnScenarioMatchChanged(ScenarioScope::kCurrentProcess, false))
      .WillOnce(base::test::RunClosure(quit_closure));
  EXPECT_CALL(mock_idle_observer2,
              OnScenarioMatchChanged(ScenarioScope::kCurrentProcess, false))
      .WillOnce(base::test::RunClosure(quit_closure));

  test_helper_->SetLoadingScenario(ScenarioScope::kCurrentProcess,
                                   LoadingScenario::kFocusedPageLoading);
  wait_for_expectations();

  // Stop testing duplicate observers now.
  scoped_observation2.RemoveAllObservations();
  idle_observation2.RemoveAllObservations();

  // Stop observing the process scenario, then toggle both scenarios again.
  //
  // kCurrentProcess scope transitions from kFocusedPageLoading (non-idle) ->
  // kBackgroundPageLoading (idle), but shouldn't notify any observers.
  //
  // kGlobal scope transitions from kVisiblePageLoading (non-idle) ->
  // kNoPageLoading (idle).
  quit_closure = base::BarrierClosure(2, task_env_.QuitClosure());
  EXPECT_CALL(mock_observer,
              OnLoadingScenarioChanged(ScenarioScope::kGlobal,
                                       LoadingScenario::kVisiblePageLoading,
                                       LoadingScenario::kNoPageLoading))
      .WillOnce(base::test::RunClosure(quit_closure));
  EXPECT_CALL(mock_idle_observer,
              OnScenarioMatchChanged(ScenarioScope::kGlobal, true))
      .WillOnce(base::test::RunClosure(quit_closure));

  scoped_observation.RemoveObservation(
      PerformanceScenarioObserverList::GetForScope(
          ScenarioScope::kCurrentProcess)
          .get());
  idle_observation.RemoveObservation(
      PerformanceScenarioObserverList::GetForScope(
          ScenarioScope::kCurrentProcess)
          .get());
  input_only_observation.RemoveObservation(
      PerformanceScenarioObserverList::GetForScope(
          ScenarioScope::kCurrentProcess)
          .get());

  test_helper_->SetLoadingScenario(ScenarioScope::kCurrentProcess,
                                   LoadingScenario::kBackgroundPageLoading);
  test_helper_->SetLoadingScenario(ScenarioScope::kGlobal,
                                   LoadingScenario::kNoPageLoading);
  wait_for_expectations();

  // Update global scenario from kNoPageLoading to kBackgroundPageLoading. The
  // idle observer shouldn't be notified because the new scenario is still idle.
  EXPECT_CALL(mock_observer,
              OnLoadingScenarioChanged(ScenarioScope::kGlobal,
                                       LoadingScenario::kNoPageLoading,
                                       LoadingScenario::kBackgroundPageLoading))
      .WillOnce(base::test::RunClosure(task_env_.QuitClosure()));

  test_helper_->SetLoadingScenario(ScenarioScope::kGlobal,
                                   LoadingScenario::kBackgroundPageLoading);
  wait_for_expectations();

  // Update the global input scenario. All 3 observers will now be notified.
  quit_closure = base::BarrierClosure(3, task_env_.QuitClosure());
  EXPECT_CALL(mock_observer, OnInputScenarioChanged(ScenarioScope::kGlobal,
                                                    InputScenario::kNoInput,
                                                    InputScenario::kTyping))
      .WillOnce(base::test::RunClosure(quit_closure));
  EXPECT_CALL(mock_idle_observer,
              OnScenarioMatchChanged(ScenarioScope::kGlobal, false))
      .WillOnce(base::test::RunClosure(quit_closure));
  EXPECT_CALL(mock_input_only_observer,
              OnScenarioMatchChanged(ScenarioScope::kGlobal, false))
      .WillOnce(base::test::RunClosure(quit_closure));

  test_helper_->SetInputScenario(ScenarioScope::kGlobal,
                                 InputScenario::kTyping);
  wait_for_expectations();
}

}  // namespace

}  // namespace performance_scenarios
