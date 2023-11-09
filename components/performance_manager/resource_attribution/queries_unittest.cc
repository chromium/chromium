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
#include "base/dcheck_is_on.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list_threadsafe.h"
#include "base/run_loop.h"
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

// Test QueryBuilder using mock graphs.
using QueryBuilderTest = GraphTestHarness;

// Test ScopedResourceUsageQuery with PerformanceManagerTestHarness to test its
// interactions on the PM sequence.
class ScopedResourceUsageQueryTest : public PerformanceManagerTestHarness {
 protected:
  using Super = PerformanceManagerTestHarness;

  void SetUp() override {
    GetGraphFeatures().EnableResourceAttributionScheduler();
    Super::SetUp();

    // Navigate to an initial page.
    SetContents(CreateTestWebContents());
    content::RenderFrameHost* rfh =
        content::NavigationSimulator::NavigateAndCommitFromBrowser(
            web_contents(), GURL("https://a.com/"));
    ASSERT_TRUE(rfh);
    main_frame_context = FrameContext::FromRenderFrameHost(rfh);
    ASSERT_TRUE(main_frame_context.has_value());
  }

  // A ResourceContext for the main frame.
  absl::optional<FrameContext> main_frame_context;
};

}  // namespace

TEST_F(QueryBuilderTest, Params) {
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

TEST_F(QueryBuilderTest, Clone) {
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

TEST_F(ScopedResourceUsageQueryTest, AddRemoveScopedQuery) {
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

TEST_F(ScopedResourceUsageQueryTest, Movable) {
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

TEST_F(ScopedResourceUsageQueryTest, Observers) {
  ScopedResourceUsageQuery scoped_query =
      QueryBuilder()
          .AddResourceContext(main_frame_context.value())
          .AddResourceType(ResourceType::kCPUTime)
          .CreateScopedQuery();

  const QueryResultMap test_results{
      {main_frame_context.value(),
       {CPUTimeResult{.cumulative_cpu = base::Minutes(1)}}},
  };

  // Safely do nothing when no observers are registered.
  Graph* graph_ptr = nullptr;
  RunInGraph([&](Graph* graph) {
    graph_ptr = graph;
    // TODO(crbug.com/1471683): QueryScheduler should be notifying the
    // observers.
    scoped_query.observer_list_->Notify(
        FROM_HERE, &QueryResultObserver::OnResourceUsageUpdated, test_results);
  });
  ASSERT_TRUE(graph_ptr);

  // Observer can be notified from the graph sequence when installed on any
  // thread.
  MockQueryResultObserver main_thread_observer;
  MockQueryResultObserver graph_sequence_observer;
  scoped_query.AddObserver(&main_thread_observer);
  RunInGraph([&] { scoped_query.AddObserver(&graph_sequence_observer); });

  auto check_graph_sequence = [&](bool expect_on_graph_sequence) {
#if DCHECK_IS_ON()
    EXPECT_EQ(graph_ptr->IsOnGraphSequence(), expect_on_graph_sequence);
#endif
  };

  // Quit the RunLoop when both observers receive results.
  base::RunLoop run_loop;
  auto quit_closure = base::BarrierClosure(2, run_loop.QuitClosure());
  EXPECT_CALL(main_thread_observer, OnResourceUsageUpdated(test_results))
      .WillOnce([&] {
        check_graph_sequence(false);
        quit_closure.Run();
      });
  EXPECT_CALL(graph_sequence_observer, OnResourceUsageUpdated(test_results))
      .WillOnce([&] {
        check_graph_sequence(true);
        quit_closure.Run();
      });

  RunInGraph([&] {
    scoped_query.observer_list_->Notify(
        FROM_HERE, &QueryResultObserver::OnResourceUsageUpdated, test_results);
  });

  // Wait for all notifications.
  run_loop.Run();
}

TEST_F(ScopedResourceUsageQueryTest, GraphTeardown) {
  // ScopedResourceUsageQuery registers with the QueryScheduler on creation and
  // unregisters on destruction. Make sure it's safe for it to outlive the
  // scheduler, which is deleted during graph teardown.
  absl::optional<ScopedResourceUsageQuery> scoped_query =
      QueryBuilder()
          .AddResourceContext(main_frame_context.value())
          .AddResourceType(ResourceType::kCPUTime)
          .CreateScopedQuery();

  TearDownNow();

  // The test passes as long as this doesn't crash.
  scoped_query.reset();
}

}  // namespace performance_manager::resource_attribution
