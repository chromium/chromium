// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/scenarios/performance_scenarios.h"

#include <atomic>
#include <utility>

#include "base/functional/callback.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/weak_ptr.h"
#include "base/task/bind_post_task.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/test_support/performance_manager_test_harness.h"
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

using PerformanceScenariosTest = PerformanceManagerTestHarness;

TEST_F(PerformanceScenariosTest, SetWithoutSharedMemory) {
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

TEST_F(PerformanceScenariosTest, SetWithSharedMemory) {
  // Create writable shared memory for the global state. This maps a read-only
  // view of the memory in as well so changes immediately become visible to the
  // current (browser) process.
  ScopedGlobalScenarioMemory global_shared_memory;
  SetGlobalLoadingScenario(LoadingScenario::kFocusedPageLoading);
  EXPECT_EQ(GetLoadingScenario(Scope::kGlobal)->load(std::memory_order_relaxed),
            LoadingScenario::kFocusedPageLoading);

  // GetPerformanceScenarioRegionForProcess() creates writable shared memory for
  // that process' state. Since it's called in the browser process and this
  // state is for a different process, it doesn't map in a read-only view.
  base::ReadOnlySharedMemoryRegion process_region =
      GetSharedScenarioRegionForProcess(process());
  SetLoadingScenarioForProcess(LoadingScenario::kVisiblePageLoading, process());
  EXPECT_EQ(GetLoadingScenario(Scope::kCurrentProcess)
                ->load(std::memory_order_relaxed),
            LoadingScenario::kNoPageLoading);

  // Map in the read-only view of `process_region`. Normally this would be done
  // in the renderer process as the "current process" state. The state should
  // now become visible.
  blink::performance_scenarios::ScopedReadOnlyScenarioMemory
      process_shared_memory(Scope::kCurrentProcess, std::move(process_region));
  EXPECT_EQ(GetLoadingScenario(Scope::kCurrentProcess)
                ->load(std::memory_order_relaxed),
            LoadingScenario::kVisiblePageLoading);
}

TEST_F(PerformanceScenariosTest, SetFromPMSequence) {
  // Load a page with FrameNodes and ProcessNodes.
  SetContents(CreateTestWebContents());
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://www.example.com"));

  // Create writable shared memory for the global state. This maps a read-only
  // view of the memory in as well.
  ScopedGlobalScenarioMemory global_shared_memory;

  // Create writable shared memory for the renderer process, and map the
  // read-only view as the "current process" state.
  ASSERT_TRUE(process());
  blink::performance_scenarios::ScopedReadOnlyScenarioMemory
      process_shared_memory(Scope::kCurrentProcess,
                            GetSharedScenarioRegionForProcess(process()));
  EXPECT_EQ(GetLoadingScenario(Scope::kCurrentProcess)
                ->load(std::memory_order_relaxed),
            LoadingScenario::kNoPageLoading);

  // Set the loading scenario from the PM sequence.
  base::WeakPtr<ProcessNode> process_node =
      PerformanceManager::GetProcessNodeForRenderProcessHost(process());
  auto set_on_pm_sequence = base::BindLambdaForTesting([&] {
    ASSERT_TRUE(process_node);
    SetLoadingScenarioForProcessNode(LoadingScenario::kVisiblePageLoading,
                                     process_node.get());
    SetGlobalLoadingScenario(LoadingScenario::kFocusedPageLoading);
  });

  // SetLoadingScenarioForProcessNode posts to the main thread. Wait until the
  // message is received to quit the run loop.
  auto quit_run_loop_on_main_thread =
      base::BindPostTaskToCurrentDefault(task_environment()->QuitClosure());

  PerformanceManager::CallOnGraph(
      FROM_HERE, std::move(set_on_pm_sequence)
                     .Then(std::move(quit_run_loop_on_main_thread)));
  task_environment()->RunUntilQuit();

  EXPECT_EQ(GetLoadingScenario(Scope::kCurrentProcess)
                ->load(std::memory_order_relaxed),
            LoadingScenario::kVisiblePageLoading);
  EXPECT_EQ(GetLoadingScenario(Scope::kGlobal)->load(std::memory_order_relaxed),
            LoadingScenario::kFocusedPageLoading);
}

}  // namespace

}  // namespace performance_manager
