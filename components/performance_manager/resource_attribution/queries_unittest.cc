// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/resource_attribution/queries.h"

#include <bitset>
#include <map>
#include <set>
#include <type_traits>
#include <utility>

#include "base/barrier_closure.h"
#include "base/containers/enum_set.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list_threadsafe.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/performance_manager/embedder/graph_features.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/performance_manager/public/resource_attribution/query_results.h"
#include "components/performance_manager/public/resource_attribution/resource_contexts.h"
#include "components/performance_manager/public/resource_attribution/resource_types.h"
#include "components/performance_manager/resource_attribution/cpu_measurement_monitor.h"
#include "components/performance_manager/resource_attribution/query_params.h"
#include "components/performance_manager/resource_attribution/query_scheduler.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/test_support/mock_graphs.h"
#include "components/performance_manager/test_support/performance_manager_test_harness.h"
#include "components/performance_manager/test_support/resource_attribution/gtest_util.h"
#include "components/performance_manager/test_support/resource_attribution/simulated_cpu_measurement_delegate.h"
#include "components/performance_manager/test_support/run_in_graph.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/gurl.h"

namespace performance_manager::resource_attribution {

namespace {

using ::testing::ElementsAre;
using QueryParams = internal::QueryParams;

class LenientMockQueryResultObserver : public QueryResultObserver {
 public:
  MOCK_METHOD(void,
              OnResourceUsageUpdated,
              (const QueryResultMap& results),
              (override));
};
using MockQueryResultObserver =
    ::testing::StrictMock<LenientMockQueryResultObserver>;

using ResourceAttrQueriesTest = GraphTestHarness;

// Tests that interact with the QueryScheduler use PerformanceManagerTestHarness
// to test its interactions on the PM sequence.
class ResourceAttrQueriesPMTest : public PerformanceManagerTestHarness {
 protected:
  using Super = PerformanceManagerTestHarness;

  ResourceAttrQueriesPMTest()
      : Super(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    GetGraphFeatures().EnableResourceAttributionScheduler();
    Super::SetUp();

    RunInGraph([&](Graph* graph) {
      graph_ = graph;
      CPUMeasurementDelegate::SetDelegateFactoryForTesting(graph,
                                                           &delegate_factory_);
    });

    // Navigate to an initial page.
    SetContents(CreateTestWebContents());
    content::RenderFrameHost* rfh =
        content::NavigationSimulator::NavigateAndCommitFromBrowser(
            web_contents(), GURL("https://a.com/"));
    ASSERT_TRUE(rfh);
    main_frame_context_ = FrameContext::FromRenderFrameHost(rfh);
    ASSERT_TRUE(main_frame_context_.has_value());
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

 private:
  raw_ptr<Graph> graph_ = nullptr;

  absl::optional<FrameContext> main_frame_context_;

  // This must be deleted after TearDown() so that it outlives the
  // CPUMeasurementMonitor.
  SimulatedCPUMeasurementDelegateFactory delegate_factory_;
};

}  // namespace

TEST_F(ResourceAttrQueriesTest, QueryBuilderParams) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());

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

  constexpr size_t kFrameContextBit = 0;
  constexpr size_t kWorkerContextBit = 3;
  static_assert(
      std::is_same_v<
          absl::variant_alternative_t<kFrameContextBit, ResourceContext>,
          FrameContext>,
      "FrameContext is no longer index 0 in the ResourceContext variant, "
      "please update the test");
  static_assert(
      std::is_same_v<
          absl::variant_alternative_t<kWorkerContextBit, ResourceContext>,
          WorkerContext>,
      "WorkerContext is no longer index 3 in the ResourceContext variant, "
      "please update the test");

  QueryParams expected_params;
  expected_params.resource_contexts = {
      mock_graph.page->GetResourceContext(),
      mock_graph.process->GetResourceContext()};
  expected_params.all_context_types.set(kFrameContextBit);
  expected_params.all_context_types.set(kWorkerContextBit);
  expected_params.resource_types = {ResourceType::kCPUTime};

