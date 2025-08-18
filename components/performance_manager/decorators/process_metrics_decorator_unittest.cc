// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/decorators/process_metrics_decorator.h"

#include <memory>

#include "base/byte_count.h"
#include "base/memory/raw_ptr.h"
#include "base/process/process.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/test_support/graph/mock_system_node_observer.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/test_support/mock_graphs.h"
#include "content/public/common/process_type.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/global_memory_dump.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/memory_instrumentation.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

namespace {

using ::testing::_;
using ::testing::ByMove;
using ::testing::Return;
using GlobalMemoryDumpPtr = memory_instrumentation::mojom::GlobalMemoryDumpPtr;

constexpr base::ByteCount kFakeResidentSet = base::KiB(12345);
constexpr base::ByteCount kFakePrivateFootprint = base::KiB(67890);

// Test version of the |ProcessMetricsDecorator| class.
class LenientTestProcessMetricsDecorator : public ProcessMetricsDecorator {
 public:
  LenientTestProcessMetricsDecorator() = default;
  ~LenientTestProcessMetricsDecorator() override = default;

  // Expose RefreshMetrics for unittesting.
  using ProcessMetricsDecorator::RefreshMetrics;

  // ProcessMetricsDecorator:
  void RequestProcessesMemoryMetrics(
      bool immediate_request,
      ProcessMemoryDumpCallback callback) override;

  // Mock method used to set the test expectations. Return `nullptr` to expect a
  // failure.
  MOCK_METHOD(GlobalMemoryDumpPtr, GetMemoryDump, ());
};
using TestProcessMetricsDecorator =
    ::testing::StrictMock<LenientTestProcessMetricsDecorator>;

void LenientTestProcessMetricsDecorator::RequestProcessesMemoryMetrics(
    bool immediate_request,
    ProcessMemoryDumpCallback callback) {
  GlobalMemoryDumpPtr global_dump = GetMemoryDump();
  bool success = !global_dump.is_null();
  std::move(callback).Run(immediate_request, success,
                          memory_instrumentation::GlobalMemoryDump::MoveFrom(
                              std::move(global_dump)));
}

struct MemoryDumpProcInfo {
  base::ProcessId pid;
  base::ByteCount resident_set;
  base::ByteCount private_footprint;
};

// Generate a GlobalMemoryDumpPtr object based on the data contained in
// |proc_info_vec|.
GlobalMemoryDumpPtr GenerateMemoryDump(
    const std::vector<MemoryDumpProcInfo>& proc_info_vec) {
  auto global_dump = memory_instrumentation::mojom::GlobalMemoryDump::New();
  for (const auto& proc_info : proc_info_vec) {
    auto pmd = memory_instrumentation::mojom::ProcessMemoryDump::New();
    pmd->pid = proc_info.pid;
    pmd->os_dump = memory_instrumentation::mojom::OSMemDump::New();
    pmd->os_dump->resident_set_kb = proc_info.resident_set.InKiB();
    pmd->os_dump->private_footprint_kb = proc_info.private_footprint.InKiB();
    global_dump->process_dumps.emplace_back(std::move(pmd));
  }
  return global_dump;
}

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
    Super::SetUp();
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

