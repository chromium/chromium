// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/resource_attribution/query_scheduler.h"

#include <bitset>
#include <memory>
#include <set>
#include <utility>

#include "base/containers/enum_set.h"
#include "base/dcheck_is_on.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/types/variant_util.h"
#include "components/performance_manager/embedder/graph_features.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/public/resource_attribution/cpu_measurement_delegate.h"
#include "components/performance_manager/public/resource_attribution/queries.h"
#include "components/performance_manager/public/resource_attribution/query_results.h"
#include "components/performance_manager/public/resource_attribution/resource_contexts.h"
#include "components/performance_manager/public/resource_attribution/resource_types.h"
#include "components/performance_manager/public/resource_attribution/scoped_cpu_query.h"
#include "components/performance_manager/resource_attribution/cpu_measurement_monitor.h"
#include "components/performance_manager/resource_attribution/query_params.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/test_support/mock_graphs.h"
#include "components/performance_manager/test_support/performance_manager_test_harness.h"
#include "components/performance_manager/test_support/resource_attribution/gtest_util.h"
#include "components/performance_manager/test_support/resource_attribution/simulated_cpu_measurement_delegate.h"
#include "components/performance_manager/test_support/run_in_graph.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager::resource_attribution {

namespace {

using ::testing::UnorderedElementsAre;
using QueryParams = internal::QueryParams;

// A QueryResultObserver that passes the result to a callback.
class CallbackQueryResultObserver final : public QueryResultObserver {
 public:
  CallbackQueryResultObserver() = default;
  ~CallbackQueryResultObserver() final = default;

  CallbackQueryResultObserver(const CallbackQueryResultObserver&) = delete;
  CallbackQueryResultObserver operator=(const CallbackQueryResultObserver&) =
      delete;

  void SetCallback(base::OnceCallback<void(const QueryResultMap&)> callback) {
    callback_ = std::move(callback);
  }

  // QueryResultObserver:
  void OnResourceUsageUpdated(const QueryResultMap& results) final {
    if (callback_) {
      std::move(callback_).Run(results);
    }
  }

 private:
  base::OnceCallback<void(const QueryResultMap&)> callback_;
};

std::unique_ptr<QueryParams> CreateQueryParams(
    ResourceTypeSet resource_types = {},
    std::set<ResourceContext> resource_contexts = {}) {
  auto params = std::make_unique<QueryParams>();
  params->resource_types = std::move(resource_types);
  params->resource_contexts = std::move(resource_contexts);
  return params;
}

// Waits for a result from `query` and tests that it matches `matcher`.
void ExpectQueryResult(QueryScheduler* scheduler,
                       QueryParams* query,
                       auto matcher) {
  base::RunLoop run_loop;
  scheduler->RequestResults(
      *query,
      base::BindLambdaForTesting([&](const QueryResultMap& query_results) {
        EXPECT_THAT(query_results, matcher);
      }).Then(run_loop.QuitClosure()));
  run_loop.Run();
}

}  // namespace

class ResourceAttrQuerySchedulerTest : public GraphTestHarness {
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

using ResourceAttrQuerySchedulerPMTest = PerformanceManagerTestHarness;

TEST_F(ResourceAttrQuerySchedulerTest, CPUQueries) {
  MockMultiplePagesWithMultipleProcessesGraph mock_graph(graph());

  auto* scheduler = QueryScheduler::GetFromGraph(graph());
  ASSERT_TRUE(scheduler);

  EXPECT_FALSE(scheduler->GetCPUMonitorForTesting().IsMonitoring());

  // Query without kCPUTime should not start CPU monitoring.
  auto no_cpu_query =
      CreateQueryParams({}, {mock_graph.process->GetResourceContext()});
  scheduler->AddScopedQuery(no_cpu_query.get());
  EXPECT_FALSE(scheduler->GetCPUMonitorForTesting().IsMonitoring());

  // First kCPUTime query should start CPU monitoring.
  auto cpu_query1 = CreateQueryParams(
      {ResourceType::kCPUTime}, {mock_graph.process->GetResourceContext()});
  scheduler->AddScopedQuery(cpu_query1.get());
  EXPECT_TRUE(scheduler->GetCPUMonitorForTesting().IsMonitoring());

  auto cpu_query2 = CreateQueryParams({ResourceType::kCPUTime}, {});
  cpu_query2->all_context_types.set(
      base::VariantIndexOfType<ResourceContext, ProcessContext>());
  scheduler->AddScopedQuery(cpu_query2.get());

  // Allow some time to pass to measure.
  task_env().FastForwardBy(base::Minutes(1));

  // Only the kCPUTime queries should receive CPU results. `cpu_query1` should
  // only get results for `process`.
  ExpectQueryResult(scheduler, no_cpu_query.get(),
                    UnorderedElementsAre(/*none*/));
  ExpectQueryResult(
      scheduler, cpu_query1.get(),
      UnorderedElementsAre(QueryResultMapEntryMatches<CPUTimeResult>(
          mock_graph.process->GetResourceContext())));
  ExpectQueryResult(scheduler, cpu_query2.get(),
                    UnorderedElementsAre(
                        QueryResultMapEntryMatches<CPUTimeResult>(
                            mock_graph.process->GetResourceContext()),
                        QueryResultMapEntryMatches<CPUTimeResult>(
                            mock_graph.other_process->GetResourceContext())));

  // Removing non-CPU query should not affect CPU monitoring.
  scheduler->RemoveScopedQuery(std::move(no_cpu_query));
  EXPECT_TRUE(scheduler->GetCPUMonitorForTesting().IsMonitoring());

  // CPU monitoring should not stop until the last query is deleted.
  scheduler->RemoveScopedQuery(std::move(cpu_query1));
  EXPECT_TRUE(scheduler->GetCPUMonitorForTesting().IsMonitoring());
  scheduler->RemoveScopedQuery(std::move(cpu_query2));
  EXPECT_FALSE(scheduler->GetCPUMonitorForTesting().IsMonitoring());
}

TEST_F(ResourceAttrQuerySchedulerPMTest, CallWithScheduler) {
  // Tests that CallWithScheduler works from PerformanceManagerTestHarness,
  // where the scheduler runs on the PM sequence as in production.
  EXPECT_TRUE(PerformanceManager::IsAvailable());
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
  QueryScheduler::CallWithScheduler(
      base::BindLambdaForTesting([&](QueryScheduler* scheduler) {
#if DCHECK_IS_ON()
        EXPECT_TRUE(graph_ptr->IsOnGraphSequence());
#endif
        EXPECT_EQ(scheduler, scheduler_ptr);
      }).Then(run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(ResourceAttrQuerySchedulerTest, CallWithScheduler) {
  // Tests that CallWithScheduler works from GraphTestHarness which doesn't set
  // up the PerformanceManager sequence. It's convenient to use GraphTestHarness
  // with mock graphs to test resource attribution queries.
  EXPECT_FALSE(PerformanceManager::IsAvailable());
  base::RunLoop run_loop;
  QueryScheduler::CallWithScheduler(
      base::BindLambdaForTesting([&](QueryScheduler* scheduler) {
        // The QueryScheduler was installed on the graph in SetUp().
        EXPECT_EQ(scheduler, graph()->GetRegisteredObjectAs<QueryScheduler>());
      }).Then(run_loop.QuitClosure()));
  run_loop.Run();
}

}  // namespace performance_manager::resource_attribution
