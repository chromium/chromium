// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/decorators/process_metrics_decorator.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/byte_count.h"
#include "base/memory/raw_ptr.h"
#include "base/process/process.h"
#include "base/run_loop.h"
#include "base/task/current_thread.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/performance_manager/public/resource_attribution/attribution_helpers.h"
#include "components/performance_manager/public/resource_attribution/query_results.h"
#include "components/performance_manager/test_support/graph/mock_system_node_observer.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/test_support/mock_graphs.h"
#include "components/performance_manager/test_support/resource_attribution/measurement_delegates.h"
#include "content/public/common/process_type.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/global_memory_dump.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/memory_instrumentation.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

namespace {

using ::testing::_;
using FakeMemoryMeasurementDelegateFactory =
    resource_attribution::FakeMemoryMeasurementDelegateFactory;
using MemoryMeasurementDelegate =
    resource_attribution::MemoryMeasurementDelegate;
using QueryBuilder = resource_attribution::QueryBuilder;
using ScopedResourceUsageQuery = resource_attribution::ScopedResourceUsageQuery;
using ResourceType = resource_attribution::ResourceType;
using FrameContext = resource_attribution::FrameContext;
using ProcessContext = resource_attribution::ProcessContext;
using WorkerContext = resource_attribution::WorkerContext;

constexpr base::ByteCount kFakeResidentSet = base::KiB(12345);
constexpr base::ByteCount kFakePrivateFootprint = base::KiB(67890);
constexpr base::ByteCount kFakePrivateSwap = base::KiB(13579);

// Test version of the |ProcessMetricsDecorator| class.
class LenientTestProcessMetricsDecorator : public ProcessMetricsDecorator {
 public:
  LenientTestProcessMetricsDecorator() = default;
  ~LenientTestProcessMetricsDecorator() override = default;
};
using TestProcessMetricsDecorator =
    ::testing::StrictMock<LenientTestProcessMetricsDecorator>;

}  // namespace

class ProcessMetricsDecoratorTest : public GraphTestHarness {
 public:
  ProcessMetricsDecoratorTest(const ProcessMetricsDecoratorTest&) = delete;
  ProcessMetricsDecoratorTest& operator=(const ProcessMetricsDecoratorTest&) =
      delete;

 protected:
  using Super = GraphTestHarness;

  ProcessMetricsDecoratorTest() = default;
  ~ProcessMetricsDecoratorTest() override = default;

  void SetUp() override {
    GetGraphFeatures().EnableResourceAttributionScheduler();
    Super::SetUp();
    MemoryMeasurementDelegate::SetDelegateFactoryForTesting(
        graph(), &memory_delegate_factory_);

    std::unique_ptr<TestProcessMetricsDecorator> decorator =
        std::make_unique<TestProcessMetricsDecorator>();
    decorator_raw_ = decorator.get();
    mock_graph_ =
        std::make_unique<MockMultiplePagesAndWorkersWithMultipleProcessesGraph>(
            graph());
    mock_utility_process_ =
        CreateNode<ProcessNodeImpl>(content::PROCESS_TYPE_UTILITY);
    mock_utility_process_->SetProcess(base::Process::Current(),
                                      /*launch_time=*/base::TimeTicks::Now());
    PrepareQueryResults();
    graph()->PassToGraph(std::move(decorator));
  }

  FakeMemoryMeasurementDelegateFactory& GetMemoryDelegate() {
    return memory_delegate_factory_;
  }

  TestProcessMetricsDecorator* decorator() const { return decorator_raw_; }

  MockMultiplePagesAndWorkersWithMultipleProcessesGraph* mock_graph() {
    return mock_graph_.get();
  }

  // After calling this function, the process memory data used by the QueryAPI
  // will be to |kFakeResidentSet|, |kFakePrivateFootprint|, and
  // |kFakePrivateSwap|.
  void PrepareQueryResults() {
    PrepareProcessQueryResults(mock_graph()->process.get(), kFakeResidentSet,
                               kFakePrivateFootprint, kFakePrivateSwap);
    PrepareProcessQueryResults(mock_graph()->other_process.get(),
                               kFakeResidentSet, kFakePrivateFootprint,
                               kFakePrivateSwap);
    PrepareProcessQueryResults(mock_utility_process_.get(), kFakeResidentSet,
                               kFakePrivateFootprint, kFakePrivateSwap);
  }