  EXPECT_EQ(*builder.GetParamsForTesting(), expected_params);

  // Creating a ScopedQuery invalidates the builder.
  auto scoped_query = builder.CreateScopedQuery();
  EXPECT_FALSE(builder.GetParamsForTesting());
  ASSERT_TRUE(scoped_query.GetParamsForTesting());
  EXPECT_EQ(*scoped_query.GetParamsForTesting(), expected_params);
}

TEST_F(ResourceAttrQueriesTest, QueryBuilderClone) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
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

  const std::set<ResourceContext> expected_contexts{
      mock_graph.page->GetResourceContext(),
      mock_graph.process->GetResourceContext()};
  EXPECT_EQ(builder.GetParamsForTesting()->resource_contexts,
            expected_contexts);
  const std::set<ResourceContext> expected_cloned_contexts{
      mock_graph.page->GetResourceContext(),
      mock_graph.frame->GetResourceContext()};
  EXPECT_EQ(cloned_builder.GetParamsForTesting()->resource_contexts,
            expected_cloned_contexts);
}

TEST_F(ResourceAttrQueriesPMTest, QueryBuilderQueryOnce) {
  base::RunLoop run_loop;
  QueryBuilder()
      .AddResourceContext(main_frame_context())
      .AddResourceType(ResourceType::kCPUTime)
      .QueryOnce(base::BindLambdaForTesting([](const QueryResultMap& results) {
                   // CPU measurements need to cover a period of time, so
                   // without a scoped query to start the monitoring period
                   // there will be no results. This just tests that the query
                   // request and empty result are delivered to and from the
                   // scheduler.
                   EXPECT_TRUE(results.empty());
                 }).Then(run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(ResourceAttrQueriesPMTest, QueryBuilderQueryOnceWithTaskRunner) {
  auto main_thread_task_runner = base::SequencedTaskRunner::GetCurrentDefault();
  base::RunLoop run_loop;
  auto expect_empty_on_main_thread =
      [main_thread_task_runner,
       quit_closure = run_loop.QuitClosure()](const QueryResultMap& results) {
        EXPECT_TRUE(results.empty());
        EXPECT_TRUE(main_thread_task_runner->RunsTasksInCurrentSequence());
        std::move(quit_closure).Run();
      };

  // Create the query on the graph sequence, but tell it to run the result
  // callback on the main thread.
  RunInGraph([&] {
    QueryBuilder()
        .AddResourceContext(main_frame_context())
        .AddResourceType(ResourceType::kCPUTime)
        .QueryOnce(base::BindLambdaForTesting(expect_empty_on_main_thread),
                   main_thread_task_runner);
  });

  // Block the main thread until the result is received.
  run_loop.Run();
}

TEST_F(ResourceAttrQueriesPMTest, AddRemoveScopedQuery) {
  QueryScheduler* scheduler = nullptr;
  RunInGraph([&](Graph* graph) {
    scheduler = QueryScheduler::GetFromGraph(graph);
    ASSERT_TRUE(scheduler);
    EXPECT_FALSE(scheduler->GetCPUMonitorForTesting().IsMonitoring());
  });
  // Abort the whole test if the scheduler wasn't found.
  ASSERT_TRUE(scheduler);

  absl::optional<ScopedResourceUsageQuery> scoped_query =
      QueryBuilder()
          .AddResourceContext(main_frame_context())
          .AddResourceType(ResourceType::kCPUTime)
          .CreateScopedQuery();
  RunInGraph([&] {
    EXPECT_TRUE(scheduler->GetCPUMonitorForTesting().IsMonitoring());
  });
  scoped_query.reset();
  RunInGraph([&] {
    EXPECT_FALSE(scheduler->GetCPUMonitorForTesting().IsMonitoring());
  });
}

TEST_F(ResourceAttrQueriesPMTest, ScopedQueryIsMovable) {
  QueryScheduler* scheduler = nullptr;
  RunInGraph([&](Graph* graph) {
    scheduler = QueryScheduler::GetFromGraph(graph);
    ASSERT_TRUE(scheduler);
    EXPECT_FALSE(scheduler->GetCPUMonitorForTesting().IsMonitoring());
  });
  // Abort the whole test if the scheduler wasn't found.
  ASSERT_TRUE(scheduler);

  absl::optional<ScopedResourceUsageQuery> outer_query;
  {
    ScopedResourceUsageQuery inner_query =
        QueryBuilder()
            .AddResourceContext(main_frame_context())
            .AddResourceType(ResourceType::kCPUTime)
            .CreateScopedQuery();
    RunInGraph([&] {
      EXPECT_TRUE(scheduler->GetCPUMonitorForTesting().IsMonitoring());
    });

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
  RunInGraph([&] {
    EXPECT_TRUE(scheduler->GetCPUMonitorForTesting().IsMonitoring());
  });
  outer_query.reset();
  RunInGraph([&] {
    EXPECT_FALSE(scheduler->GetCPUMonitorForTesting().IsMonitoring());
  });
}

TEST_F(ResourceAttrQueriesPMTest, Observers) {
  ScopedResourceUsageQuery scoped_query =
      QueryBuilder()
          .AddResourceContext(main_frame_context())
          .AddResourceType(ResourceType::kCPUTime)
          .CreateScopedQuery();

  // Allow some time to pass to measure.
  task_environment()->FastForwardBy(base::Minutes(1));

  // Safely do nothing when no observers are registered.
  scoped_query.QueryOnce();
  // Post an empty task to the graph sequence to give time for the query to run
  // there. Nothing should happen.
  RunInGraph([] {});

  // Observer can be notified from the graph sequence when installed on any
  // thread.
  MockQueryResultObserver main_thread_observer;
  scoped_query.AddObserver(&main_thread_observer);
  auto main_thread_task_runner = base::SequencedTaskRunner::GetCurrentDefault();

  MockQueryResultObserver graph_sequence_observer;
  scoped_refptr<base::SequencedTaskRunner> graph_sequence_task_runner;
  RunInGraph([&] {
    scoped_query.AddObserver(&graph_sequence_observer);
    graph_sequence_task_runner = base::SequencedTaskRunner::GetCurrentDefault();
  });

  // Quit the RunLoop when both observers receive results. Expect each result to
  // contain a single ResourceContext with a CPUTimeResult.
  base::RunLoop run_loop;
  auto barrier_closure = base::BarrierClosure(2, run_loop.QuitClosure());
  EXPECT_CALL(
      main_thread_observer,
      OnResourceUsageUpdated(ElementsAre(
          QueryResultMapEntryMatches<CPUTimeResult>(main_frame_context()))))
      .WillOnce([&] {
        EXPECT_TRUE(main_thread_task_runner->RunsTasksInCurrentSequence());
        barrier_closure.Run();
      });
  EXPECT_CALL(
      graph_sequence_observer,
      OnResourceUsageUpdated(ElementsAre(
          QueryResultMapEntryMatches<CPUTimeResult>(main_frame_context()))))
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
  absl::optional<ScopedResourceUsageQuery> scoped_query =
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
      .AddResourceType(ResourceType::kCPUTime);

  // Create a scoped query to start the CPU monitor.
  auto scoped_query = builder.Clone().CreateScopedQuery();

  // Allow some time to pass to measure.
  task_environment()->FastForwardBy(base::Minutes(1));

  base::RunLoop run_loop;
  builder.Clone().QueryOnce(
      base::BindLambdaForTesting([&](const QueryResultMap& results) {
        // QueryOnce should get measurements that were collected for
        // `scoped_query`.
        EXPECT_THAT(results,
                    ElementsAre(QueryResultMapEntryMatches<CPUTimeResult>(
                        main_frame_context())));
      }).Then(run_loop.QuitClosure()));
  run_loop.Run();
}

}  // namespace performance_manager::resource_attribution
