// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/scenarios/browser_performance_scenarios.h"

#include <atomic>
#include <optional>
#include <utility>

#include "base/containers/span.h"
#include "base/memory/platform_shared_memory_handle.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/shared_memory_mapper.h"
#include "base/memory/weak_ptr.h"
#include "base/test/gmock_callback_support.h"
#include "components/performance_manager/embedder/scoped_global_scenario_memory.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/scenario_api/performance_scenario_memory.h"
#include "components/performance_manager/scenario_api/performance_scenario_observer.h"
#include "components/performance_manager/scenario_api/performance_scenarios.h"
#include "components/performance_manager/scenarios/performance_scenario_data.h"
#include "components/performance_manager/test_support/performance_manager_test_harness.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/navigation_simulator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace performance_manager {

namespace {

using performance_scenarios::GetLoadingScenario;
using performance_scenarios::PerformanceScenarioObserver;
using performance_scenarios::PerformanceScenarioObserverList;
using performance_scenarios::ScenarioScope;
using performance_scenarios::ScopedReadOnlyScenarioMemory;
using ::testing::_;

// Since the browser process also maps in a read-only view of the global
// scenario state for querying outside performance_manager, the observer is also
// notified.
class MockPerformanceScenarioObserver : public PerformanceScenarioObserver {
 public:
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

class BrowserPerformanceScenariosTest : public PerformanceManagerTestHarness {
 public:
  void SetUp() override {
    PerformanceManagerTestHarness::SetUp();

    // Load a page with FrameNodes and ProcessNodes.
    SetContents(CreateTestWebContents());
    content::NavigationSimulator::NavigateAndCommitFromBrowser(
        web_contents(), GURL("https://www.example.com"));
  }