    EXPECT_FALSE(decorator_raw_->IsTimerRunningForTesting());
    graph()->PassToGraph(std::move(decorator));
    EXPECT_FALSE(decorator_raw_->IsTimerRunningForTesting());
  }

  TestProcessMetricsDecorator* decorator() const { return decorator_raw_; }

  MockMultiplePagesAndWorkersWithMultipleProcessesGraph* mock_graph() {
    return mock_graph_.get();
  }

  // Returns a default GlobalMemoryDumpPtr suitable for most tests.
  GlobalMemoryDumpPtr DefaultMemoryDump() {
    return GenerateMemoryDump({
        {mock_graph()->process->GetProcessId(), kFakeResidentSet,
         kFakePrivateFootprint},
        {mock_graph()->other_process->GetProcessId(), kFakeResidentSet,
         kFakePrivateFootprint},
        {mock_utility_process_->GetProcessId(), kFakeResidentSet,
         kFakePrivateFootprint},
    });
  }

  void ExpectProcessResults(base::ByteCount resident_set,
                            base::ByteCount private_footprint) {
    EXPECT_EQ(resident_set, mock_graph()->process->GetResidentSet());
    EXPECT_EQ(private_footprint, mock_graph()->process->GetPrivateFootprint());

    EXPECT_EQ(resident_set / 3, mock_graph()->frame->GetResidentSetEstimate());
    EXPECT_EQ(private_footprint / 3,
              mock_graph()->frame->GetPrivateFootprintEstimate());
    EXPECT_EQ(resident_set / 3,
              mock_graph()->other_frame->GetResidentSetEstimate());
    EXPECT_EQ(private_footprint / 3,
              mock_graph()->other_frame->GetPrivateFootprintEstimate());
    EXPECT_EQ(resident_set / 3, mock_graph()->worker->GetResidentSetEstimate());
    EXPECT_EQ(private_footprint / 3,
              mock_graph()->worker->GetPrivateFootprintEstimate());
  }

  void ExpectOtherProcessResults(base::ByteCount resident_set,
                                 base::ByteCount private_footprint) {
    EXPECT_EQ(resident_set, mock_graph()->other_process->GetResidentSet());
    EXPECT_EQ(private_footprint,
              mock_graph()->other_process->GetPrivateFootprint());

    EXPECT_EQ(resident_set / 2,
              mock_graph()->child_frame->GetResidentSetEstimate());
    EXPECT_EQ(private_footprint / 2,
              mock_graph()->child_frame->GetPrivateFootprintEstimate());
    EXPECT_EQ(resident_set / 2,
              mock_graph()->other_worker->GetResidentSetEstimate());
    EXPECT_EQ(private_footprint / 2,
              mock_graph()->other_worker->GetPrivateFootprintEstimate());
  }

  void ExpectUtilityProcessResults(base::ByteCount resident_set,
                                   base::ByteCount private_footprint) {
    EXPECT_EQ(resident_set, mock_utility_process_->GetResidentSet());
    EXPECT_EQ(private_footprint, mock_utility_process_->GetPrivateFootprint());
    // No frames or workers to measure.
  }

  void ResetResults() {
    mock_graph()->process->set_resident_set(base::ByteCount(0));
    mock_graph()->process->set_private_footprint(base::ByteCount(0));
    mock_graph()->frame->SetResidentSetEstimate(base::ByteCount(0));
    mock_graph()->frame->SetPrivateFootprintEstimate(base::ByteCount(0));
    mock_graph()->other_frame->SetResidentSetEstimate(base::ByteCount(0));
    mock_graph()->other_frame->SetPrivateFootprintEstimate(base::ByteCount(0));
    mock_graph()->worker->SetResidentSetEstimate(base::ByteCount(0));
    mock_graph()->worker->SetPrivateFootprintEstimate(base::ByteCount(0));
    mock_graph()->other_process->set_resident_set(base::ByteCount(0));
    mock_graph()->other_process->set_private_footprint(base::ByteCount(0));
    mock_graph()->child_frame->SetResidentSetEstimate(base::ByteCount(0));
    mock_graph()->child_frame->SetPrivateFootprintEstimate(base::ByteCount(0));
    mock_graph()->other_worker->SetResidentSetEstimate(base::ByteCount(0));
    mock_graph()->other_worker->SetPrivateFootprintEstimate(base::ByteCount(0));
    mock_utility_process_->set_resident_set(base::ByteCount(0));
    mock_utility_process_->set_private_footprint(base::ByteCount(0));
  }

  void ExpectAndResetAllProcessResults(base::ByteCount resident_set,
                                       base::ByteCount private_footprint) {
    ExpectProcessResults(resident_set, private_footprint);
    ExpectOtherProcessResults(resident_set, private_footprint);
    ExpectUtilityProcessResults(resident_set, private_footprint);
    ResetResults();
  }

  TestNodeWrapper<ProcessNodeImpl> mock_utility_process_;

 private:
  raw_ptr<TestProcessMetricsDecorator> decorator_raw_;

  std::unique_ptr<MockMultiplePagesAndWorkersWithMultipleProcessesGraph>
      mock_graph_;
};