  void PrepareProcessQueryResults(ProcessNode* process_node,
                                  base::ByteCount resident_set,
                                  base::ByteCount private_footprint,
                                  base::ByteCount private_swap) {
    MemoryMeasurementDelegate::MemorySummaryMap& memory_summaries =
        GetMemoryDelegate().memory_summaries();
    memory_summaries[process_node->GetResourceContext()] = {
        .resident_set_size = resident_set,
        .private_footprint = private_footprint,
        .private_swap = private_swap,
    };
  }

  void ClearPrepareQueryResults() {
    GetMemoryDelegate().memory_summaries().clear();
  }

  void TriggerOtherQueryOnce() {
    ScopedResourceUsageQuery scoped_query =
        QueryBuilder()
            .AddAllContextsOfType<ProcessContext>()
            .AddAllContextsOfType<FrameContext>()
            .AddAllContextsOfType<WorkerContext>()
            .AddResourceType(ResourceType::kMemorySummary)
            .CreateScopedQuery();
    scoped_query.QueryOnce();
  }

  void TriggerOtherQueryTimer() {
    ScopedResourceUsageQuery scoped_query =
        QueryBuilder()
            .AddAllContextsOfType<ProcessContext>()
            .AddAllContextsOfType<FrameContext>()
            .AddAllContextsOfType<WorkerContext>()
            .AddResourceType(ResourceType::kMemorySummary)
            .CreateScopedQuery();
    const base::TimeDelta delay = base::Seconds(1);
    scoped_query.Start(delay);
    task_env().FastForwardBy(delay);
  }

  void ExpectProcessResults(ProcessNode* process_node,
                            base::ByteCount resident_set,
                            base::ByteCount private_footprint,
                            base::ByteCount private_swap) {
    ASSERT_TRUE(process_node);
    auto child_size = process_node->GetFrameNodes().size() +
                      process_node->GetWorkerNodes().size();
    EXPECT_EQ(resident_set, process_node->GetResidentSet());
    EXPECT_EQ(private_footprint, process_node->GetPrivateFootprint());
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
    EXPECT_EQ(private_swap, process_node->GetPrivateSwap());
#endif
    for (const auto* frame : process_node->GetFrameNodes()) {
      EXPECT_EQ(resident_set / child_size, frame->GetResidentSetEstimate());
      EXPECT_EQ(private_footprint / child_size,
                frame->GetPrivateFootprintEstimate());
    }
    for (const auto* worker : process_node->GetWorkerNodes()) {
      EXPECT_EQ(resident_set / child_size, worker->GetResidentSetEstimate());
      EXPECT_EQ(private_footprint / child_size,
                worker->GetPrivateFootprintEstimate());
    }
  }

  void ResetResults() {
    mock_graph()->process->set_resident_set(base::ByteCount(0));
    mock_graph()->process->set_private_footprint(base::ByteCount(0));
    mock_graph()->process->set_private_swap(base::ByteCount(0));
    mock_graph()->frame->SetResidentSetEstimate(base::ByteCount(0));
    mock_graph()->frame->SetPrivateFootprintEstimate(base::ByteCount(0));
    mock_graph()->other_frame->SetResidentSetEstimate(base::ByteCount(0));
    mock_graph()->other_frame->SetPrivateFootprintEstimate(base::ByteCount(0));
    mock_graph()->worker->SetResidentSetEstimate(base::ByteCount(0));
    mock_graph()->worker->SetPrivateFootprintEstimate(base::ByteCount(0));
    mock_graph()->other_process->set_resident_set(base::ByteCount(0));
    mock_graph()->other_process->set_private_footprint(base::ByteCount(0));
    mock_graph()->other_process->set_private_swap(base::ByteCount(0));
    mock_graph()->child_frame->SetResidentSetEstimate(base::ByteCount(0));
    mock_graph()->child_frame->SetPrivateFootprintEstimate(base::ByteCount(0));
    mock_graph()->other_worker->SetResidentSetEstimate(base::ByteCount(0));
    mock_graph()->other_worker->SetPrivateFootprintEstimate(base::ByteCount(0));
    mock_utility_process_->set_resident_set(base::ByteCount(0));
    mock_utility_process_->set_private_footprint(base::ByteCount(0));
    mock_utility_process_->set_private_swap(base::ByteCount(0));
  }

