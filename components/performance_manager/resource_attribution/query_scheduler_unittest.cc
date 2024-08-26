// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/resource_attribution/query_scheduler.h"

#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/containers/enum_set.h"
#include "base/dcheck_is_on.h"
#include "base/location.h"
#include "base/process/process.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/performance_manager/embedder/graph_features.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/graph/worker_node_impl.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/public/resource_attribution/cpu_measurement_delegate.h"
#include "components/performance_manager/public/resource_attribution/queries.h"
#include "components/performance_manager/public/resource_attribution/query_results.h"
#include "components/performance_manager/public/resource_attribution/resource_contexts.h"
#include "components/performance_manager/public/resource_attribution/resource_types.h"
#include "components/performance_manager/resource_attribution/context_collection.h"
#include "components/performance_manager/resource_attribution/cpu_measurement_monitor.h"
#include "components/performance_manager/resource_attribution/performance_manager_aliases.h"
#include "components/performance_manager/resource_attribution/query_params.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/test_support/mock_graphs.h"
#include "components/performance_manager/test_support/performance_manager_test_harness.h"
#include "components/performance_manager/test_support/resource_attribution/gtest_util.h"
#include "components/performance_manager/test_support/resource_attribution/measurement_delegates.h"
#include "components/performance_manager/test_support/run_in_graph.h"
#include "content/public/browser/browsing_instance_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace resource_attribution::internal {

namespace {

using performance_manager::features::kResourceAttributionIncludeOrigins;
using performance_manager::features::kRunOnMainThreadSync;
using ::testing::_;
using ::testing::Bool;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;
using ::testing::WithParamInterface;

std::unique_ptr<QueryParams> CreateQueryParams(
    ResourceTypeSet resource_types = {},
    std::set<ResourceContext> resource_contexts = {},
    std::set<ResourceContextTypeId> all_context_types = {}) {
  auto params = std::make_unique<QueryParams>();
  params->resource_types = std::move(resource_types);
  params->contexts = ContextCollection::CreateForTesting(
      std::move(resource_contexts), std::move(all_context_types));
  return params;
}

// Waits for a result from `query` and tests that it matches `matcher`.
void ExpectQueryResult(
    QueryScheduler* scheduler,
    QueryParams* query,
    auto matcher,
    const base::Location& location = base::Location::Current()) {
  base::RunLoop run_loop;
  scheduler->RequestResults(
      *query,
      base::BindLambdaForTesting([&](const QueryResultMap& query_results) {
        EXPECT_THAT(query_results, matcher) << location.ToString();
      }).Then(run_loop.QuitClosure()));
  run_loop.Run();
}

}  // namespace

class ResourceAttrQuerySchedulerTest
    : public performance_manager::GraphTestHarness,
      public WithParamInterface<bool> {
 protected:
  using Super = performance_manager::GraphTestHarness;

  ResourceAttrQuerySchedulerTest() {
    std::vector<base::test::FeatureRef> enabled_features{
        kResourceAttributionIncludeOrigins};
    if (GetParam()) {
      enabled_features.push_back(kRunOnMainThreadSync);
    }
    scoped_feature_list_.InitWithFeatures(enabled_features, {});
  }

  void SetUp() override {
    GetGraphFeatures().EnableResourceAttributionScheduler();
    Super::SetUp();
    CPUMeasurementDelegate::SetDelegateFactoryForTesting(
        graph(), &cpu_delegate_factory_);
    MemoryMeasurementDelegate::SetDelegateFactoryForTesting(
        graph(), &memory_delegate_factory_);
  }

  base::test::ScopedFeatureList scoped_feature_list_;

  // These must be deleted after TearDown() so that they outlive the
  // CPUMeasurementMonitor and MemoryMeasurementProvider.
  SimulatedCPUMeasurementDelegateFactory cpu_delegate_factory_;
  FakeMemoryMeasurementDelegateFactory memory_delegate_factory_;
};

INSTANTIATE_TEST_SUITE_P(All, ResourceAttrQuerySchedulerTest, Bool());

