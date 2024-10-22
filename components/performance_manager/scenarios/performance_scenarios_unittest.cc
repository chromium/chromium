// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/scenarios/performance_scenarios.h"

#include <atomic>
#include <utility>

#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/weak_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/test_support/performance_manager_test_harness.h"
#include "components/performance_manager/test_support/run_in_graph.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/navigation_simulator.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/performance/performance_scenarios.h"
#include "url/gurl.h"

namespace performance_manager {

namespace {

using blink::performance_scenarios::GetLoadingScenario;
using blink::performance_scenarios::Scope;

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
    PerformanceManagerTestHarness::SetUp();

    // Load a page with FrameNodes and ProcessNodes.
    SetContents(CreateTestWebContents());
    content::NavigationSimulator::NavigateAndCommitFromBrowser(
        web_contents(), GURL("https://www.example.com"));
  }

  // Returns the shared memory region handle for the main frame's process, or an
  // invalid handle if there s none.
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

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All, PerformanceScenariosTest, ::testing::Bool());

TEST_P(PerformanceScenariosTest, SetWithoutSharedMemory) {
  // When the shared scenario memory isn't set up, setting a scenario should
  // silently do nothing.
  SetGlobalLoadingScenario(LoadingScenario::kFocusedPageLoading);
  EXPECT_EQ(GetLoadingScenario(Scope::kGlobal)->load(std::memory_order_relaxed),
            LoadingScenario::kNoPageLoading);

  SetLoadingScenarioForProcess(LoadingScenario::kVisiblePageLoading, process());
  EXPECT_EQ(GetLoadingScenario(Scope::kCurrentProcess)
                ->load(std::memory_order_relaxed),
            LoadingScenario::kNoPageLoading);
}

TEST_P(PerformanceScenariosTest, SetWithSharedMemory) {
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
  // Create writable shared memory for the global state. This maps a read-only
  // view of the memory in as well.
  ScopedGlobalScenarioMemory global_shared_memory;

  // Create writable shared memory for a render process state. Since this is
  // called in the browser process and the state is for a different process, it
  // doesn't map in a read-only view.
  base::ReadOnlySharedMemoryRegion process_region = main_process_region();
  EXPECT_TRUE(process_region.IsValid());

  // Set the loading scenario from the PM sequence.
  base::WeakPtr<ProcessNode> process_node =
      PerformanceManager::GetProcessNodeForRenderProcessHost(process());
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

  // Map in the read-only view of `process_region`. Normally this would be done
  // in the renderer process as the "current process" state. The state should
  // now become visible.
  blink::performance_scenarios::ScopedReadOnlyScenarioMemory
      process_shared_memory(Scope::kCurrentProcess, std::move(process_region));
  EXPECT_EQ(GetLoadingScenario(Scope::kCurrentProcess)
                ->load(std::memory_order_relaxed),
            LoadingScenario::kVisiblePageLoading);
}

}  // namespace

}  // namespace performance_manager
