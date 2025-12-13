// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/resource_attribution/queries.h"

#include <map>
#include <optional>
#include <set>
#include <utility>

#include "base/barrier_closure.h"
#include "base/containers/enum_set.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list_threadsafe.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/performance_manager/embedder/graph_features.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/public/resource_attribution/query_results.h"
#include "components/performance_manager/public/resource_attribution/resource_contexts.h"
#include "components/performance_manager/public/resource_attribution/resource_types.h"
#include "components/performance_manager/resource_attribution/context_collection.h"
#include "components/performance_manager/resource_attribution/performance_manager_aliases.h"
#include "components/performance_manager/resource_attribution/query_params.h"
#include "components/performance_manager/resource_attribution/query_scheduler.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/test_support/mock_graphs.h"
#include "components/performance_manager/test_support/performance_manager_test_harness.h"
#include "components/performance_manager/test_support/resource_attribution/gtest_util.h"
#include "components/performance_manager/test_support/resource_attribution/measurement_delegates.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace resource_attribution {

namespace {

using ::testing::_;
using ::testing::Between;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using QueryParams = internal::QueryParams;
using QueryScheduler = internal::QueryScheduler;
using ResourceContextTypeId = internal::ResourceContextTypeId;

constexpr auto kFrameContextTypeId =
    ResourceContextTypeId::ForType<FrameContext>();
constexpr auto kWorkerContextTypeId =
    ResourceContextTypeId::ForType<WorkerContext>();

// Fake memory results.
constexpr base::ByteCount kFakeResidentSetSize = base::KiB(123);
constexpr base::ByteCount kFakePrivateFootprint = base::KiB(456);
constexpr base::ByteCount kFakePrivateSwap = base::KiB(789);

class LenientMockQueryResultObserver : public QueryResultObserver {
 public:
  MOCK_METHOD(void,
              OnResourceUsageUpdated,
              (const QueryResultMap& results),
              (override));
};
using MockQueryResultObserver =
    ::testing::StrictMock<LenientMockQueryResultObserver>;

using ResourceAttrQueriesTest = performance_manager::GraphTestHarness;

// Tests that interact with the QueryScheduler use PerformanceManagerTestHarness
// to test its interactions on the PM sequence.
class ResourceAttrQueriesPMTest
    : public performance_manager::PerformanceManagerTestHarness {
 protected:
  using Super = performance_manager::PerformanceManagerTestHarness;

  ResourceAttrQueriesPMTest()
      : Super(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    GetGraphFeatures().EnableResourceAttributionScheduler();
    Super::SetUp();

    graph_ = performance_manager::PerformanceManager::GetGraph();
    CPUMeasurementDelegate::SetDelegateFactoryForTesting(
        graph_, &cpu_delegate_factory_);
    MemoryMeasurementDelegate::SetDelegateFactoryForTesting(
        graph_, &memory_delegate_factory_);

    // Navigate to an initial page.
    SetContents(CreateTestWebContents());
    content::RenderFrameHost* rfh =
        content::NavigationSimulator::NavigateAndCommitFromBrowser(
            web_contents(), GURL("https://a.com/"));
    ASSERT_TRUE(rfh);
    main_frame_context_ = FrameContext::FromRenderFrameHost(rfh);
    ASSERT_TRUE(main_frame_context_.has_value());

    // Set fake memory results for the page's process.
    content::RenderProcessHost* rph = rfh->GetProcess();
    ASSERT_TRUE(rph);
    main_frame_process_context_ = ProcessContext::FromRenderProcessHost(rph);
    memory_delegate_factory_
        .memory_summaries()[main_frame_process_context_.value()] =
        MemoryMeasurementDelegate::MemorySummaryMeasurement{
            .resident_set_size = kFakeResidentSetSize,
            .private_footprint = kFakePrivateFootprint,
            .private_swap = kFakePrivateSwap,
        };
  }

  void TearDown() override {
    graph_ = nullptr;
    Super::TearDown();
  }

  void TearDownGraph() {
    graph_ = nullptr;
    Super::TearDownNow();
  }

  Graph* graph() { return graph_.get(); }

  // A ResourceContext for the main frame.
  ResourceContext main_frame_context() const {
    return main_frame_context_.value();
  }

  // A ResourceContext for the main frame's process.
  ResourceContext main_frame_process_context() const {
    return main_frame_process_context_.value();
  }

  // Lets tests update the fake results for kMemorySummary queries.
  MemoryMeasurementDelegate::MemorySummaryMap& fake_memory_summaries() {
    return memory_delegate_factory_.memory_summaries();
  }

 private:
  raw_ptr<Graph> graph_ = nullptr;

  std::optional<FrameContext> main_frame_context_;
  std::optional<ProcessContext> main_frame_process_context_;

  // These must be deleted after TearDown() so that they outlive the
  // CPUMeasurementMonitor and MemoryMeasurementProvider.
  SimulatedCPUMeasurementDelegateFactory cpu_delegate_factory_;
  FakeMemoryMeasurementDelegateFactory memory_delegate_factory_;
};

QueryParams CreateQueryParams(
    ResourceTypeSet resource_types = {},
    std::set<ResourceContext> resource_contexts = {},
    std::set<ResourceContextTypeId> all_context_types = {}) {
  QueryParams params;
  params.resource_types = std::move(resource_types);
  params.contexts = ContextCollection::CreateForTesting(
      std::move(resource_contexts), std::move(all_context_types));
  return params;
}

// Returns a MemorySummaryResult containing the default fake memory results.
// This can be used for the results from a process, or a page or frame that gets
// all the memory from one process. `expected_algorithm` is the measurement
// algorithm for that context type, and `expected_measurement_time` is the time
// the measurement should be taken. By default, since the tests use the mock
// clock, the expected measurement time is the same time the fake result is
// created.
MemorySummaryResult FakeMemorySummaryResult(
    MeasurementAlgorithm expected_algorithm,
    base::TimeTicks expected_measurement_time = base::TimeTicks::Now()) {
  return {
      .metadata = ResultMetadata(expected_measurement_time, expected_algorithm),
      .resident_set_size = kFakeResidentSetSize,
      .private_footprint = kFakePrivateFootprint,
      .private_swap = kFakePrivateSwap,
  };
}

}  // namespace