  void ExpectAndResetAllProcessResults(base::ByteCount resident_set,
                                       base::ByteCount private_footprint,
                                       base::ByteCount private_swap) {
    WaitForMetricsUpdated();
    ExpectProcessResults(mock_graph()->process.get(), resident_set,
                         private_footprint, private_swap);
    ExpectProcessResults(mock_graph()->other_process.get(), resident_set,
                         private_footprint, private_swap);
    ExpectProcessResults(mock_utility_process_.get(), resident_set,
                         private_footprint, private_swap);
    ResetResults();
  }

  FakeMemoryMeasurementDelegateFactory memory_delegate_factory_;

  TestNodeWrapper<ProcessNodeImpl> mock_utility_process_;

  void WaitForMetricsUpdated() {
    // QueryResultObserver is thread safe, it notifies results via a
    // posted task, just fast forward by a tiny timeout to ensure the task is
    // run completely.
    // TODO(crbug.com/40755583): Remove this when QueryResultObserver is not
    // thread-safe
    task_env().FastForwardBy(TestTimeouts::tiny_timeout());
  }

 private:
  raw_ptr<TestProcessMetricsDecorator> decorator_raw_;

  std::unique_ptr<MockMultiplePagesAndWorkersWithMultipleProcessesGraph>
      mock_graph_;
};

TEST_F(ProcessMetricsDecoratorTest, UpdateByOtherQueries) {
  // There's no data available initially.
  ExpectAndResetAllProcessResults(base::ByteCount(0), base::ByteCount(0),
                                  base::ByteCount(0));

  auto interest_token =
      ProcessMetricsDecorator::RegisterInterestForProcessMetrics(graph());
  // The first measurement should be taken immediately.
  ExpectAndResetAllProcessResults(kFakeResidentSet, kFakePrivateFootprint,
                                  kFakePrivateSwap);

  TriggerOtherQueryOnce();
  ExpectAndResetAllProcessResults(kFakeResidentSet, kFakePrivateFootprint,
                                  kFakePrivateSwap);

  TriggerOtherQueryTimer();
  ExpectAndResetAllProcessResults(kFakeResidentSet, kFakePrivateFootprint,
                                  kFakePrivateSwap);

  // Do not update results when there are no tokens left.
  interest_token.reset();
  TriggerOtherQueryOnce();
  ExpectAndResetAllProcessResults(base::ByteCount(0), base::ByteCount(0),
                                  base::ByteCount(0));
}

TEST_F(ProcessMetricsDecoratorTest, UpdateByQueryTimer) {
  MockSystemNodeObserver sys_node_observer;

  graph()->AddSystemNodeObserver(&sys_node_observer);

  // There's no data available initially.
  ExpectAndResetAllProcessResults(base::ByteCount(0), base::ByteCount(0),
                                  base::ByteCount(0));

  // The first measurement should be taken immediately.
  EXPECT_CALL(sys_node_observer, OnProcessMemoryMetricsAvailable(_));
  auto interest_token =
      ProcessMetricsDecorator::RegisterInterestForProcessMetrics(graph());
  ExpectAndResetAllProcessResults(kFakeResidentSet, kFakePrivateFootprint,
                                  kFakePrivateSwap);

  // Advance the timer, this should trigger a refresh of the metrics.
  EXPECT_CALL(sys_node_observer, OnProcessMemoryMetricsAvailable(_));
  task_env().FastForwardBy(decorator()->GetTimerDelayForTesting());
  ExpectAndResetAllProcessResults(kFakeResidentSet, kFakePrivateFootprint,
                                  kFakePrivateSwap);

  // Refreshes should stop when there are no tokens left.
  interest_token.reset();
  task_env().FastForwardBy(decorator()->GetTimerDelayForTesting());
  ExpectAndResetAllProcessResults(base::ByteCount(0), base::ByteCount(0),
                                  base::ByteCount(0));

  graph()->RemoveSystemNodeObserver(&sys_node_observer);
}