  // Returns the ProcessNode for the main frame's process, or nullptr if there
  // is none.
  const ProcessNode* main_process_node() {
    if (!process()) {
      return nullptr;
    }
    base::WeakPtr<ProcessNode> process_node =
        PerformanceManager::GetProcessNodeForRenderProcessHost(process());
    return process_node.get();
  }
};

TEST_F(BrowserPerformanceScenariosTest, SetWithoutSharedMemory) {
  // Can't set up the scenario observer without mapped memory.
  EXPECT_FALSE(
      PerformanceScenarioObserverList::GetForScope(ScenarioScope::kGlobal));

  // When the global shared scenario memory isn't set up, setting a scenario
  // should silently do nothing. (Process scenario memory is scoped to the
  // ProcessNode so will always be mapped as needed.)
  SetGlobalLoadingScenario(LoadingScenario::kFocusedPageLoading);
  EXPECT_EQ(GetLoadingScenario(ScenarioScope::kGlobal)
                ->load(std::memory_order_relaxed),
            LoadingScenario::kNoPageLoading);
}

TEST_F(BrowserPerformanceScenariosTest, SetWithSharedMemory) {
  StrictMockPerformanceScenarioObserver mock_observer;
  EXPECT_CALL(mock_observer,
              OnLoadingScenarioChanged(ScenarioScope::kGlobal,
                                       LoadingScenario::kNoPageLoading,
                                       LoadingScenario::kFocusedPageLoading))
      .WillOnce(base::test::RunOnceClosure(task_environment()->QuitClosure()));

  // Create writable shared memory for the global state. This maps a read-only
  // view of the memory in as well so changes immediately become visible to the
  // current (browser) process.
  ScopedGlobalScenarioMemory global_shared_memory;
  auto observer_list =
      PerformanceScenarioObserverList::GetForScope(ScenarioScope::kGlobal);
  ASSERT_TRUE(observer_list);
  observer_list->AddObserver(&mock_observer);

  SetGlobalLoadingScenario(LoadingScenario::kFocusedPageLoading);
  EXPECT_EQ(GetLoadingScenario(ScenarioScope::kGlobal)
                ->load(std::memory_order_relaxed),
            LoadingScenario::kFocusedPageLoading);

  // PerformanceScenarioObserverList is an ObserverListThreadSafe that posts
  // a message to notify. Need to wait for the message.
  task_environment()->RunUntilQuit();
  ::testing::Mock::VerifyAndClearExpectations(&mock_observer);

  // Create writable shared memory for a render process state. Since this is
  // called in the browser process and the state is for a different process, it
  // doesn't map in a read-only view.
  ASSERT_TRUE(process());
  ASSERT_TRUE(main_process_node());
  base::ReadOnlySharedMemoryRegion process_region =
      GetSharedScenarioRegionForProcessNode(main_process_node());
  EXPECT_TRUE(process_region.IsValid());
  SetLoadingScenarioForProcess(LoadingScenario::kVisiblePageLoading, process());

  EXPECT_EQ(GetLoadingScenario(ScenarioScope::kCurrentProcess)
                ->load(std::memory_order_relaxed),
            LoadingScenario::kNoPageLoading);

  // Map in the read-only view of `process_region`. Normally this would be done
  // in the renderer process as the "current process" state. The state should
  // now become visible.
  ScopedReadOnlyScenarioMemory process_shared_memory(
      ScenarioScope::kCurrentProcess, std::move(process_region));
  EXPECT_EQ(GetLoadingScenario(ScenarioScope::kCurrentProcess)
                ->load(std::memory_order_relaxed),
            LoadingScenario::kVisiblePageLoading);

  observer_list->RemoveObserver(&mock_observer);
}

TEST_F(BrowserPerformanceScenariosTest, MappingFailure) {
  FailingSharedMemoryMapper failing_mapper;
  ScopedGlobalScenarioMemory global_shared_memory(&failing_mapper);

  // When the mapping fails, observer lists should exist but do nothing, so that
  // callers don't have to handle unexpected nullptr's on failure. (They should
  // only be null during process startup, before ScopedReadOnlyScenarioMemory
  // exists.)
  StrictMockPerformanceScenarioObserver mock_observer;
  auto global_observer_list =
      PerformanceScenarioObserverList::GetForScope(ScenarioScope::kGlobal);
  ASSERT_TRUE(global_observer_list);
  global_observer_list->AddObserver(&mock_observer);

  // Setting the global scenario should safely be ignored.
  SetGlobalLoadingScenario(LoadingScenario::kFocusedPageLoading);
  EXPECT_EQ(GetLoadingScenario(ScenarioScope::kGlobal)
                ->load(std::memory_order_relaxed),
            LoadingScenario::kNoPageLoading);

  // First access to the PerformanceScenarioData uses the failing mapper.
  ASSERT_TRUE(process());
  ASSERT_TRUE(main_process_node());
  PerformanceScenarioData::GetOrCreate(main_process_node(), &failing_mapper);
  base::ReadOnlySharedMemoryRegion process_region =
      GetSharedScenarioRegionForProcessNode(main_process_node());
  EXPECT_FALSE(process_region.IsValid());

  // Trying to map in the read-only view should safely create no-op observers
  // and cause reads and writes to do nothing.
  ScopedReadOnlyScenarioMemory process_shared_memory(
      ScenarioScope::kCurrentProcess, std::move(process_region));

  auto process_observer_list = PerformanceScenarioObserverList::GetForScope(
      ScenarioScope::kCurrentProcess);
  ASSERT_TRUE(process_observer_list);
  process_observer_list->AddObserver(&mock_observer);

  SetLoadingScenarioForProcess(LoadingScenario::kVisiblePageLoading, process());
  EXPECT_EQ(GetLoadingScenario(ScenarioScope::kCurrentProcess)
                ->load(std::memory_order_relaxed),
            LoadingScenario::kNoPageLoading);

  process_observer_list->RemoveObserver(&mock_observer);
  global_observer_list->RemoveObserver(&mock_observer);
}

}  // namespace

}  // namespace performance_manager