TEST_F(ProcessMetricsDecoratorTest, RefreshTimer) {
  MockSystemNodeObserver sys_node_observer;

  graph()->AddSystemNodeObserver(&sys_node_observer);

  // There's no data available initially.
  ExpectAndResetAllProcessResults(base::ByteCount(0), base::ByteCount(0));

  // The first measurement should be taken immediately.
  EXPECT_CALL(*decorator(), GetMemoryDump())
      .WillOnce(Return(ByMove(DefaultMemoryDump())));
  EXPECT_CALL(sys_node_observer, OnProcessMemoryMetricsAvailable(_));

  auto interest_token =
      ProcessMetricsDecorator::RegisterInterestForProcessMetrics(graph());
  ExpectAndResetAllProcessResults(kFakeResidentSet, kFakePrivateFootprint);

  // Advance the timer, this should trigger a refresh of the metrics.
  EXPECT_CALL(*decorator(), GetMemoryDump())
      .WillOnce(Return(ByMove(DefaultMemoryDump())));
  EXPECT_CALL(sys_node_observer, OnProcessMemoryMetricsAvailable(_));

  task_env().FastForwardBy(decorator()->GetTimerDelayForTesting());
  ExpectAndResetAllProcessResults(kFakeResidentSet, kFakePrivateFootprint);

  // Requesting an immediate measurement partway through the timer period should
  // reset the timer.
  base::TimeDelta delay = decorator()->GetTimerDelayForTesting();

  EXPECT_CALL(*decorator(), GetMemoryDump())
      .WillOnce(Return(ByMove(DefaultMemoryDump())));
  EXPECT_CALL(sys_node_observer, OnProcessMemoryMetricsAvailable(_));

  task_env().FastForwardBy(delay / 2);
  decorator()->RequestImmediateMetrics();
  ExpectAndResetAllProcessResults(kFakeResidentSet, kFakePrivateFootprint);

  // Timer should not fire again until the full `delay` passes.
  task_env().FastForwardBy(delay / 2);
  ExpectAndResetAllProcessResults(base::ByteCount(0), base::ByteCount(0));

  EXPECT_CALL(*decorator(), GetMemoryDump())
      .WillOnce(Return(ByMove(DefaultMemoryDump())));
  EXPECT_CALL(sys_node_observer, OnProcessMemoryMetricsAvailable(_));

  task_env().FastForwardBy(delay / 2);
  ExpectAndResetAllProcessResults(kFakeResidentSet, kFakePrivateFootprint);

  // Refreshes should stop when there are no tokens left.
  interest_token.reset();
  task_env().FastForwardBy(decorator()->GetTimerDelayForTesting());
  ExpectAndResetAllProcessResults(base::ByteCount(0), base::ByteCount(0));

  graph()->RemoveSystemNodeObserver(&sys_node_observer);
}

TEST_F(ProcessMetricsDecoratorTest, PartialRefresh) {
  // Only contains the data for one of the three processes.
  EXPECT_CALL(*decorator(), GetMemoryDump())
      .WillOnce(Return(ByMove(
          GenerateMemoryDump({{mock_graph()->process->GetProcessId(),
                               kFakeResidentSet, kFakePrivateFootprint}}))));

  auto interest_token =
      ProcessMetricsDecorator::RegisterInterestForProcessMetrics(graph());

  ExpectProcessResults(kFakeResidentSet, kFakePrivateFootprint);
  ExpectOtherProcessResults(base::ByteCount(0), base::ByteCount(0));
  ExpectUtilityProcessResults(base::ByteCount(0), base::ByteCount(0));

  // Do another partial refresh but this time for the other process. The
  // data attached to |mock_graph()->process| shouldn't change.
  EXPECT_CALL(*decorator(), GetMemoryDump())
      .WillOnce(Return(ByMove(GenerateMemoryDump(
          {{mock_graph()->other_process->GetProcessId(), kFakeResidentSet * 2,
            kFakePrivateFootprint * 2}}))));

  task_env().FastForwardBy(decorator()->GetTimerDelayForTesting());

  ExpectProcessResults(kFakeResidentSet, kFakePrivateFootprint);
  ExpectOtherProcessResults(kFakeResidentSet * 2, kFakePrivateFootprint * 2);
  ExpectUtilityProcessResults(base::ByteCount(0), base::ByteCount(0));
}

TEST_F(ProcessMetricsDecoratorTest, RefreshFailure) {
  EXPECT_CALL(*decorator(), GetMemoryDump()).WillOnce(Return(ByMove(nullptr)));

  auto interest_token =
      ProcessMetricsDecorator::RegisterInterestForProcessMetrics(graph());

  ExpectAndResetAllProcessResults(base::ByteCount(0), base::ByteCount(0));

  // A failure shouldn't stop the next refresh.
  EXPECT_CALL(*decorator(), GetMemoryDump())
      .WillOnce(Return(ByMove(GenerateMemoryDump({
          {mock_graph()->process->GetProcessId(), kFakeResidentSet,
           kFakePrivateFootprint},
          {mock_graph()->other_process->GetProcessId(), kFakeResidentSet,
           kFakePrivateFootprint},
          {mock_utility_process_->GetProcessId(), kFakeResidentSet,
           kFakePrivateFootprint},
      }))));

  task_env().FastForwardBy(decorator()->GetTimerDelayForTesting());

  ExpectAndResetAllProcessResults(kFakeResidentSet, kFakePrivateFootprint);
}