// If QueryResult only have partial metrics, we should update partially.
TEST_F(ProcessMetricsDecoratorTest, PartialUpdate) {
  // Only contains the data for one of the three processes.
  ClearPrepareQueryResults();
  PrepareProcessQueryResults(mock_graph()->process.get(), kFakeResidentSet,
                             kFakePrivateFootprint, kFakePrivateSwap);
  // The first measurement should be taken immediately.
  auto interest_token =
      ProcessMetricsDecorator::RegisterInterestForProcessMetrics(graph());

  WaitForMetricsUpdated();
  ExpectProcessResults(mock_graph()->process.get(), kFakeResidentSet,
                       kFakePrivateFootprint, kFakePrivateSwap);
  ExpectProcessResults(mock_graph()->other_process.get(), base::ByteCount(0),
                       base::ByteCount(0), base::ByteCount(0));
  ExpectProcessResults(mock_utility_process_.get(), base::ByteCount(0),
                       base::ByteCount(0), base::ByteCount(0));

  // Do another partial refresh but this time for the other process. The
  // data attached to |mock_graph()->process| shouldn't change.
  ClearPrepareQueryResults();
  PrepareProcessQueryResults(mock_graph()->other_process.get(),
                             kFakeResidentSet * 2, kFakePrivateFootprint * 2,
                             kFakePrivateSwap * 2);
  task_env().FastForwardBy(decorator()->GetTimerDelayForTesting());
  WaitForMetricsUpdated();
  ExpectProcessResults(mock_graph()->process.get(), kFakeResidentSet,
                       kFakePrivateFootprint, kFakePrivateSwap);
  ExpectProcessResults(mock_graph()->other_process.get(), kFakeResidentSet * 2,
                       kFakePrivateFootprint * 2, kFakePrivateSwap * 2);
  ExpectProcessResults(mock_utility_process_.get(), base::ByteCount(0),
                       base::ByteCount(0), base::ByteCount(0));
}

TEST_F(ProcessMetricsDecoratorTest, UpdateByImmediateRequest) {
  // There's no data available initially.
  ExpectAndResetAllProcessResults(base::ByteCount(0), base::ByteCount(0),
                                  base::ByteCount(0));

  // The first measurement should be taken immediately.
  auto interest_token =
      ProcessMetricsDecorator::RegisterInterestForProcessMetrics(graph());
  ExpectAndResetAllProcessResults(kFakeResidentSet, kFakePrivateFootprint,
                                  kFakePrivateSwap);

  // Updated again after immediate collection.
  decorator()->RequestImmediateMetrics();
  ExpectAndResetAllProcessResults(kFakeResidentSet, kFakePrivateFootprint,
                                  kFakePrivateSwap);
}

TEST_F(ProcessMetricsDecoratorTest, MetricsInterestTokens) {
  EXPECT_FALSE(decorator()->IsTimerRunningForTesting());

  // The first token created will take a measurement, then start the timer.
  auto metrics_interest_token1 =
      ProcessMetricsDecorator::RegisterInterestForProcessMetrics(graph());
  EXPECT_TRUE(decorator()->IsTimerRunningForTesting());

  auto metrics_interest_token2 =
      ProcessMetricsDecorator::RegisterInterestForProcessMetrics(graph());
  EXPECT_TRUE(decorator()->IsTimerRunningForTesting());

  metrics_interest_token1.reset();
  EXPECT_TRUE(decorator()->IsTimerRunningForTesting());

  metrics_interest_token2.reset();
  EXPECT_FALSE(decorator()->IsTimerRunningForTesting());

  // Creating another token after all are deleted should take another
  // measurement.
  auto metrics_interest_token3 =
      ProcessMetricsDecorator::RegisterInterestForProcessMetrics(graph());
  EXPECT_TRUE(decorator()->IsTimerRunningForTesting());
}

}  // namespace performance_manager