namespace internal {

// Allow EXPECT_EQ to compare QueryParams, not including the QueryId.
bool operator==(const QueryParams& a, const QueryParams& b) {
  return a.resource_types == b.resource_types && a.contexts == b.contexts;
}

}  // namespace internal

TEST_F(ResourceAttrQueriesTest, QueryBuilder_Params) {
  performance_manager::MockSinglePageInSingleProcessGraph mock_graph(graph());

  QueryBuilder builder;
  ASSERT_TRUE(builder.GetParamsForTesting());
  EXPECT_EQ(*builder.GetParamsForTesting(), QueryParams{});

  QueryBuilder& builder_ref =
      builder.AddResourceContext(mock_graph.page->GetResourceContext())
          .AddResourceContext(mock_graph.process->GetResourceContext())
          .AddAllContextsOfType<FrameContext>()
          .AddAllContextsOfType<WorkerContext>()
          .AddResourceType(ResourceType::kCPUTime);
  EXPECT_EQ(builder.GetParamsForTesting(), builder_ref.GetParamsForTesting());

  const QueryParams expected_params =
      CreateQueryParams({ResourceType::kCPUTime},
                        {mock_graph.page->GetResourceContext(),
                         mock_graph.process->GetResourceContext()},
                        {kFrameContextTypeId, kWorkerContextTypeId});
  EXPECT_EQ(*builder.GetParamsForTesting(), expected_params);

  // Creating a ScopedQuery invalidates the builder.
  auto scoped_query = builder.CreateScopedQuery();
  EXPECT_FALSE(builder.GetParamsForTesting());
  ASSERT_TRUE(scoped_query.GetParamsForTesting());
  EXPECT_EQ(*scoped_query.GetParamsForTesting(), expected_params);
}

TEST_F(ResourceAttrQueriesTest, QueryBuilder_Clone) {
  performance_manager::MockSinglePageInSingleProcessGraph mock_graph(graph());
  QueryBuilder builder;
  builder.AddResourceContext(mock_graph.page->GetResourceContext())
      .AddAllContextsOfType<FrameContext>()
      .AddResourceType(ResourceType::kCPUTime);
  QueryBuilder cloned_builder = builder.Clone();

  ASSERT_TRUE(builder.GetParamsForTesting());
  ASSERT_TRUE(cloned_builder.GetParamsForTesting());
  EXPECT_EQ(*builder.GetParamsForTesting(),
            *cloned_builder.GetParamsForTesting());

  // Cloned builder can be modified independently.
  builder.AddResourceContext(mock_graph.process->GetResourceContext());
  cloned_builder.AddResourceContext(mock_graph.frame->GetResourceContext());
  cloned_builder.AddResourceType(ResourceType::kMemorySummary);

  EXPECT_EQ(*builder.GetParamsForTesting(),
            CreateQueryParams({ResourceType::kCPUTime},
                              {mock_graph.page->GetResourceContext(),
                               mock_graph.process->GetResourceContext()},
                              {kFrameContextTypeId}));
  EXPECT_EQ(
      *cloned_builder.GetParamsForTesting(),
      CreateQueryParams({ResourceType::kCPUTime, ResourceType::kMemorySummary},
                        {mock_graph.page->GetResourceContext(),
                         mock_graph.frame->GetResourceContext()},
                        {kFrameContextTypeId}));
}

