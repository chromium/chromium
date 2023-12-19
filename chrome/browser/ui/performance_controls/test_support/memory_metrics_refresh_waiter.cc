// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/performance_controls/test_support/memory_metrics_refresh_waiter.h"

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "components/performance_manager/public/performance_manager.h"

MemoryMetricsRefreshWaiter::MemoryMetricsRefreshWaiter() = default;
MemoryMetricsRefreshWaiter::~MemoryMetricsRefreshWaiter() = default;

void MemoryMetricsRefreshWaiter::OnMemoryMetricsRefreshed() {
  std::move(quit_closure_).Run();
}

// Forces and waits for the memory metrics to refresh
void MemoryMetricsRefreshWaiter::Wait() {
  auto* const manager = performance_manager::user_tuning::
      UserPerformanceTuningManager::GetInstance();
  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  quit_closure_ = run_loop.QuitClosure();
  base::ScopedObservation<
      performance_manager::user_tuning::UserPerformanceTuningManager,
      MemoryMetricsRefreshWaiter>
      memory_metrics_observer(this);
  memory_metrics_observer.Observe(manager);
  performance_manager::PerformanceManager::CallOnGraph(
      FROM_HERE,
      base::BindLambdaForTesting([](performance_manager::Graph* graph) {
        auto* metrics_decorator = graph->GetRegisteredObjectAs<
            performance_manager::ProcessMetricsDecorator>();
        metrics_decorator->RequestImmediateMetrics();
      }));
  run_loop.Run();
}
