// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/resource_attribution/query_scheduler.h"

#include <memory>
#include <utility>

#include "base/containers/enum_set.h"
#include "base/dcheck_is_on.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/performance_manager/embedder/graph_features.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/performance_manager/public/resource_attribution/cpu_measurement_delegate.h"
#include "components/performance_manager/public/resource_attribution/process_context.h"
#include "components/performance_manager/public/resource_attribution/query_results.h"
#include "components/performance_manager/public/resource_attribution/resource_types.h"
#include "components/performance_manager/public/resource_attribution/scoped_cpu_query.h"
#include "components/performance_manager/resource_attribution/cpu_measurement_monitor.h"
#include "components/performance_manager/resource_attribution/query_params.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/test_support/mock_graphs.h"
#include "components/performance_manager/test_support/performance_manager_test_harness.h"
#include "components/performance_manager/test_support/resource_attribution/simulated_cpu_measurement_delegate.h"
#include "components/performance_manager/test_support/run_in_graph.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager::resource_attribution {

namespace {

using ::testing::Contains;
using ::testing::Key;
using QueryParams = internal::QueryParams;

std::unique_ptr<QueryParams> CreateQueryParams(ResourceTypeSet resource_types) {
  auto params = std::make_unique<QueryParams>();
  params->resource_types = std::move(resource_types);
  return params;
}

// Waits for a result from `query` and tests that it matches `matcher`.
void ExpectQueryResult(ScopedCPUQuery* query, auto matcher) {
  base::RunLoop run_loop;
  query->QueryOnce(
      base::BindLambdaForTesting([&](const QueryResultMap& query_results) {
        EXPECT_THAT(query_results, matcher);
      }).Then(run_loop.QuitClosure()));
  run_loop.Run();
}

}  // namespace

class QuerySchedulerTest : public GraphTestHarness {
 protected:
  using Super = GraphTestHarness;

  void SetUp() override {
    GetGraphFeatures().EnableResourceAttributionScheduler();
    Super::SetUp();
    CPUMeasurementDelegate::SetDelegateFactoryForTesting(graph(),
                                                         &delegate_factory_);
  }

  // This must be deleted after TearDown() so that it outlives the
  // CPUMeasurementMonitor.
  SimulatedCPUMeasurementDelegateFactory delegate_factory_;
};

using QuerySchedulerPMTest = PerformanceManagerTestHarness;

TEST_F(QuerySchedulerTest, CPUQueries) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());

  auto* scheduler = QueryScheduler::GetFromGraph(graph());
  ASSERT_TRUE(scheduler);

  EXPECT_FALSE(scheduler->GetCPUMonitorForTesting().IsMonitoring());
  auto cpu_query1 = std::make_unique<ScopedCPUQuery>(graph());

  // First query created should start CPU monitoring.
  EXPECT_TRUE(scheduler->GetCPUMonitorForTesting().IsMonitoring());

  // Allow some time to pass to measure.
  task_env().FastForwardBy(base::Minutes(1));
  ExpectQueryResult(cpu_query1.get(),
                    Contains(Key(mock_graph.process->GetResourceContext())));

  // CPU monitoring should not stop until the last query is deleted.
  auto cpu_query2 = std::make_unique<ScopedCPUQuery>(graph());
  cpu_query1.reset();
  EXPECT_TRUE(scheduler->GetCPUMonitorForTesting().IsMonitoring());
  ExpectQueryResult(cpu_query2.get(),
                    Contains(Key(mock_graph.process->GetResourceContext())));

  cpu_query2.reset();
  EXPECT_FALSE(scheduler->GetCPUMonitorForTesting().IsMonitoring());
}

TEST_F(QuerySchedulerTest, ScopedQueries) {
  auto* scheduler = QueryScheduler::GetFromGraph(graph());
  ASSERT_TRUE(scheduler);

  // Query without kCPUTime should not start CPU monitoring.
  auto no_cpu_params = CreateQueryParams({});
  scheduler->AddScopedQuery(no_cpu_params.get());
  EXPECT_FALSE(scheduler->GetCPUMonitorForTesting().IsMonitoring());

  // First kCPUTime query should start monitoring.
  auto cpu_params1 = CreateQueryParams({ResourceType::kCPUTime});
  scheduler->AddScopedQuery(cpu_params1.get());
  EXPECT_TRUE(scheduler->GetCPUMonitorForTesting().IsMonitoring());

  // Removing non-CPU query should not affect CPU monitoring.
  scheduler->RemoveScopedQuery(std::move(no_cpu_params));
  EXPECT_TRUE(scheduler->GetCPUMonitorForTesting().IsMonitoring());

  // CPU monitoring should not stop until the last CPU query is deleted.
  auto cpu_params2 = CreateQueryParams({ResourceType::kCPUTime});
  scheduler->AddScopedQuery(cpu_params2.get());
  scheduler->RemoveScopedQuery(std::move(cpu_params1));
  EXPECT_TRUE(scheduler->GetCPUMonitorForTesting().IsMonitoring());
  scheduler->RemoveScopedQuery(std::move(cpu_params2));
  EXPECT_FALSE(scheduler->GetCPUMonitorForTesting().IsMonitoring());
}

TEST_F(QuerySchedulerTest, GraphTeardown) {
  // Make sure queries that still exist when the scheduler is deleted during
  // graph teardown safely return no data.
  auto* scheduler = QueryScheduler::GetFromGraph(graph());
  ASSERT_TRUE(scheduler);
  auto weak_scheduler = scheduler->GetWeakPtr();

  ScopedCPUQuery query(graph());

  TearDownAndDestroyGraph();

  EXPECT_FALSE(weak_scheduler);

  base::RunLoop run_loop;
  query.QueryOnce(base::BindOnce(
      [](base::ScopedClosureRunner closure_runner, const QueryResultMap&) {
        // The result callback should never run since the scheduler is
        // unavailable. The ScopedClosureRunner will run when the result
        // callback is deleted with all its bound parameters though.
        FAIL();
      },
      base::ScopedClosureRunner(run_loop.QuitClosure())));
}

TEST_F(QuerySchedulerPMTest, CallOnGraphWithScheduler) {
  QueryScheduler* scheduler_ptr = nullptr;
  Graph* graph_ptr = nullptr;
  RunInGraph([&](Graph* graph) {
    auto scheduler = std::make_unique<QueryScheduler>();
    scheduler_ptr = scheduler.get();
    graph_ptr = graph;
    graph->PassToGraph(std::move(scheduler));
  });
  ASSERT_TRUE(scheduler_ptr);
  ASSERT_TRUE(graph_ptr);
  base::RunLoop run_loop;
  QueryScheduler::CallOnGraphWithScheduler(
      base::BindLambdaForTesting([&](QueryScheduler* scheduler) {
#if DCHECK_IS_ON()
        EXPECT_TRUE(graph_ptr->IsOnGraphSequence());
#endif
        EXPECT_EQ(scheduler, scheduler_ptr);
      }).Then(run_loop.QuitClosure()));
  run_loop.Run();
}

}  // namespace performance_manager::resource_attribution