class ResourceAttrQuerySchedulerPMTest
    : public performance_manager::PerformanceManagerTestHarness,
      public WithParamInterface<bool> {
 protected:
  ResourceAttrQuerySchedulerPMTest() {
    scoped_feature_list_.InitWithFeatureState(kRunOnMainThreadSync, GetParam());
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All, ResourceAttrQuerySchedulerPMTest, Bool());

TEST_P(ResourceAttrQuerySchedulerTest, AddRemoveQueries) {
  performance_manager::MockMultiplePagesWithMultipleProcessesGraph mock_graph(
      graph());

  // Install fake memory results for all processes.
  for (const ProcessNode* node :
       {mock_graph.browser_process.get(), mock_graph.process.get(),
        mock_graph.other_process.get()}) {
    memory_delegate_factory_.memory_summaries()[node->GetResourceContext()] =
        MemoryMeasurementDelegate::MemorySummaryMeasurement{
            .resident_set_size_kb = 1,
            .private_footprint_kb = 2,
        };
  }

  auto* scheduler = QueryScheduler::GetFromGraph(graph());
  ASSERT_TRUE(scheduler);

  EXPECT_FALSE(scheduler->GetCPUMonitorForTesting().IsMonitoring());

  // Queries without kCPUTime should not start CPU monitoring.
  auto no_resource_query =
      CreateQueryParams({}, {mock_graph.process->GetResourceContext()});
  auto memory_query =
      CreateQueryParams({ResourceType::kMemorySummary},
                        {mock_graph.process->GetResourceContext()});
  scheduler->AddScopedQuery(no_resource_query.get());
  scheduler->AddScopedQuery(memory_query.get());
  EXPECT_FALSE(scheduler->GetCPUMonitorForTesting().IsMonitoring());

  // First kCPUTime query should start CPU monitoring.
  auto cpu_query = CreateQueryParams(
      {ResourceType::kCPUTime}, {mock_graph.process->GetResourceContext()});
  scheduler->AddScopedQuery(cpu_query.get());
  EXPECT_TRUE(scheduler->GetCPUMonitorForTesting().IsMonitoring());

  auto cpu_memory_query =
      CreateQueryParams({ResourceType::kCPUTime, ResourceType::kMemorySummary},
                        /*resource_contexts=*/{},
                        {ResourceContextTypeId::ForType<ProcessContext>()});
  scheduler->AddScopedQuery(cpu_memory_query.get());

  // Simulate a query that uses Start() to request a measurement every minute.
  // The other queries in this test simulate queries that call QueryOnce() after
  // a minute without calling Start().
  auto repeating_query = CreateQueryParams(
      {ResourceType::kCPUTime}, {mock_graph.process->GetResourceContext()});
  scheduler->AddScopedQuery(repeating_query.get());
  scheduler->StartRepeatingQuery(repeating_query.get());

  // Only the repeating query should have a QueryId.
  EXPECT_EQ(no_resource_query->GetIdForTesting(), std::nullopt);
  EXPECT_EQ(memory_query->GetIdForTesting(), std::nullopt);
  EXPECT_EQ(cpu_memory_query->GetIdForTesting(), std::nullopt);
  std::optional<QueryId> repeating_query_id =
      repeating_query->GetIdForTesting();
  ASSERT_TRUE(repeating_query_id.has_value());
  EXPECT_TRUE(scheduler->GetCPUMonitorForTesting().IsTrackingQueryForTesting(
      repeating_query_id.value()));

  // Allow some time to pass to measure.
  task_env().FastForwardBy(base::Minutes(1));

  // Only the kCPUTime queries should receive CPU results.
  ExpectQueryResult(scheduler, no_resource_query.get(), IsEmpty());
  ExpectQueryResult(scheduler, memory_query.get(),
                    ElementsAre(ResultForContextMatches<MemorySummaryResult>(
                        mock_graph.process->GetResourceContext(), _)));
  ExpectQueryResult(scheduler, cpu_query.get(),
                    ElementsAre(ResultForContextMatches<CPUTimeResult>(
                        mock_graph.process->GetResourceContext(), _)));
  ExpectQueryResult(
      scheduler, cpu_memory_query.get(),
      UnorderedElementsAre(
          ResultForContextMatchesAll<CPUTimeResult, MemorySummaryResult>(
              mock_graph.process->GetResourceContext(), _, _),
          ResultForContextMatchesAll<CPUTimeResult, MemorySummaryResult>(
              mock_graph.other_process->GetResourceContext(), _, _),
          ResultForContextMatchesAll<CPUTimeResult, MemorySummaryResult>(
              mock_graph.browser_process->GetResourceContext(), _, _)));
  ExpectQueryResult(scheduler, repeating_query.get(),
                    ElementsAre(ResultForContextMatches<CPUTimeResult>(
                        mock_graph.process->GetResourceContext(), _)));

  // Removing non-CPU query should not affect CPU monitoring.
  scheduler->RemoveScopedQuery(std::move(no_resource_query));
  EXPECT_TRUE(scheduler->GetCPUMonitorForTesting().IsMonitoring());

  // CPU monitoring should not stop until the last CPU query is deleted.
  scheduler->RemoveScopedQuery(std::move(repeating_query));
  EXPECT_FALSE(scheduler->GetCPUMonitorForTesting().IsTrackingQueryForTesting(
      repeating_query_id.value()));
  EXPECT_TRUE(scheduler->GetCPUMonitorForTesting().IsMonitoring());
  scheduler->RemoveScopedQuery(std::move(cpu_query));
  EXPECT_TRUE(scheduler->GetCPUMonitorForTesting().IsMonitoring());
  scheduler->RemoveScopedQuery(std::move(cpu_memory_query));
  EXPECT_FALSE(scheduler->GetCPUMonitorForTesting().IsMonitoring());
}

TEST_P(ResourceAttrQuerySchedulerTest, AddRemoveNodes) {
  auto* scheduler = QueryScheduler::GetFromGraph(graph());
  ASSERT_TRUE(scheduler);

  EXPECT_FALSE(scheduler->GetCPUMonitorForTesting().IsMonitoring());

  auto process1 = CreateRendererProcessNode();
  auto process2 = CreateRendererProcessNode();
  auto process3 = CreateRendererProcessNode();
  process1->SetProcess(base::Process::Current(), base::TimeTicks::Now());
  process2->SetProcess(base::Process::Current(), base::TimeTicks::Now());
  process3->SetProcess(base::Process::Current(), base::TimeTicks::Now());
  const auto process_context1 = process1->GetResourceContext();
  const auto process_context2 = process2->GetResourceContext();
  const auto process_context3 = process3->GetResourceContext();

  // Create a page with several origins, to validate that
  // OriginInBrowsingInstanceContext results are cleared along with the
  // PageContext.
  constexpr content::BrowsingInstanceId kBrowsingInstance =
      content::BrowsingInstanceId::FromUnsafeValue(1);
  auto page1 = CreateNode<PageNodeImpl>();
  const GURL kUrl1("https://a.com");
  const url::Origin kOrigin1 = url::Origin::Create(kUrl1);
  auto frame1 =
      CreateFrameNodeAutoId(process3.get(), page1.get(),
                            /*parent_frame_node=*/nullptr, kBrowsingInstance);
  frame1->OnNavigationCommitted(kUrl1, kOrigin1, /*same_document=*/false,
                                /*is_served_from_back_forward_cache=*/false);
  const GURL kUrl2("https://b.com");
  const url::Origin kOrigin2 = url::Origin::Create(kUrl2);
  auto frame2 =
      CreateFrameNodeAutoId(process3.get(), page1.get(),
                            /*parent_frame_node=*/nullptr, kBrowsingInstance);
  frame2->OnNavigationCommitted(kUrl2, kOrigin2, /*same_document=*/false,
                                /*is_served_from_back_forward_cache=*/false);

  const auto page_context1 = page1->GetResourceContext();
  const auto frame_context1 = frame1->GetResourceContext();
  const auto frame_context2 = frame2->GetResourceContext();
  const auto origin_in_page_context1 =
      OriginInBrowsingInstanceContext(kOrigin1, kBrowsingInstance);
  const auto origin_in_page_context2 =
      OriginInBrowsingInstanceContext(kOrigin2, kBrowsingInstance);

  // Also test that WorkerContexts are tracked correctly.
  auto worker1 = CreateNode<WorkerNodeImpl>(WorkerNode::WorkerType::kDedicated,
                                            process3.get());
  const auto worker_context1 = worker1->GetResourceContext();

  // Simulates a query that never calls Start(), just uses QueryOnce() to
  // request results periodically.
  auto non_repeating_query =
      CreateQueryParams({ResourceType::kCPUTime}, /*resource_contexts=*/{},
                        {ResourceContextTypeId::ForType<ProcessContext>()});
  scheduler->AddScopedQuery(non_repeating_query.get());

  // Simulates queries that call Start() to request results for all processes
  // on a schedule.
  auto repeating_all_process_query =
      CreateQueryParams({ResourceType::kCPUTime}, /*resource_contexts=*/{},
                        {ResourceContextTypeId::ForType<ProcessContext>()});
  scheduler->AddScopedQuery(repeating_all_process_query.get());
  scheduler->StartRepeatingQuery(repeating_all_process_query.get());

  auto repeating_all_process_query2 =
      CreateQueryParams({ResourceType::kCPUTime}, /*resource_contexts=*/{},
                        {ResourceContextTypeId::ForType<ProcessContext>()});
  scheduler->AddScopedQuery(repeating_all_process_query2.get());
  scheduler->StartRepeatingQuery(repeating_all_process_query2.get());

  // Simulates a query that calls Start() to request results for a fixed set of
  // processes on a schedule.
  auto repeating_some_process_query =
      CreateQueryParams({ResourceType::kCPUTime},
                        {process_context1, process_context2, process_context3});
  scheduler->AddScopedQuery(repeating_some_process_query.get());
  scheduler->StartRepeatingQuery(repeating_some_process_query.get());

  EXPECT_TRUE(scheduler->GetCPUMonitorForTesting().IsMonitoring());
  EXPECT_FALSE(non_repeating_query->GetIdForTesting().has_value());
  ASSERT_TRUE(repeating_all_process_query->GetIdForTesting().has_value());
  ASSERT_TRUE(repeating_all_process_query2->GetIdForTesting().has_value());
  ASSERT_TRUE(repeating_some_process_query->GetIdForTesting().has_value());

  // Allow some time to pass to measure.
  task_env().FastForwardBy(base::Minutes(1));

  int i = 0;
  for (QueryParams* query :
       {non_repeating_query.get(), repeating_all_process_query.get(),
        repeating_all_process_query2.get(),
        repeating_some_process_query.get()}) {
    SCOPED_TRACE(::testing::Message() << "Query " << i++);
    ExpectQueryResult(
        scheduler, query,
        UnorderedElementsAre(
            ResultForContextMatches<CPUTimeResult>(process_context1, _),
            ResultForContextMatches<CPUTimeResult>(process_context2, _),
            ResultForContextMatches<CPUTimeResult>(process_context3, _)));
  }

  // Delete a process after the measurement. Results should still be delivered
  // to the repeating queries, but not the non-repeating query.
  process1.reset();
  task_env().FastForwardBy(base::Minutes(1));

  ExpectQueryResult(
      scheduler, non_repeating_query.get(),
      UnorderedElementsAre(
          ResultForContextMatches<CPUTimeResult>(process_context2, _),
          ResultForContextMatches<CPUTimeResult>(process_context3, _)));
  ExpectQueryResult(
      scheduler, repeating_all_process_query.get(),
      UnorderedElementsAre(
          ResultForContextMatches<CPUTimeResult>(process_context1, _),
          ResultForContextMatches<CPUTimeResult>(process_context2, _),
          ResultForContextMatches<CPUTimeResult>(process_context3, _)));

  task_env().FastForwardBy(base::Minutes(1));
  ExpectQueryResult(
      scheduler, non_repeating_query.get(),
      UnorderedElementsAre(
          ResultForContextMatches<CPUTimeResult>(process_context2, _),
          ResultForContextMatches<CPUTimeResult>(process_context3, _)));
  // Should not see the result for `process1` twice.
  ExpectQueryResult(
      scheduler, repeating_all_process_query.get(),
      UnorderedElementsAre(
          ResultForContextMatches<CPUTimeResult>(process_context2, _),
          ResultForContextMatches<CPUTimeResult>(process_context3, _)));
  // Seeing the result for `process1` for the first time.
  ExpectQueryResult(
      scheduler, repeating_all_process_query2.get(),
      UnorderedElementsAre(
          ResultForContextMatches<CPUTimeResult>(process_context1, _),
          ResultForContextMatches<CPUTimeResult>(process_context2, _),
          ResultForContextMatches<CPUTimeResult>(process_context3, _)));
  ExpectQueryResult(
      scheduler, repeating_some_process_query.get(),
      UnorderedElementsAre(
          ResultForContextMatches<CPUTimeResult>(process_context1, _),
          ResultForContextMatches<CPUTimeResult>(process_context2, _),
          ResultForContextMatches<CPUTimeResult>(process_context3, _)));

  task_env().FastForwardBy(base::Minutes(1));
  // All queries have now seen the result for `process1`.
  i = 0;
  for (QueryParams* query :
       {non_repeating_query.get(), repeating_all_process_query.get(),
        repeating_all_process_query2.get(),
        repeating_some_process_query.get()}) {
    SCOPED_TRACE(::testing::Message() << "Query " << i++);
    ExpectQueryResult(
        scheduler, query,
        UnorderedElementsAre(
            ResultForContextMatches<CPUTimeResult>(process_context2, _),
            ResultForContextMatches<CPUTimeResult>(process_context3, _)));
  }

  auto process4 = CreateRendererProcessNode();
  process4->SetProcess(base::Process::Current(), base::TimeTicks::Now());
  const auto process_context4 = process4->GetResourceContext();
  process2.reset();

  // Create a query that measures all context types.
  // Since it's created after `process2` dies it should never see its result.
  auto all_context_query = CreateQueryParams(
      {ResourceType::kCPUTime}, /*resource_contexts=*/{},
      {
          ResourceContextTypeId::ForType<FrameContext>(),
          ResourceContextTypeId::ForType<PageContext>(),
          ResourceContextTypeId::ForType<ProcessContext>(),
          ResourceContextTypeId::ForType<WorkerContext>(),
          ResourceContextTypeId::ForType<OriginInBrowsingInstanceContext>(),
      });
  scheduler->AddScopedQuery(all_context_query.get());
  scheduler->StartRepeatingQuery(all_context_query.get());
  ASSERT_TRUE(all_context_query->GetIdForTesting().has_value());

  task_env().FastForwardBy(base::Minutes(1));
  ExpectQueryResult(
      scheduler, non_repeating_query.get(),
      UnorderedElementsAre(
          ResultForContextMatches<CPUTimeResult>(process_context3, _),
          ResultForContextMatches<CPUTimeResult>(process_context4, _)));
  ExpectQueryResult(
      scheduler, repeating_all_process_query.get(),
      UnorderedElementsAre(
          ResultForContextMatches<CPUTimeResult>(process_context2, _),
          ResultForContextMatches<CPUTimeResult>(process_context3, _),
          ResultForContextMatches<CPUTimeResult>(process_context4, _)));
  ExpectQueryResult(
      scheduler, repeating_some_process_query.get(),
      UnorderedElementsAre(
          ResultForContextMatches<CPUTimeResult>(process_context2, _),
          ResultForContextMatches<CPUTimeResult>(process_context3, _)));
  ExpectQueryResult(
      scheduler, all_context_query.get(),
      UnorderedElementsAre(
          ResultForContextMatches<CPUTimeResult>(frame_context1, _),
          ResultForContextMatches<CPUTimeResult>(frame_context2, _),
          ResultForContextMatches<CPUTimeResult>(page_context1, _),
          ResultForContextMatches<CPUTimeResult>(process_context3, _),
          ResultForContextMatches<CPUTimeResult>(process_context4, _),
          ResultForContextMatches<CPUTimeResult>(worker_context1, _),
          ResultForContextMatches<CPUTimeResult>(origin_in_page_context1, _),
          ResultForContextMatches<CPUTimeResult>(origin_in_page_context2, _)));

  process4.reset();

  // Frames must be removed from the page before it's deleted.
  frame1.reset();
  frame2.reset();
  page1.reset();

  worker1.reset();

  task_env().FastForwardBy(base::Minutes(1));
  ExpectQueryResult(scheduler, non_repeating_query.get(),
                    UnorderedElementsAre(ResultForContextMatches<CPUTimeResult>(
                        process_context3, _)));
  // Already seen the response for `process2`, now sees the last response for
  // `process4`.
  ExpectQueryResult(
      scheduler, repeating_all_process_query.get(),
      UnorderedElementsAre(
          ResultForContextMatches<CPUTimeResult>(process_context3, _),
          ResultForContextMatches<CPUTimeResult>(process_context4, _)));
  // Now sees the last response for both `process2` and `process4`.
  ExpectQueryResult(
      scheduler, repeating_all_process_query2.get(),
      UnorderedElementsAre(
          ResultForContextMatches<CPUTimeResult>(process_context2, _),
          ResultForContextMatches<CPUTimeResult>(process_context3, _),
          ResultForContextMatches<CPUTimeResult>(process_context4, _)));
  // Already seen the response for `process2`, not measuring `process4`.
  ExpectQueryResult(scheduler, repeating_some_process_query.get(),
                    UnorderedElementsAre(ResultForContextMatches<CPUTimeResult>(
                        process_context3, _)));
  // Never measured `process2`, now sees the last response for `process4` and
  // all non-process contexts.
  ExpectQueryResult(
      scheduler, all_context_query.get(),
      UnorderedElementsAre(
          ResultForContextMatches<CPUTimeResult>(frame_context1, _),
          ResultForContextMatches<CPUTimeResult>(frame_context2, _),
          ResultForContextMatches<CPUTimeResult>(page_context1, _),
          ResultForContextMatches<CPUTimeResult>(process_context3, _),
          ResultForContextMatches<CPUTimeResult>(process_context4, _),
          ResultForContextMatches<CPUTimeResult>(worker_context1, _),
          ResultForContextMatches<CPUTimeResult>(origin_in_page_context1, _),
          ResultForContextMatches<CPUTimeResult>(origin_in_page_context2, _)));

  task_env().FastForwardBy(base::Minutes(1));
  // All queries have now seen the results for all dead contexts. Only
  // `process3` is live. Note: Results for dead
  // `OriginInBrowsingInstanceContext`s are retained in case they are revived.
  i = 0;
  for (QueryParams* query :
       {non_repeating_query.get(), repeating_all_process_query.get(),
        repeating_all_process_query2.get(), repeating_some_process_query.get(),
        all_context_query.get()}) {
    SCOPED_TRACE(::testing::Message() << "Query " << i++);
    ExpectQueryResult(
        scheduler, query,
        UnorderedElementsAre(
            ResultForContextMatches<CPUTimeResult>(process_context3, _)));
  }
  // Now that each query got a measurement without the dead
  // `OriginInBrowsingInstanceContext`s, no results should be retained.
  EXPECT_EQ(
      scheduler->GetCPUMonitorForTesting().GetDeadContextCountForTesting(), 0u);

  process3.reset();
  EXPECT_EQ(
      scheduler->GetCPUMonitorForTesting().GetDeadContextCountForTesting(), 4u);

  // As repeating queries are removed, results not reported to them should be
  // dropped.
  scheduler->RemoveScopedQuery(std::move(repeating_all_process_query));
  EXPECT_EQ(
      scheduler->GetCPUMonitorForTesting().GetDeadContextCountForTesting(), 3u);
  scheduler->RemoveScopedQuery(std::move(repeating_all_process_query2));
  EXPECT_EQ(
      scheduler->GetCPUMonitorForTesting().GetDeadContextCountForTesting(), 2u);
  scheduler->RemoveScopedQuery(std::move(repeating_some_process_query));
  EXPECT_EQ(
      scheduler->GetCPUMonitorForTesting().GetDeadContextCountForTesting(), 1u);
  scheduler->RemoveScopedQuery(std::move(all_context_query));
  EXPECT_EQ(
      scheduler->GetCPUMonitorForTesting().GetDeadContextCountForTesting(), 0u);

  scheduler->RemoveScopedQuery(std::move(non_repeating_query));
  EXPECT_FALSE(scheduler->GetCPUMonitorForTesting().IsMonitoring());
}

TEST_P(ResourceAttrQuerySchedulerPMTest, CallWithScheduler) {
  // Tests that CallWithScheduler works from PerformanceManagerTestHarness,
  // where the scheduler runs on the PM sequence as in production.
  EXPECT_TRUE(PerformanceManager::IsAvailable());
  QueryScheduler* scheduler_ptr = nullptr;
  Graph* graph_ptr = nullptr;
  performance_manager::RunInGraph([&](Graph* graph) {
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

TEST_P(ResourceAttrQuerySchedulerTest, CallWithScheduler) {
  // Tests that CallWithScheduler works from GraphTestHarness which doesn't set
  // up the PerformanceManager sequence. It's convenient to use GraphTestHarness
  // with mock graphs to test resource attribution queries.
  EXPECT_FALSE(performance_manager::PerformanceManager::IsAvailable());
  base::RunLoop run_loop;
  QueryScheduler::CallWithScheduler(
      base::BindLambdaForTesting([&](QueryScheduler* scheduler) {
        // The QueryScheduler was installed on the graph in SetUp().
        EXPECT_EQ(scheduler, graph()->GetRegisteredObjectAs<QueryScheduler>());
      }).Then(run_loop.QuitClosure()));
  run_loop.Run();
}

}  // namespace resource_attribution::internal