TEST_F(ResourceAttrQueriesPMTest, QueryBuilder_QueryOnce_CPU) {
  auto expect_no_results = [&](const QueryResultMap& results) {
    // CPU measurements need to cover a period of time, so without a scoped
    // query to start the monitoring period there will be no results. This just
    // tests that the query request and empty result are delivered to and from
    // the scheduler.
    EXPECT_TRUE(results.empty());
  };

  base::RunLoop run_loop;
  QueryBuilder()
      .AddResourceContext(main_frame_context())
      .AddResourceType(ResourceType::kCPUTime)
      .QueryOnce(base::BindLambdaForTesting(expect_no_results)
                     .Then(run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(ResourceAttrQueriesPMTest, QueryBuilder_QueryOnce_Memory) {
  auto expect_memory_results = [&](const QueryResultMap& results) {
    EXPECT_THAT(results,
                ElementsAre(ResultForContextMatches<MemorySummaryResult>(
                    main_frame_context(),
                    FakeMemorySummaryResult(MeasurementAlgorithm::kSplit))));
  };

  base::RunLoop run_loop;
  QueryBuilder()
      .AddResourceContext(main_frame_context())
      .AddResourceType(ResourceType::kMemorySummary)
      .QueryOnce(base::BindLambdaForTesting(expect_memory_results)
                     .Then(run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(ResourceAttrQueriesPMTest, QueryBuilder_QueryOnce_CPUAndMemory) {
  auto expect_only_memory_results = [&](const QueryResultMap& results) {
    // CPU measurements need to cover a period of time, so without a scoped
    // query to start the monitoring period there will be no results. The memory
    // result should be delivered from the scheduler without a CPU measurement.
    EXPECT_THAT(results,
                ElementsAre(ResultForContextMatches<MemorySummaryResult>(
                    main_frame_context(),
                    FakeMemorySummaryResult(MeasurementAlgorithm::kSplit))));
  };

  base::RunLoop run_loop;
  QueryBuilder()
      .AddResourceContext(main_frame_context())
      .AddResourceType(ResourceType::kCPUTime)
      .AddResourceType(ResourceType::kMemorySummary)
      .QueryOnce(base::BindLambdaForTesting(expect_only_memory_results)
                     .Then(run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(ResourceAttrQueriesPMTest, AddRemoveScopedQuery) {
  auto* scheduler = QueryScheduler::GetFromGraph(
      performance_manager::PerformanceManager::GetGraph());
  // Abort the whole test if the scheduler wasn't found.
  ASSERT_TRUE(scheduler);
  EXPECT_EQ(scheduler->GetQueryCountForTesting(ResourceType::kCPUTime), 0U);
  EXPECT_EQ(scheduler->GetQueryCountForTesting(ResourceType::kMemorySummary),
            0U);

  std::optional<ScopedResourceUsageQuery> scoped_memory_query =
      QueryBuilder()
          .AddResourceContext(main_frame_context())
          .AddResourceType(ResourceType::kMemorySummary)
          .CreateScopedQuery();
  EXPECT_EQ(scheduler->GetQueryCountForTesting(ResourceType::kCPUTime), 0U);
  EXPECT_EQ(scheduler->GetQueryCountForTesting(ResourceType::kMemorySummary),
            1U);
  std::optional<ScopedResourceUsageQuery> scoped_cpu_query =
      QueryBuilder()
          .AddResourceContext(main_frame_context())
          .AddResourceType(ResourceType::kCPUTime)
          .CreateScopedQuery();
  EXPECT_EQ(scheduler->GetQueryCountForTesting(ResourceType::kCPUTime), 1U);
  EXPECT_EQ(scheduler->GetQueryCountForTesting(ResourceType::kMemorySummary),
            1U);
  scoped_memory_query.reset();
  EXPECT_EQ(scheduler->GetQueryCountForTesting(ResourceType::kCPUTime), 1U);
  EXPECT_EQ(scheduler->GetQueryCountForTesting(ResourceType::kMemorySummary),
            0U);
  std::optional<ScopedResourceUsageQuery> scoped_cpu_memory_query =
      QueryBuilder()
          .AddResourceContext(main_frame_context())
          .AddResourceType(ResourceType::kCPUTime)
          .AddResourceType(ResourceType::kMemorySummary)
          .CreateScopedQuery();
  EXPECT_EQ(scheduler->GetQueryCountForTesting(ResourceType::kCPUTime), 2U);
  EXPECT_EQ(scheduler->GetQueryCountForTesting(ResourceType::kMemorySummary),
            1U);
  scoped_cpu_query.reset();
  EXPECT_EQ(scheduler->GetQueryCountForTesting(ResourceType::kCPUTime), 1U);
  EXPECT_EQ(scheduler->GetQueryCountForTesting(ResourceType::kMemorySummary),
            1U);
  scoped_cpu_memory_query.reset();
  EXPECT_EQ(scheduler->GetQueryCountForTesting(ResourceType::kCPUTime), 0U);
  EXPECT_EQ(scheduler->GetQueryCountForTesting(ResourceType::kMemorySummary),
            0U);
}

TEST_F(ResourceAttrQueriesPMTest, ScopedQueryIsMovable) {
  QueryScheduler* scheduler = nullptr;
  scheduler = QueryScheduler::GetFromGraph(
      performance_manager::PerformanceManager::GetGraph());
  ASSERT_TRUE(scheduler);
  EXPECT_EQ(scheduler->GetQueryCountForTesting(ResourceType::kCPUTime), 0U);

  std::optional<ScopedResourceUsageQuery> outer_query;
  {
    ScopedResourceUsageQuery inner_query =
        QueryBuilder()
            .AddResourceContext(main_frame_context())
            .AddResourceType(ResourceType::kCPUTime)
            .CreateScopedQuery();
    EXPECT_EQ(scheduler->GetQueryCountForTesting(ResourceType::kCPUTime), 1U);

    auto* params = inner_query.GetParamsForTesting();
    EXPECT_TRUE(params);
    scoped_refptr<ScopedResourceUsageQuery::ObserverList> observer_list =
        inner_query.observer_list_;
    EXPECT_TRUE(observer_list);

    outer_query = std::move(inner_query);

    // Moving invalidates the original query.
    EXPECT_FALSE(inner_query.GetParamsForTesting());
    EXPECT_EQ(outer_query->GetParamsForTesting(), params);

    // There shouldn't be duplicate observers, to prevent extra notifications.
    EXPECT_FALSE(inner_query.observer_list_);
    EXPECT_EQ(outer_query->observer_list_, observer_list);
  }

  // `inner_query` should not notify the scheduler when it goes out of scope.
  EXPECT_EQ(scheduler->GetQueryCountForTesting(ResourceType::kCPUTime), 1U);
  outer_query.reset();
  EXPECT_EQ(scheduler->GetQueryCountForTesting(ResourceType::kCPUTime), 0U);
}

TEST_F(ResourceAttrQueriesPMTest, Observers) {
  ScopedResourceUsageQuery::ScopedDisableMemoryQueryDelayForTesting disable;

  ScopedResourceUsageQuery scoped_query =
      QueryBuilder()
          .AddResourceContext(main_frame_context())
          .AddResourceType(ResourceType::kCPUTime)
          .AddResourceType(ResourceType::kMemorySummary)
          .CreateScopedQuery();

  // Allow some time to pass to measure.
  task_environment()->FastForwardBy(base::Minutes(1));

  // Safely do nothing when no observers are registered.
  scoped_query.QueryOnce();

  // Allow some time to pass to give time for the query to run there. Nothing
  // should happen.
  task_environment()->FastForwardBy(base::Minutes(1));

  // Observer can be notified from the graph sequence when installed on any
  // thread.
  // TODO(crbug.com/40755583): This test can be simplified once observers use
  // ObserverList instead of ObserverListThreadSafe.
  MockQueryResultObserver main_thread_observer;
  scoped_query.AddObserver(&main_thread_observer);
  auto main_thread_task_runner = base::SequencedTaskRunner::GetCurrentDefault();

  MockQueryResultObserver graph_sequence_observer;
  scoped_refptr<base::SequencedTaskRunner> graph_sequence_task_runner;
  scoped_query.AddObserver(&graph_sequence_observer);
  graph_sequence_task_runner = base::SequencedTaskRunner::GetCurrentDefault();

  // Quit the RunLoop when both observers receive results. Expect each result to
  // contain a single ResourceContext with both results.
  base::RunLoop run_loop;
  auto barrier_closure = base::BarrierClosure(2, run_loop.QuitClosure());
  EXPECT_CALL(
      main_thread_observer,
      OnResourceUsageUpdated(ElementsAre(
          ResultForContextMatchesAll<MemorySummaryResult, CPUTimeResult>(
              main_frame_context(),
              FakeMemorySummaryResult(MeasurementAlgorithm::kSplit), _))))
      .WillOnce([&] {
        EXPECT_TRUE(main_thread_task_runner->RunsTasksInCurrentSequence());
        barrier_closure.Run();
      });
  EXPECT_CALL(
      graph_sequence_observer,
      OnResourceUsageUpdated(ElementsAre(
          ResultForContextMatchesAll<MemorySummaryResult, CPUTimeResult>(
              main_frame_context(),
              FakeMemorySummaryResult(MeasurementAlgorithm::kSplit), _))))
      .WillOnce([&] {
        EXPECT_TRUE(graph_sequence_task_runner->RunsTasksInCurrentSequence());
        barrier_closure.Run();
      });
  scoped_query.QueryOnce();
  run_loop.Run();
}

TEST_F(ResourceAttrQueriesPMTest, GraphTeardown) {
  // ScopedResourceUsageQuery registers with the QueryScheduler on creation and
  // unregisters on destruction. Make sure it's safe for it to outlive the
  // scheduler, which is deleted during graph teardown.
  std::optional<ScopedResourceUsageQuery> scoped_query =
      QueryBuilder()
          .AddResourceContext(main_frame_context())
          .AddResourceType(ResourceType::kCPUTime)
          .CreateScopedQuery();
  MockQueryResultObserver observer;
  scoped_query->AddObserver(&observer);

  TearDownGraph();

  // The test passes as long as these don't crash. `observer` should not be
  // notified (StrictMock will test this).
  scoped_query->QueryOnce();
  scoped_query.reset();
}

TEST_F(ResourceAttrQueriesPMTest, ScopedQueryAndQueryOnce) {
  QueryBuilder builder;
  builder.AddResourceContext(main_frame_context())
      .AddResourceType(ResourceType::kCPUTime)
      .AddResourceType(ResourceType::kMemorySummary);

  // Create a scoped query to start the CPU monitor.
  auto scoped_query = builder.Clone().CreateScopedQuery();

  // Allow some time to pass to measure.
  task_environment()->FastForwardBy(base::Minutes(1));

  auto expect_results = [&](const QueryResultMap& results) {
    // QueryOnce should get measurements that were collected for `scoped_query`,
    // including CPU time.
    EXPECT_THAT(
        results,
        ElementsAre(
            ResultForContextMatchesAll<MemorySummaryResult, CPUTimeResult>(
                main_frame_context(),
                FakeMemorySummaryResult(MeasurementAlgorithm::kSplit), _)));
  };

  base::RunLoop run_loop;
  builder.Clone().QueryOnce(
      base::BindLambdaForTesting(expect_results).Then(run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(ResourceAttrQueriesPMTest, RepeatingQueries) {
  constexpr auto kDelay = base::Minutes(1);
  constexpr int kRepetitions = 3;

  ScopedResourceUsageQuery::ScopedDisableMemoryQueryDelayForTesting disable;

  std::optional<ScopedResourceUsageQuery> scoped_query =
      QueryBuilder()
          .AddResourceContext(main_frame_context())
          .AddResourceType(ResourceType::kMemorySummary)
          .CreateScopedQuery();

  MockQueryResultObserver observer;
  scoped_query->AddObserver(&observer);

  // Query should not get a QueryId until it's started. Id's are assigned on the
  // PM sequence.
  EXPECT_EQ(scoped_query->GetParamsForTesting()->GetIdForTesting(),
            std::nullopt);

  // Returns a gMock matcher expecting that a QueryResultMap has a
  // MemorySummaryResult for main_frame_context().
  auto memory_result_matcher = [&](base::TimeTicks expected_measurement_time) {
    return ElementsAre(ResultForContextMatches<MemorySummaryResult>(
        main_frame_context(),
        FakeMemorySummaryResult(MeasurementAlgorithm::kSplit,
                                expected_measurement_time)));
  };

  // Expect exactly 1 query per repetition, with exactly kDelay between
  // measurements.
  {
    ::testing::InSequence s;
    base::TimeTicks next_measurement_time = base::TimeTicks::Now();
    for (int i = 0; i < kRepetitions; ++i) {
      next_measurement_time += kDelay;
      EXPECT_CALL(observer, OnResourceUsageUpdated(
                                memory_result_matcher(next_measurement_time)))
          .Times(1);
    }
  }

  scoped_query->Start(kDelay);

  auto* params = scoped_query->GetParamsForTesting();
  EXPECT_NE(params->GetIdForTesting(), std::nullopt);

  // Cloning the params should not clone the id.
  std::unique_ptr<QueryParams> cloned_params = params->Clone();
  EXPECT_EQ(*cloned_params, *params);
  EXPECT_EQ(cloned_params->GetIdForTesting(), std::nullopt);

  task_environment()->FastForwardBy(kDelay * kRepetitions);

  // Test changes that happen between repetitions.
  {
    ::testing::InSequence s;
    base::TimeTicks next_measurement_time = base::TimeTicks::Now();

    // Repetition 1.
    next_measurement_time += kDelay;
    EXPECT_CALL(observer, OnResourceUsageUpdated(
                              memory_result_matcher(next_measurement_time)))
        .Times(1);

    // QueryOnce called half-way to repetition 2.
    EXPECT_CALL(observer, OnResourceUsageUpdated(memory_result_matcher(
                              next_measurement_time + kDelay / 2)))
        .Times(1);

    // Repetition 2.
    next_measurement_time += kDelay;
    EXPECT_CALL(observer, OnResourceUsageUpdated(
                              memory_result_matcher(next_measurement_time)))
        .Times(1);

    // Memory provider returns error at next repetition. Observer should still
    // be notified.
    next_measurement_time += kDelay;
    EXPECT_CALL(observer, OnResourceUsageUpdated(IsEmpty())).Times(1);
  }

  // Repetition 1.
  task_environment()->FastForwardBy(kDelay);

  // QueryOnce called half-way to repetition 2.
  task_environment()->FastForwardBy(kDelay / 2);
  scoped_query->QueryOnce();

  // Repetition 2.
  task_environment()->FastForwardBy(kDelay / 2);

  // Memory provider returns error at next repetition.
  fake_memory_summaries().clear();
  task_environment()->FastForwardBy(kDelay);

  // Reporting should stop once the query is deleted. StrictMock will give an
  // error if OnResourceUsageUpdated() is called again.
  scoped_query.reset();
  task_environment()->FastForwardBy(kDelay);
}

TEST_F(ResourceAttrQueriesPMTest, RepeatingQueriesZeroDelta) {
  ScopedResourceUsageQuery scoped_query =
      QueryBuilder()
          .AddResourceContext(main_frame_context())
          .AddResourceType(ResourceType::kMemorySummary)
          .CreateScopedQuery();

  MockQueryResultObserver observer;
  scoped_query.AddObserver(&observer);

  EXPECT_CALL(observer, OnResourceUsageUpdated(_)).Times(0);

  scoped_query.Start(base::TimeDelta());
  task_environment()->FastForwardBy(base::Minutes(1));

  ::testing::Mock::VerifyAndClearExpectations(&observer);

  // Observer should be notified only by QueryOnce.
  EXPECT_CALL(observer, OnResourceUsageUpdated(_))
      .WillOnce(base::test::RunClosure(task_environment()->QuitClosure()));

  scoped_query.QueryOnce();
  task_environment()->RunUntilQuit();
}

TEST_F(ResourceAttrQueriesPMTest, ThrottleQueryOnce) {
  const base::TimeDelta min_query_once_delay =
      ScopedResourceUsageQuery::GetMinMemoryQueryDelayForTesting();
  const base::TimeDelta repeating_query_delay = min_query_once_delay * 5;

  // CPU-only query should not be throttled.
  auto cpu_query = QueryBuilder()
                       .AddResourceContext(main_frame_context())
                       .AddResourceType(ResourceType::kCPUTime)
                       .CreateScopedQuery();
  MockQueryResultObserver cpu_observer;
  cpu_query.AddObserver(&cpu_observer);

  // Memory-only query should be throttled.
  auto memory_query = QueryBuilder()
                          .AddResourceContext(main_frame_context())
                          .AddResourceType(ResourceType::kMemorySummary)
                          .CreateScopedQuery();
  MockQueryResultObserver memory_observer;
  memory_query.AddObserver(&memory_observer);

  // Memory+CPU query should be throttled.
  auto memory_cpu_query = QueryBuilder()
                              .AddResourceContext(main_frame_context())
                              .AddResourceType(ResourceType::kMemorySummary)
                              .AddResourceType(ResourceType::kCPUTime)
                              .CreateScopedQuery();
  MockQueryResultObserver memory_cpu_observer;
  memory_cpu_query.AddObserver(&memory_cpu_observer);

  // Helper to fast forward to a fixed delta from the start of the test.
  auto fast_forward_to = [this, start_time = base::TimeTicks::Now()](
                             base::TimeDelta delta_from_start) {
    task_environment()->FastForwardBy(start_time + delta_from_start -
                                      base::TimeTicks::Now());
  };

  // Queries should not get a QueryId until they're started. Id's are assigned
  // on the PM sequence.
  QueryParams* cpu_params = cpu_query.GetParamsForTesting();
  QueryParams* memory_params = memory_query.GetParamsForTesting();
  QueryParams* memory_cpu_params = memory_cpu_query.GetParamsForTesting();
  EXPECT_EQ(cpu_params->GetIdForTesting(), std::nullopt);
  EXPECT_EQ(memory_params->GetIdForTesting(), std::nullopt);
  EXPECT_EQ(memory_cpu_params->GetIdForTesting(), std::nullopt);

  // Each observer has its own sequence, since at each tick they could fire in
  // any order.
  ::testing::Sequence cpu_sequence, memory_sequence, memory_cpu_sequence;

  cpu_query.Start(repeating_query_delay);
  memory_query.Start(repeating_query_delay);
  memory_cpu_query.Start(repeating_query_delay);

  EXPECT_NE(cpu_params->GetIdForTesting(), std::nullopt);
  EXPECT_NE(cpu_params->GetIdForTesting(), memory_params->GetIdForTesting());
  EXPECT_NE(memory_params->GetIdForTesting(), std::nullopt);
  EXPECT_NE(memory_params->GetIdForTesting(),
            memory_cpu_params->GetIdForTesting());
  EXPECT_NE(memory_cpu_params->GetIdForTesting(), std::nullopt);
  EXPECT_NE(memory_cpu_params->GetIdForTesting(),
            cpu_params->GetIdForTesting());

  // QueryOnce just before the timer fires the first time.
  EXPECT_CALL(cpu_observer, OnResourceUsageUpdated(_)).InSequence(cpu_sequence);
  fast_forward_to(repeating_query_delay - min_query_once_delay +
                  base::Milliseconds(1));
  cpu_query.QueryOnce();
  memory_query.QueryOnce();
  memory_cpu_query.QueryOnce();

  // Timer fires.
  EXPECT_CALL(cpu_observer, OnResourceUsageUpdated(_)).InSequence(cpu_sequence);
  EXPECT_CALL(memory_observer, OnResourceUsageUpdated(_))
      .InSequence(memory_sequence);
  EXPECT_CALL(memory_cpu_observer, OnResourceUsageUpdated(_))
      .InSequence(memory_cpu_sequence);
  fast_forward_to(repeating_query_delay);

  // QueryOnce just after timer fires - should be throttled until
  // `min_query_once_delay` passes.
  EXPECT_CALL(cpu_observer, OnResourceUsageUpdated(_))
      .Times(3)
      .InSequence(cpu_sequence);
  EXPECT_CALL(memory_observer, OnResourceUsageUpdated(_))
      .InSequence(memory_sequence);
  EXPECT_CALL(memory_cpu_observer, OnResourceUsageUpdated(_))
      .InSequence(memory_cpu_sequence);
  // Throttled.
  cpu_query.QueryOnce();
  memory_query.QueryOnce();
  memory_cpu_query.QueryOnce();
  // Throttled.
  fast_forward_to(repeating_query_delay + min_query_once_delay -
                  base::Milliseconds(1));
  cpu_query.QueryOnce();
  memory_query.QueryOnce();
  memory_cpu_query.QueryOnce();
  // Not throttled.
  fast_forward_to(repeating_query_delay + min_query_once_delay);
  cpu_query.QueryOnce();
  memory_query.QueryOnce();
  memory_cpu_query.QueryOnce();

  // QueryOnce again just after a query - should be throttled until
  // `min_query_once_delay` passes again.
  EXPECT_CALL(cpu_observer, OnResourceUsageUpdated(_))
      .Times(3)
      .InSequence(cpu_sequence);
  EXPECT_CALL(memory_observer, OnResourceUsageUpdated(_))
      .InSequence(memory_sequence);
  EXPECT_CALL(memory_cpu_observer, OnResourceUsageUpdated(_))
      .InSequence(memory_cpu_sequence);
  // Throttled.
  cpu_query.QueryOnce();
  memory_query.QueryOnce();
  memory_cpu_query.QueryOnce();
  // Throttled.
  fast_forward_to(repeating_query_delay + 2 * min_query_once_delay -
                  base::Milliseconds(1));
  cpu_query.QueryOnce();
  memory_query.QueryOnce();
  memory_cpu_query.QueryOnce();
  // Not throttled.
  fast_forward_to(repeating_query_delay + 2 * min_query_once_delay);
  cpu_query.QueryOnce();
  memory_query.QueryOnce();
  memory_cpu_query.QueryOnce();

  // QueryOnce just before the timer fires again - should not start throttling
  // until inside `min_query_once_delay`.
  EXPECT_CALL(cpu_observer, OnResourceUsageUpdated(_))
      .Times(2)
      .InSequence(cpu_sequence);
  EXPECT_CALL(memory_observer, OnResourceUsageUpdated(_))
      .InSequence(memory_sequence);
  EXPECT_CALL(memory_cpu_observer, OnResourceUsageUpdated(_))
      .InSequence(memory_cpu_sequence);
  // Not throttled.
  fast_forward_to(2 * repeating_query_delay - min_query_once_delay);
  cpu_query.QueryOnce();
  memory_query.QueryOnce();
  memory_cpu_query.QueryOnce();
  // Throttled.
  fast_forward_to(2 * repeating_query_delay - min_query_once_delay +
                  base::Milliseconds(1));
  cpu_query.QueryOnce();
  memory_query.QueryOnce();
  memory_cpu_query.QueryOnce();

  // Timer fires (not throttled).
  EXPECT_CALL(cpu_observer, OnResourceUsageUpdated(_)).InSequence(cpu_sequence);
  EXPECT_CALL(memory_observer, OnResourceUsageUpdated(_))
      .InSequence(memory_sequence);
  EXPECT_CALL(memory_cpu_observer, OnResourceUsageUpdated(_))
      .InSequence(memory_cpu_sequence);
  fast_forward_to(2 * repeating_query_delay);
}

TEST_F(ResourceAttrQueriesPMTest, ObserveOtherQueries) {
  ScopedResourceUsageQuery::ScopedDisableMemoryQueryDelayForTesting disable;

  // Queries that drive repeated measurements.
  ScopedResourceUsageQuery cpu_query =
      QueryBuilder()
          .AddResourceContext(main_frame_context())
          .AddResourceType(ResourceType::kCPUTime)
          .CreateScopedQuery();
  MockQueryResultObserver cpu_observer;
  cpu_query.AddObserver(&cpu_observer);
  cpu_query.Start(base::Minutes(2));

  ScopedResourceUsageQuery memory_query =
      QueryBuilder()
          .AddResourceContext(main_frame_context())
          .AddResourceType(ResourceType::kMemorySummary)
          .CreateScopedQuery();
  MockQueryResultObserver memory_observer;
  memory_query.AddObserver(&memory_observer);
  memory_query.Start(base::Minutes(3));

  // Queries that don't trigger repeated measurements of their own.
  ScopedResourceUsageQuery passive_cpu_query =
      QueryBuilder()
          .AddResourceContext(main_frame_context())
          .AddResourceType(ResourceType::kCPUTime)
          .CreateScopedQuery();
  MockQueryResultObserver passive_cpu_observer;
  passive_cpu_query.AddObserver(&passive_cpu_observer);
  passive_cpu_query.Start(base::TimeDelta(), /*observe_other_queries=*/true);

  ScopedResourceUsageQuery passive_memory_query =
      QueryBuilder()
          .AddResourceContext(main_frame_context())
          .AddResourceType(ResourceType::kMemorySummary)
          .CreateScopedQuery();
  MockQueryResultObserver passive_memory_observer;
  passive_memory_query.AddObserver(&passive_memory_observer);
  passive_memory_query.Start(base::TimeDelta(), /*observe_other_queries=*/true);

  ScopedResourceUsageQuery passive_all_query =
      QueryBuilder()
          .AddAllContextsOfType<FrameContext>()
          .AddResourceType(ResourceType::kCPUTime)
          .AddResourceType(ResourceType::kMemorySummary)
          .CreateScopedQuery();
  MockQueryResultObserver passive_all_observer;
  passive_all_query.AddObserver(&passive_all_observer);
  passive_all_query.Start(base::TimeDelta(), /*observe_other_queries=*/true);

  // Should be notified with results from the main frame's process, which must
  // be measured to calculate the main frame's results, even though no query
  // directly measures it.
  ScopedResourceUsageQuery passive_process_query =
      QueryBuilder()
          .AddResourceContext(main_frame_process_context())
          .AddResourceType(ResourceType::kCPUTime)
          .AddResourceType(ResourceType::kMemorySummary)
          .CreateScopedQuery();
  MockQueryResultObserver passive_process_observer;
  passive_process_query.AddObserver(&passive_process_observer);
  passive_process_query.Start(base::TimeDelta(),
                              /*observe_other_queries=*/true);

  // Should not be notified because it doesn't observe a context with results.
  ScopedResourceUsageQuery passive_no_context_query =
      QueryBuilder()
          .AddAllContextsOfType<WorkerContext>()
          .AddResourceType(ResourceType::kCPUTime)
          .AddResourceType(ResourceType::kMemorySummary)
          .CreateScopedQuery();
  MockQueryResultObserver passive_no_context_observer;
  passive_no_context_query.AddObserver(&passive_no_context_observer);
  passive_no_context_query.Start(base::TimeDelta(),
                                 /*observe_other_queries=*/true);

  // This query both makes its own repeated measurements, and observes others.
  ScopedResourceUsageQuery other_cpu_query =
      QueryBuilder()
          .AddResourceContext(main_frame_context())
          .AddResourceType(ResourceType::kCPUTime)
          .CreateScopedQuery();
  MockQueryResultObserver other_cpu_observer;
  other_cpu_query.AddObserver(&other_cpu_observer);
  other_cpu_query.Start(base::Minutes(5), /*observe_other_queries=*/true);

  auto verify_and_clear_expectations = [&] {
    ::testing::Mock::VerifyAndClearExpectations(&cpu_observer);
    ::testing::Mock::VerifyAndClearExpectations(&memory_observer);
    ::testing::Mock::VerifyAndClearExpectations(&passive_cpu_observer);
    ::testing::Mock::VerifyAndClearExpectations(&passive_memory_observer);
    ::testing::Mock::VerifyAndClearExpectations(&passive_all_observer);
    ::testing::Mock::VerifyAndClearExpectations(&passive_process_observer);
    ::testing::Mock::VerifyAndClearExpectations(&passive_no_context_observer);
    ::testing::Mock::VerifyAndClearExpectations(&other_cpu_observer);
  };

  // No measurements after 1 minute.
  task_environment()->FastForwardBy(base::Minutes(1));
  verify_and_clear_expectations();

  // At minute 2, `cpu_query` measures CPU.
  EXPECT_CALL(cpu_observer, OnResourceUsageUpdated(_));
  EXPECT_CALL(passive_cpu_observer, OnResourceUsageUpdated(_));
  EXPECT_CALL(passive_all_observer, OnResourceUsageUpdated(_));
  EXPECT_CALL(passive_process_observer, OnResourceUsageUpdated(_));
  EXPECT_CALL(other_cpu_observer, OnResourceUsageUpdated(_));
  task_environment()->FastForwardBy(base::Minutes(1));
  verify_and_clear_expectations();

  // At minute 3, `memory_query` measures memory.
  EXPECT_CALL(memory_observer, OnResourceUsageUpdated(_));
  EXPECT_CALL(passive_memory_observer, OnResourceUsageUpdated(_));
  EXPECT_CALL(passive_all_observer, OnResourceUsageUpdated(_));
  EXPECT_CALL(passive_process_observer, OnResourceUsageUpdated(_));
  task_environment()->FastForwardBy(base::Minutes(1));
  verify_and_clear_expectations();

  // At minute 4, `cpu_query` measures CPU again.
  EXPECT_CALL(cpu_observer, OnResourceUsageUpdated(_));
  EXPECT_CALL(passive_cpu_observer, OnResourceUsageUpdated(_));
  EXPECT_CALL(passive_all_observer, OnResourceUsageUpdated(_));
  EXPECT_CALL(passive_process_observer, OnResourceUsageUpdated(_));
  EXPECT_CALL(other_cpu_observer, OnResourceUsageUpdated(_));
  task_environment()->FastForwardBy(base::Minutes(1));
  verify_and_clear_expectations();

  // At minute 5, `other_cpu_query` measures CPU. `cpu_observer` should NOT be
  // notified because it doesn't have `observe_other_queries` set.
  EXPECT_CALL(cpu_observer, OnResourceUsageUpdated(_)).Times(0);
  EXPECT_CALL(passive_cpu_observer, OnResourceUsageUpdated(_));
  EXPECT_CALL(passive_all_observer, OnResourceUsageUpdated(_));
  EXPECT_CALL(passive_process_observer, OnResourceUsageUpdated(_));
  // Must only be notified once (not once for the repeating timer and again for
  // `observe_other_queries`).
  EXPECT_CALL(other_cpu_observer, OnResourceUsageUpdated(_)).Times(1);
  task_environment()->FastForwardBy(base::Minutes(1));
  verify_and_clear_expectations();

  // At minute 6, `cpu_query` and `memory_query` both take measurements.
  EXPECT_CALL(cpu_observer, OnResourceUsageUpdated(_));
  EXPECT_CALL(memory_observer, OnResourceUsageUpdated(_));
  EXPECT_CALL(passive_cpu_observer, OnResourceUsageUpdated(_));
  EXPECT_CALL(passive_memory_observer, OnResourceUsageUpdated(_));
  // Memory and CPU results may be notified together or separately.
  EXPECT_CALL(passive_all_observer, OnResourceUsageUpdated(_))
      .Times(Between(1, 2));
  EXPECT_CALL(passive_process_observer, OnResourceUsageUpdated(_))
      .Times(Between(1, 2));
  EXPECT_CALL(other_cpu_observer, OnResourceUsageUpdated(_));
  task_environment()->FastForwardBy(base::Minutes(1));
  verify_and_clear_expectations();

  // QueryOnce also notifies other queries.
  {
    auto barrier_closure =
        base::BarrierClosure(5, task_environment()->QuitClosure());
    EXPECT_CALL(cpu_observer, OnResourceUsageUpdated(_))
        .WillOnce(base::test::RunClosure(barrier_closure));
    EXPECT_CALL(passive_cpu_observer, OnResourceUsageUpdated(_))
        .WillOnce(base::test::RunClosure(barrier_closure));
    EXPECT_CALL(passive_all_observer, OnResourceUsageUpdated(_))
        .WillOnce(base::test::RunClosure(barrier_closure));
    EXPECT_CALL(passive_process_observer, OnResourceUsageUpdated(_))
        .WillOnce(base::test::RunClosure(barrier_closure));
    EXPECT_CALL(other_cpu_observer, OnResourceUsageUpdated(_))
        .WillOnce(base::test::RunClosure(barrier_closure));
    cpu_query.QueryOnce();
    task_environment()->RunUntilQuit();
    verify_and_clear_expectations();
  }

  // QueryOnce on a query that doesn't make repeating queries also notifies
  // other queries, but NOT queries without `observe_other_queries` set.
  {
    auto barrier_closure =
        base::BarrierClosure(4, task_environment()->QuitClosure());
    EXPECT_CALL(cpu_observer, OnResourceUsageUpdated(_)).Times(0);
    // Must only be notified once (not once for QueryOnce and again for
    // `observe_other_queries`).
    EXPECT_CALL(passive_cpu_observer, OnResourceUsageUpdated(_))
        .WillOnce(base::test::RunClosure(barrier_closure));
    EXPECT_CALL(passive_all_observer, OnResourceUsageUpdated(_))
        .WillOnce(base::test::RunClosure(barrier_closure));
    EXPECT_CALL(passive_process_observer, OnResourceUsageUpdated(_))
        .WillOnce(base::test::RunClosure(barrier_closure));
    EXPECT_CALL(other_cpu_observer, OnResourceUsageUpdated(_))
        .WillOnce(base::test::RunClosure(barrier_closure));
    passive_cpu_query.QueryOnce();
    task_environment()->RunUntilQuit();
    verify_and_clear_expectations();
  }

  // A non-scoped query should also notify scoped query observers with
  // `observe_other_queries` set.
  auto barrier_closure =
      base::BarrierClosure(4, task_environment()->QuitClosure());
  EXPECT_CALL(passive_memory_observer, OnResourceUsageUpdated(_))
      .WillOnce(base::test::RunClosure(barrier_closure));
  EXPECT_CALL(passive_all_observer, OnResourceUsageUpdated(_))
      .WillOnce(base::test::RunClosure(barrier_closure));
  EXPECT_CALL(passive_process_observer, OnResourceUsageUpdated(_))
      .WillOnce(base::test::RunClosure(barrier_closure));
  QueryBuilder()
      .AddResourceContext(main_frame_context())
      .AddResourceType(ResourceType::kMemorySummary)
      .QueryOnce(base::IgnoreArgs<const QueryResultMap&>(barrier_closure));
  task_environment()->RunUntilQuit();
  verify_and_clear_expectations();
}

}  // namespace resource_attribution
