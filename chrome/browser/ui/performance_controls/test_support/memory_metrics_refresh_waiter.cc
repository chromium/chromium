// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/performance_controls/test_support/memory_metrics_refresh_waiter.h"

#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/test/bind.h"
#include "chrome/browser/ui/performance_controls/tab_resource_usage_collector.h"
#include "components/performance_manager/public/decorators/process_metrics_decorator.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/performance_manager.h"

// Forces and waits for the memory metrics to refresh
void MemoryMetricsRefreshWaiter::Wait() {
  // kNestableTasksAllowed is used to prevent kombucha interactive tests from
  // hanging
  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  base::OnceClosure quit_closure = run_loop.QuitClosure();
  performance_manager::PerformanceManager::CallOnGraph(
      FROM_HERE,
      base::BindLambdaForTesting([&](performance_manager::Graph* graph) {
        auto* const metrics_decorator = graph->GetRegisteredObjectAs<
            performance_manager::ProcessMetricsDecorator>();
        metrics_decorator->RequestImmediateMetrics(std::move(quit_closure));
      }));
  run_loop.Run();
}

TabResourceUsageRefreshWaiter::TabResourceUsageRefreshWaiter()
    : ResourceUsageCollectorObserver(base::DoNothing()) {}
TabResourceUsageRefreshWaiter::~TabResourceUsageRefreshWaiter() = default;

// Forces and waits for the memory metrics to refresh
void TabResourceUsageRefreshWaiter::Wait() {
  // kNestableTasksAllowed is used to prevent kombucha interactive tests from
  // hanging
  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  metrics_refresh_callback_ = run_loop.QuitClosure();
  TabResourceUsageCollector::Get()->ImmediatelyRefreshMetricsForAllTabs();
  run_loop.Run();
}