TEST_F(ProcessMetricsDecoratorTest, ImmediateRequestThrottling) {
  // There's no data available initially.
  ExpectAndResetAllProcessResults(base::ByteCount(0), base::ByteCount(0));

  // The first measurement should be taken immediately.
  EXPECT_CALL(*decorator(), GetMemoryDump())
      .WillOnce(Return(ByMove(DefaultMemoryDump())));
  auto interest_token =
      ProcessMetricsDecorator::RegisterInterestForProcessMetrics(graph());
  ExpectAndResetAllProcessResults(kFakeResidentSet, kFakePrivateFootprint);

  // Immediate measurements should be available immediately after timed
  // measurements.
  EXPECT_CALL(*decorator(), GetMemoryDump())
      .WillOnce(Return(ByMove(DefaultMemoryDump())));
  decorator()->RequestImmediateMetrics();
  ExpectAndResetAllProcessResults(kFakeResidentSet, kFakePrivateFootprint);

  EXPECT_CALL(*decorator(), GetMemoryDump())
      .WillOnce(Return(ByMove(DefaultMemoryDump())));
  task_env().FastForwardBy(decorator()->GetTimerDelayForTesting());
  ExpectAndResetAllProcessResults(kFakeResidentSet, kFakePrivateFootprint);

  EXPECT_CALL(*decorator(), GetMemoryDump())
      .WillOnce(Return(ByMove(DefaultMemoryDump())));
  decorator()->RequestImmediateMetrics();
  ExpectAndResetAllProcessResults(kFakeResidentSet, kFakePrivateFootprint);

  // Calls to RequestImmediateMetrics should be throttled to
  // kMinImmediateRefreshDelay.
  constexpr base::TimeDelta kMinDelay =
      ProcessMetricsDecorator::kMinImmediateRefreshDelay;
  task_env().FastForwardBy(kMinDelay / 2);
  decorator()->RequestImmediateMetrics();
  ExpectAndResetAllProcessResults(base::ByteCount(0), base::ByteCount(0));

  // After the min delay, RequestImmediateMetrics() causes a refresh.
  EXPECT_CALL(*decorator(), GetMemoryDump())
      .WillOnce(Return(ByMove(DefaultMemoryDump())));
  task_env().FastForwardBy(kMinDelay / 2);
  decorator()->RequestImmediateMetrics();
  ExpectAndResetAllProcessResults(kFakeResidentSet, kFakePrivateFootprint);

  // Throttling should compare the request time to the last *successful*
  // immediate refresh, to be sure it measures the freshness of valid data.
  EXPECT_CALL(*decorator(), GetMemoryDump()).WillOnce(Return(ByMove(nullptr)));
  task_env().FastForwardBy(kMinDelay);
  decorator()->RequestImmediateMetrics();
  ExpectAndResetAllProcessResults(
      base::ByteCount(0), base::ByteCount(0));  // Failed to get results.

  EXPECT_CALL(*decorator(), GetMemoryDump())
      .WillOnce(Return(ByMove(DefaultMemoryDump())));
  task_env().FastForwardBy(kMinDelay / 2);
  decorator()->RequestImmediateMetrics();
  ExpectAndResetAllProcessResults(kFakeResidentSet, kFakePrivateFootprint);

  // Requesting an immediate measurement while a measurement is already in
  // progress should do nothing. (If this fails, GetMemoryDump() will be invoked
  // multiple times.)
  EXPECT_CALL(*decorator(), GetMemoryDump()).WillOnce([&] {
    decorator()->RequestImmediateMetrics();
    return DefaultMemoryDump();
  });
  task_env().FastForwardBy(decorator()->GetTimerDelayForTesting());
  ExpectAndResetAllProcessResults(kFakeResidentSet, kFakePrivateFootprint);
}

TEST_F(ProcessMetricsDecoratorTest, MetricsInterestTokens) {
  EXPECT_FALSE(decorator()->IsTimerRunningForTesting());

  // The first token created will take a measurement, then start the timer.
  EXPECT_CALL(*decorator(), GetMemoryDump()).WillOnce(Return(ByMove(nullptr)));
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
  EXPECT_CALL(*decorator(), GetMemoryDump()).WillOnce(Return(ByMove(nullptr)));
  auto metrics_interest_token3 =
      ProcessMetricsDecorator::RegisterInterestForProcessMetrics(graph());
}

}  // namespace performance_manager
