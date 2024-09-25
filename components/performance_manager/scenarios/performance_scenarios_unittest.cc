// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/scenarios/performance_scenarios.h"

#include <atomic>
#include <string>

#include "base/location.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/performance/performance_scenarios.h"

namespace performance_manager {

namespace {

using blink::performance_scenarios::GetLoadingScenario;
using blink::performance_scenarios::Scope;
using PerformanceScenariosTest = content::RenderViewHostTestHarness;

TEST_F(PerformanceScenariosTest, SetWithoutSharedMemory) {
  // When the shared scenario memory isn't set up, setting a scenario should
  // silently do nothing.
  SetGlobalLoadingScenario(LoadingScenario::kFocusedPageLoading);
  EXPECT_EQ(GetLoadingScenario(Scope::kGlobal)->load(std::memory_order_relaxed),
            LoadingScenario::kNoPageLoading);

  SetLoadingScenarioForProcess(*process(),
                               LoadingScenario::kVisiblePageLoading);
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
      GetSharedScenarioRegionForProcess(*process());
  SetLoadingScenarioForProcess(*process(),
                               LoadingScenario::kVisiblePageLoading);
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

}  // namespace

}  // namespace performance_manager
