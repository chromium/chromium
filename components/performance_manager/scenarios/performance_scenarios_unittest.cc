// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/scenarios/performance_scenarios.h"

#include <atomic>
#include <utility>

#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/weak_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "components/performance_manager/embedder/graph_features.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/public/scenarios/performance_scenario_observer.h"
#include "components/performance_manager/test_support/performance_manager_test_harness.h"
#include "components/performance_manager/test_support/run_in_graph.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/navigation_simulator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/performance/performance_scenarios.h"
#include "url/gurl.h"

namespace performance_manager {

namespace {

using blink::performance_scenarios::GetLoadingScenario;
using blink::performance_scenarios::Scope;
using ::testing::_;

class MockPerformanceScenarioObserver : public PerformanceScenarioObserver {
 public:
  MOCK_METHOD(void,
              OnGlobalLoadingScenarioChanged,
              (LoadingScenario, LoadingScenario),
              (override));
  MOCK_METHOD(void,
              OnGlobalInputScenarioChanged,
              (InputScenario, InputScenario),
              (override));
  MOCK_METHOD(void,
              OnProcessLoadingScenarioChanged,
              (const ProcessNode*, LoadingScenario, LoadingScenario),
              (override));
  MOCK_METHOD(void,
              OnProcessInputScenarioChanged,
              (const ProcessNode*, InputScenario, InputScenario),
              (override));
};

class PerformanceScenariosTest : public PerformanceManagerTestHarness,
                                 public ::testing::WithParamInterface<bool> {
 public:
  PerformanceScenariosTest() {
    // Run with and without PM on main thread, since this can affect whether
    // setting a scenario needs to post a task.
    scoped_feature_list_.InitWithFeatureState(features::kRunOnMainThreadSync,
                                              GetParam());
  }

  void SetUp() override {
    // Enable the PerformanceScenarioNotifier.
    GetGraphFeatures().EnablePerformanceScenarios();

    PerformanceManagerTestHarness::SetUp();

    // Load a page with FrameNodes and ProcessNodes.
    SetContents(CreateTestWebContents());
    content::NavigationSimulator::NavigateAndCommitFromBrowser(
        web_contents(), GURL("https://www.example.com"));

    // Observe global scenario changes and scenarios for two ProcessNodes.
    // (The browser process is a convenient 2nd process.)
    base::WeakPtr<ProcessNode> process_node =
        PerformanceManager::GetProcessNodeForRenderProcessHost(process());
    base::WeakPtr<ProcessNode> browser_process_node =
        PerformanceManager::GetProcessNodeForBrowserProcess();
    RunInGraph([&](Graph* graph) {
      auto* notifier = PerformanceScenarioNotifier::GetFromGraph(graph);
      ASSERT_TRUE(notifier);
      ASSERT_TRUE(process_node);
      ASSERT_TRUE(browser_process_node);
      notifier->AddGlobalObserver(&mock_observer_);
      notifier->AddObserverForProcess(process_node.get(), &mock_observer_);

      // This observer should never fire (verified by StrictMock).
      notifier->AddObserverForProcess(browser_process_node.get(),
                                      &mock_observer_);
    });
  }

  // Returns the shared memory region handle for the main frame's process, or an
  // invalid handle if there is none.
  base::ReadOnlySharedMemoryRegion main_process_region() {
    if (!process()) {
      return base::ReadOnlySharedMemoryRegion();
    }
    base::WeakPtr<ProcessNode> process_node =
        PerformanceManager::GetProcessNodeForRenderProcessHost(process());
    base::ReadOnlySharedMemoryRegion process_region;
    RunInGraph([&] {
      ASSERT_TRUE(process_node);
      // GetPerformanceScenarioRegionForProcess() creates writable shared memory
      // for that process' state if it doesn't already exist.
      process_region =
          GetSharedScenarioRegionForProcessNode(process_node.get());
    });
    return process_region;
  }

 protected:
  ::testing::StrictMock<MockPerformanceScenarioObserver> mock_observer_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All, PerformanceScenariosTest, ::testing::Bool());

TEST_P(PerformanceScenariosTest, SetWithoutSharedMemory) {
  // When the global shared scenario memory isn't set up, setting a scenario
  // should silently do nothing. (Process scenario memory is scoped to the
  // ProcessNode so will always be mapped as needed.)
  SetGlobalLoadingScenario(LoadingScenario::kFocusedPageLoading);
  EXPECT_EQ(GetLoadingScenario(Scope::kGlobal)->load(std::memory_order_relaxed),
            LoadingScenario::kNoPageLoading);
}

TEST_P(PerformanceScenariosTest, SetWithSharedMemory) {
  base::WeakPtr<ProcessNode> process_node =
      PerformanceManager::GetProcessNodeForRenderProcessHost(process());
  RunInGraph([&] {
    ASSERT_TRUE(process_node);
    EXPECT_CALL(mock_observer_, OnGlobalLoadingScenarioChanged(
                                    LoadingScenario::kNoPageLoading,
                                    LoadingScenario::kFocusedPageLoading));
    EXPECT_CALL(mock_observer_,
                OnProcessLoadingScenarioChanged(
                    process_node.get(), LoadingScenario::kNoPageLoading,
                    LoadingScenario::kVisiblePageLoading));
  });

  // Create writable shared memory for the global state. This maps a read-only
  // view of the memory in as well so changes immediately become visible to the
  // current (browser) process.
  ScopedGlobalScenarioMemory global_shared_memory;
  SetGlobalLoadingScenario(LoadingScenario::kFocusedPageLoading);
  EXPECT_EQ(GetLoadingScenario(Scope::kGlobal)->load(std::memory_order_relaxed),
            LoadingScenario::kFocusedPageLoading);

  // Create writable shared memory for a render process state. Since this is
  // called in the browser process and the state is for a different process, it
  // doesn't map in a read-only view.
  base::ReadOnlySharedMemoryRegion process_region = main_process_region();
  EXPECT_TRUE(process_region.IsValid());
  SetLoadingScenarioForProcess(LoadingScenario::kVisiblePageLoading, process());

  // SetLoadingScenarioForProcess posts to the PM thread. Wait until the message
  // is received before reading.
  RunInGraph([] {
    EXPECT_EQ(GetLoadingScenario(Scope::kCurrentProcess)
                  ->load(std::memory_order_relaxed),
              LoadingScenario::kNoPageLoading);
  });

  // Ensure that the ProcessNode observer was notified in the browser process,
  // even though the child hasn't mapped in the memory yet.
  ::testing::Mock::VerifyAndClearExpectations(&mock_observer_);

  // Map in the read-only view of `process_region`. Normally this would be done
  // in the renderer process as the "current process" state. The state should
  // now become visible.
  blink::performance_scenarios::ScopedReadOnlyScenarioMemory
      process_shared_memory(Scope::kCurrentProcess, std::move(process_region));
  EXPECT_EQ(GetLoadingScenario(Scope::kCurrentProcess)
                ->load(std::memory_order_relaxed),
            LoadingScenario::kVisiblePageLoading);
}

TEST_P(PerformanceScenariosTest, SetFromPMSequence) {
  base::WeakPtr<ProcessNode> process_node =
      PerformanceManager::GetProcessNodeForRenderProcessHost(process());
  RunInGraph([&] {
    ASSERT_TRUE(process_node);
    EXPECT_CALL(mock_observer_, OnGlobalLoadingScenarioChanged(
                                    LoadingScenario::kNoPageLoading,
                                    LoadingScenario::kFocusedPageLoading));
    EXPECT_CALL(mock_observer_,
                OnProcessLoadingScenarioChanged(
                    process_node.get(), LoadingScenario::kNoPageLoading,
                    LoadingScenario::kVisiblePageLoading));
  });

  // Create writable shared memory for the global state. This maps a read-only
  // view of the memory in as well.
  ScopedGlobalScenarioMemory global_shared_memory;

  // Create writable shared memory for a render process state. Since this is
  // called in the browser process and the state is for a different process, it
  // doesn't map in a read-only view.
  base::ReadOnlySharedMemoryRegion process_region = main_process_region();
  EXPECT_TRUE(process_region.IsValid());

  // Set the loading scenario from the PM sequence.
  RunInGraph([&] {
    ASSERT_TRUE(process_node);
    SetLoadingScenarioForProcessNode(LoadingScenario::kVisiblePageLoading,
                                     process_node.get());
    SetGlobalLoadingScenario(LoadingScenario::kFocusedPageLoading);
  });

  EXPECT_EQ(GetLoadingScenario(Scope::kCurrentProcess)
                ->load(std::memory_order_relaxed),
            LoadingScenario::kNoPageLoading);
  EXPECT_EQ(GetLoadingScenario(Scope::kGlobal)->load(std::memory_order_relaxed),
            LoadingScenario::kFocusedPageLoading);

  // Ensure that the ProcessNode observer was notified in the browser process,
  // even though the child hasn't mapped in the memory yet.
  ::testing::Mock::VerifyAndClearExpectations(&mock_observer_);

  // Map in the read-only view of `process_region`. Normally this would be done
  // in the renderer process as the "current process" state. The state should
  // now become visible.
  blink::performance_scenarios::ScopedReadOnlyScenarioMemory
      process_shared_memory(Scope::kCurrentProcess, std::move(process_region));
  EXPECT_EQ(GetLoadingScenario(Scope::kCurrentProcess)
                ->load(std::memory_order_relaxed),
            LoadingScenario::kVisiblePageLoading);
}

TEST_P(PerformanceScenariosTest, SetWithoutObservers) {
  // Stop observing scenarios. StrictMock will complain if any observer method
  // is called.
  base::WeakPtr<ProcessNode> process_node =
      PerformanceManager::GetProcessNodeForRenderProcessHost(process());
  RunInGraph([&](Graph* graph) {
    auto* notifier = PerformanceScenarioNotifier::GetFromGraph(graph);
    ASSERT_TRUE(notifier);
    ASSERT_TRUE(process_node);
    notifier->RemoveGlobalObserver(&mock_observer_);
    notifier->RemoveObserverForProcess(process_node.get(), &mock_observer_);
  });

  ScopedGlobalScenarioMemory global_shared_memory;
  blink::performance_scenarios::ScopedReadOnlyScenarioMemory
      process_shared_memory(Scope::kCurrentProcess, main_process_region());

  SetGlobalLoadingScenario(LoadingScenario::kFocusedPageLoading);
  SetLoadingScenarioForProcess(LoadingScenario::kVisiblePageLoading, process());

  // SetLoadingScenarioForProcess posts to the PM thread. Wait until the message
  // is received before reading.
  RunInGraph([] {
    EXPECT_EQ(
        GetLoadingScenario(Scope::kGlobal)->load(std::memory_order_relaxed),
        LoadingScenario::kFocusedPageLoading);
    EXPECT_EQ(GetLoadingScenario(Scope::kCurrentProcess)
                  ->load(std::memory_order_relaxed),
              LoadingScenario::kVisiblePageLoading);
  });
}

}  // namespace

}  // namespace performance_manager
