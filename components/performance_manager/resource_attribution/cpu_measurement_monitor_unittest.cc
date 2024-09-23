// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/resource_attribution/cpu_measurement_monitor.h"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/process/kill.h"
#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/test/test_waitable_event.h"
#include "base/time/time.h"
#include "components/performance_manager/embedder/graph_features.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/graph/worker_node_impl.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/public/resource_attribution/cpu_measurement_delegate.h"
#include "components/performance_manager/public/resource_attribution/cpu_proportion_tracker.h"
#include "components/performance_manager/public/resource_attribution/origin_in_browsing_instance_context.h"
#include "components/performance_manager/public/resource_attribution/query_results.h"
#include "components/performance_manager/public/resource_attribution/resource_contexts.h"
#include "components/performance_manager/resource_attribution/performance_manager_aliases.h"
#include "components/performance_manager/resource_attribution/query_params.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/test_support/mock_graphs.h"
#include "components/performance_manager/test_support/performance_manager_test_harness.h"
#include "components/performance_manager/test_support/resource_attribution/gtest_util.h"
#include "components/performance_manager/test_support/resource_attribution/measurement_delegates.h"
#include "components/performance_manager/test_support/run_in_graph.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/process_type.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/navigation_simulator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace resource_attribution {

namespace {

using ::testing::AllOf;
using ::testing::Conditional;
using ::testing::Contains;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::Pair;

using performance_manager::kBrowsingInstanceForOtherPage;
using performance_manager::kBrowsingInstanceForPage;
using performance_manager::TestNodeWrapper;
using performance_manager::features::kResourceAttributionIncludeOrigins;
using ProcessCPUUsageError = CPUMeasurementDelegate::ProcessCPUUsageError;

constexpr base::TimeDelta kTimeBetweenMeasurements = base::Minutes(5);

// Creates a stub WorkerNode hosted in the given `process_node`, with the given
// `origin`, and adds it to `graph`.
TestNodeWrapper<WorkerNodeImpl> CreateWorkerNodeWithOrigin(
    GraphImpl* graph,
    ProcessNodeImpl* process_node,
    const url::Origin& origin) {
  return TestNodeWrapper<WorkerNodeImpl>::Create(
      graph, WorkerNode::WorkerType::kDedicated, process_node,
      /*browser_context_id=*/std::string(), blink::DedicatedWorkerToken(),
      origin);
}

// Like MockMultiplePagesAndWorkersWithMultipleProcessesGraph (see
// mock_graphs.h), but assigns a fixed origin to each WorkerNode.
struct MockMultiplePagesAndWorkersWithKnownOriginsGraph
    : public performance_manager::MockMultiplePagesWithMultipleProcessesGraph {
  // Creates a graph with the same structure as
  // MockMultiplePagesAndWorkersWithMultipleProcessesGraph, assigning `origin`
  // to `worker` and `other_origin` to `other_worker`.
  MockMultiplePagesAndWorkersWithKnownOriginsGraph(
      performance_manager::TestGraphImpl* graph,
      const url::Origin& origin,
      const url::Origin& other_origin)
      : performance_manager::MockMultiplePagesWithMultipleProcessesGraph(graph),
        worker(CreateWorkerNodeWithOrigin(graph, process.get(), origin)),
        other_worker(CreateWorkerNodeWithOrigin(graph,
                                                other_process.get(),
                                                other_origin)) {
    worker->AddClientFrame(frame.get());
    other_worker->AddClientFrame(child_frame.get());
  }

  ~MockMultiplePagesAndWorkersWithKnownOriginsGraph() {
    other_worker->RemoveClientFrame(child_frame.get());
    worker->RemoveClientFrame(frame.get());
  }

  TestNodeWrapper<WorkerNodeImpl> worker;
  TestNodeWrapper<WorkerNodeImpl> other_worker;
};

constexpr internal::QueryId kQueryId = internal::QueryId::FromUnsafeValue(1);
constexpr internal::QueryId kOtherQueryId =
    internal::QueryId::FromUnsafeValue(2);

}  // namespace

// A test that creates mock processes to simulate exact CPU usage.
class ResourceAttrCPUMonitorTest
    : public performance_manager::GraphTestHarness {
 protected:
  using Super = performance_manager::GraphTestHarness;

  ResourceAttrCPUMonitorTest() {
    scoped_feature_list_.InitAndEnableFeature(
        kResourceAttributionIncludeOrigins);
  }

  void SetUp() override {
    GetGraphFeatures().EnableResourceAttributionScheduler();
    Super::SetUp();
    // These tests validate specific timing of measurements around process
    // creation and destruction.
    delegate_factory_.SetRequireValidProcesses(true);
    cpu_monitor_.SetDelegateFactoryForTesting(&delegate_factory_);
  }

  // Creates a renderer process and starts mocking its CPU measurements. By
  // default the process will use 100% CPU as long as it's alive.
  TestNodeWrapper<ProcessNodeImpl> CreateMockCPURenderer() {
    auto process_node = CreateRendererProcessNode();
    SetProcessCPUUsage(process_node.get(), 1.0);
    return process_node;
  }

  // Creates a process of type `process_type` and starts mocking its CPU
  // measurements. By default the process will use 100% CPU as long as it's
  // alive.
  TestNodeWrapper<ProcessNodeImpl> CreateMockCPUProcess(
      content::ProcessType process_type) {
    if (process_type == content::PROCESS_TYPE_RENDERER) {
      return CreateMockCPURenderer();
    }
    auto process_node = (process_type == content::PROCESS_TYPE_BROWSER)
                            ? CreateBrowserProcessNode()
                            : CreateBrowserChildProcessNode(process_type);
    SetProcessCPUUsage(process_node.get(), 1.0);
    return process_node;
  }

  void SetProcessId(ProcessNodeImpl* process_node) {
    // Assigns the current process object to the node, including its pid.
    process_node->SetProcess(base::Process::Current(), base::TimeTicks::Now());
  }

  void SetProcessExited(ProcessNodeImpl* process_node) {
    process_node->SetProcessExitStatus(0);
    // After a process exits, GetCumulativeCPUUsage() starts returning an error.
    SetProcessCPUUsageError(process_node,
                            ProcessCPUUsageError::kProcessNotFound);
  }

  void SetProcessCPUUsage(const ProcessNodeImpl* process_node, double usage) {
    delegate_factory_.GetDelegate(process_node).SetCPUUsage(usage);
  }

  void SetProcessCPUUsageError(const ProcessNodeImpl* process_node,
                               std::optional<ProcessCPUUsageError> error) {
    delegate_factory_.GetDelegate(process_node).SetError(std::move(error));
  }

  // Calls StartMonitoring() on the CPUMeasurementMonitor under test, and
  // clears any cached results.
  void StartMonitoring() {
    last_measurements_ = {};
    current_measurements_ = {};
    cpu_monitor_.StartMonitoring(graph());
  }

  // Calls UpdateAndGetCPUMeasurements() on the CPUMeasurementMonitor under
  // test, and caches the results.
  void UpdateAndGetCPUMeasurements(
      std::optional<internal::QueryId> query_id = std::nullopt) {
    last_measurements_ = current_measurements_;
    current_measurements_ = cpu_monitor_.UpdateAndGetCPUMeasurements(query_id);
  }

  // Helper to get the most recent output of `cpu_monitor_` and convert to a
  // QueryResultMap which CPUProportionTracker expects.
  QueryResultMap GetCPUQueryResults(
      std::optional<internal::QueryId> query_id = std::nullopt) {
    QueryResultMap results;
    for (const auto& [context, cpu_time_result] :
         cpu_monitor_.UpdateAndGetCPUMeasurements(query_id)) {
      results[context] = QueryResults{cpu_time_result};
    }
    return results;
  }

  // GMock matcher expecting that a given QueryResults object contains a
  // CPUTimeResult with cumulative_cpu `last_measurements_[context] +
  // expected_delta`. That is, since the last time `context` was tested, expect
  // that `expected_delta` was added to its CPU measurement, which was taken at
  // `expected_measurement_time`. `expected_algorithm` is the measurement
  // algorithm that should be used, which defaults to the algorithm used for
  // processes (kDirectMeasurement).
  auto CPUDeltaMatchesWithMeasurementTime(
      const ResourceContext& context,
      base::TimeDelta expected_delta,
      base::TimeDelta expected_background_delta,
      base::TimeTicks expected_measurement_time,
      MeasurementAlgorithm expected_algorithm =
          MeasurementAlgorithm::kDirectMeasurement) const {
    base::TimeDelta expected_cpu = expected_delta;
    base::TimeDelta expected_background_cpu = expected_background_delta;
    base::TimeTicks expected_start_time;
    const auto last_it = last_measurements_.find(context);
    if (last_it != last_measurements_.end()) {
      expected_cpu += last_it->second.cpu_time_result->cumulative_cpu;
      expected_background_cpu +=
          last_it->second.cpu_time_result->cumulative_background_cpu;
      expected_start_time = last_it->second.cpu_time_result->start_time;
    }
    return QueryResultsMatch<CPUTimeResult>(AllOf(
        Field("cumulative_cpu", &CPUTimeResult::cumulative_cpu, expected_cpu),
        Field("cumulative_background_cpu",
              &CPUTimeResult::cumulative_background_cpu,
              expected_background_cpu),
        // `start_time` should not change. If this was the first measurement,
        // allow any non-null `start_time`. Note Conditional() doesn't
        // short-circuit, so the first branch will always be evaluated and can't
        // dereference `last_it`, which is why `expected_start_time` is put in a
        // temporary.
        Field("start_time", &CPUTimeResult::start_time,
              Conditional(last_it != last_measurements_.end(),
                          expected_start_time, Not(base::TimeTicks()))),
        ResultMetadataMatches<CPUTimeResult>(expected_measurement_time,
                                             expected_algorithm)));
  }

  // As CPUDeltaMatchesWithMeasurementTime, but assumes the mock clock hasn't
  // advanced since the measurement (so the measurement time is "now").
  auto CPUDeltaWithBackgroundMatches(
      const ResourceContext& context,
      base::TimeDelta expected_delta,
      base::TimeDelta expected_background_delta,
      MeasurementAlgorithm expected_algorithm =
          MeasurementAlgorithm::kDirectMeasurement) const {
    return CPUDeltaMatchesWithMeasurementTime(
        context, expected_delta, expected_background_delta,
        /*expected_measurement_time=*/base::TimeTicks::Now(),
        expected_algorithm);
  }

  // As CPUDeltaWithBackgroundMatches, but expects no background CPU time.
  auto CPUDeltaMatches(const ResourceContext& context,
                       base::TimeDelta expected_delta,
                       MeasurementAlgorithm expected_algorithm =
                           MeasurementAlgorithm::kDirectMeasurement) const {
    return CPUDeltaWithBackgroundMatches(
        context, expected_delta,
        /* expected_background_delta=*/base::TimeDelta(), expected_algorithm);
  }

  // GMock matcher expecting that a given QueryResults object contains a
  // CPUTimeResult with the given `expected_start_time`.
  auto StartTimeMatches(base::TimeTicks expected_start_time) const {
    return QueryResultsMatch<CPUTimeResult>(
        Field("start_time", &CPUTimeResult::start_time, expected_start_time));
  }

  base::test::ScopedFeatureList scoped_feature_list_;

  // URLs and corresponding origins used in tests.
  const GURL kUrl1 = GURL("http://a.com");
  const GURL kUrl2 = GURL("http://b.com");
  const url::Origin kOrigin1 = url::Origin::Create(kUrl1);
  const url::Origin kOrigin2 = url::Origin::Create(kUrl2);

  // Factory to return CPUMeasurementDelegates for `cpu_monitor_`. This must be
  // created before `cpu_monitor_` and deleted afterward to ensure that it
  // outlives all delegates it creates.
  SimulatedCPUMeasurementDelegateFactory delegate_factory_;

  // The object under test.
  CPUMeasurementMonitor cpu_monitor_;

  // Cached results from UpdateAndGetCPUMeasurements(). Most tests will validate
  // the difference between the "last" and "current" measurements, which is
  // easier to follow than the full cumulative measurements at any given time.
  QueryResultMap last_measurements_;
  QueryResultMap current_measurements_;
};

// Tests that renderers created at various points around CPU measurement
// snapshots are handled correctly.
TEST_F(ResourceAttrCPUMonitorTest, CreateTiming) {
  // Renderer in existence before StartMonitoring().
  const TestNodeWrapper<ProcessNodeImpl> renderer1 = CreateMockCPURenderer();
  SetProcessId(renderer1.get());

  // Renderer starts and exits before StartMonitoring().
  const TestNodeWrapper<ProcessNodeImpl> early_exit_renderer =
      CreateMockCPURenderer();
  SetProcessId(early_exit_renderer.get());

  // Advance the clock before monitoring starts, so that the process launch
  // times can be distinguished from the start of monitoring.
  task_env().FastForwardBy(kTimeBetweenMeasurements);
  SetProcessExited(early_exit_renderer.get());

  // Renderer creation racing with StartMonitoring(). Its pid will not be
  // available until after monitoring starts.
  const TestNodeWrapper<ProcessNodeImpl> renderer2 = CreateMockCPURenderer();
  ASSERT_EQ(renderer2->GetProcessId(), base::kNullProcessId);

  // `renderer1` begins measurement as soon as StartMonitoring is called.
  // `renderer2` begins measurement when its pid is available.
  StartMonitoring();
  const auto renderer1_start_time = base::TimeTicks::Now();

  task_env().FastForwardBy(kTimeBetweenMeasurements / 2);
  SetProcessId(renderer2.get());
  const auto renderer2_start_time = base::TimeTicks::Now();

  // Renderer created halfway through the measurement interval.
  const TestNodeWrapper<ProcessNodeImpl> renderer3 = CreateMockCPURenderer();
  SetProcessId(renderer3.get());
  const auto renderer3_start_time = base::TimeTicks::Now();

  // Renderer creation racing with UpdateAndGetCPUMeasurements(). `renderer4`'s
  // pid will become available on the same tick the measurement is taken,
  // `renderer5`'s pid will become available after the measurement.
  const TestNodeWrapper<ProcessNodeImpl> renderer4 = CreateMockCPURenderer();
  const TestNodeWrapper<ProcessNodeImpl> renderer5 = CreateMockCPURenderer();

  // Finish next measurement interval.
  task_env().FastForwardBy(kTimeBetweenMeasurements / 2);
  SetProcessId(renderer4.get());
  const auto renderer4_start_time = base::TimeTicks::Now();

  // `renderer1` existed for the entire measurement period. The CPU it used
  // before StartMonitoring() was called is ignored.
  // `renderer2` existed for all of it, but was only measured for the last half,
  // after its pid became available.
  // `renderer3` only existed for the last half.
  // `renderer4` existed for the measurement but no time passed so it had 0% CPU
  // usage.
  // `renderer5` is not measured yet.
  UpdateAndGetCPUMeasurements();

  EXPECT_FALSE(base::Contains(current_measurements_,
                              early_exit_renderer->GetResourceContext()));
  EXPECT_THAT(current_measurements_[renderer1->GetResourceContext()],
              AllOf(CPUDeltaMatches(renderer1->GetResourceContext(),
                                    kTimeBetweenMeasurements),
                    StartTimeMatches(renderer1_start_time)));
  EXPECT_THAT(current_measurements_[renderer2->GetResourceContext()],
              AllOf(CPUDeltaMatches(renderer2->GetResourceContext(),
                                    kTimeBetweenMeasurements / 2),
                    StartTimeMatches(renderer2_start_time)));
  EXPECT_THAT(current_measurements_[renderer3->GetResourceContext()],
              AllOf(CPUDeltaMatches(renderer3->GetResourceContext(),
                                    kTimeBetweenMeasurements / 2),
                    StartTimeMatches(renderer3_start_time)));
  EXPECT_FALSE(
      base::Contains(current_measurements_, renderer4->GetResourceContext()));
  EXPECT_FALSE(
      base::Contains(current_measurements_, renderer5->GetResourceContext()));

  SetProcessId(renderer5.get());
  const auto renderer5_start_time = base::TimeTicks::Now();

  // Finish next measurement interval.
  task_env().FastForwardBy(kTimeBetweenMeasurements);

  // All nodes existed for entire measurement interval.
  UpdateAndGetCPUMeasurements();

  EXPECT_THAT(current_measurements_[renderer1->GetResourceContext()],
              CPUDeltaMatches(renderer1->GetResourceContext(),
                              kTimeBetweenMeasurements));
  EXPECT_THAT(current_measurements_[renderer2->GetResourceContext()],
              CPUDeltaMatches(renderer2->GetResourceContext(),
                              kTimeBetweenMeasurements));
  EXPECT_THAT(current_measurements_[renderer3->GetResourceContext()],
              CPUDeltaMatches(renderer3->GetResourceContext(),
                              kTimeBetweenMeasurements));
  EXPECT_THAT(current_measurements_[renderer4->GetResourceContext()],
              AllOf(CPUDeltaMatches(renderer4->GetResourceContext(),
                                    kTimeBetweenMeasurements),
                    StartTimeMatches(renderer4_start_time)));
  EXPECT_THAT(current_measurements_[renderer5->GetResourceContext()],
              AllOf(CPUDeltaMatches(renderer5->GetResourceContext(),
                                    kTimeBetweenMeasurements),
                    StartTimeMatches(renderer5_start_time)));
}

// Tests that renderers exiting at various points around CPU measurement
// snapshots are handled correctly.
TEST_F(ResourceAttrCPUMonitorTest, ExitTiming) {
  const TestNodeWrapper<ProcessNodeImpl> renderer1 = CreateMockCPURenderer();
  SetProcessId(renderer1.get());
  const TestNodeWrapper<ProcessNodeImpl> renderer2 = CreateMockCPURenderer();
  SetProcessId(renderer2.get());
  const TestNodeWrapper<ProcessNodeImpl> renderer3 = CreateMockCPURenderer();
  SetProcessId(renderer3.get());
  const TestNodeWrapper<ProcessNodeImpl> renderer4 = CreateMockCPURenderer();
  SetProcessId(renderer4.get());
  const TestNodeWrapper<ProcessNodeImpl> renderer5 = CreateMockCPURenderer();
  SetProcessId(renderer5.get());
  const TestNodeWrapper<ProcessNodeImpl> renderer6 = CreateMockCPURenderer();
  SetProcessId(renderer6.get());
  const TestNodeWrapper<ProcessNodeImpl> renderer7 = CreateMockCPURenderer();
  SetProcessId(renderer7.get());
  const TestNodeWrapper<ProcessNodeImpl> renderer8 = CreateMockCPURenderer();
  SetProcessId(renderer8.get());

  StartMonitoring();

  // Test renderers that exit before UpdateAndGetCPUMeasurements is ever called:
  // `renderer1` exits at the beginning of the first measurement interval.
  // `renderer2` exits halfway through.
  // `renderer3` exits at the end of the interval.
  SetProcessExited(renderer1.get());
  task_env().FastForwardBy(kTimeBetweenMeasurements / 2);
  SetProcessExited(renderer2.get());

  // Finish the measurement interval.
  task_env().FastForwardBy(kTimeBetweenMeasurements / 2);
  SetProcessExited(renderer3.get());

  UpdateAndGetCPUMeasurements();
  const auto previous_update_time = base::TimeTicks::Now();

  // Renderers that have exited were never measured.
  EXPECT_FALSE(
      base::Contains(current_measurements_, renderer1->GetResourceContext()));
  EXPECT_FALSE(
      base::Contains(current_measurements_, renderer2->GetResourceContext()));
  EXPECT_FALSE(
      base::Contains(current_measurements_, renderer3->GetResourceContext()));

  // Remaining renderers are using 100% CPU.
  EXPECT_THAT(current_measurements_[renderer4->GetResourceContext()],
              CPUDeltaMatches(renderer4->GetResourceContext(),
                              kTimeBetweenMeasurements));
  EXPECT_THAT(current_measurements_[renderer5->GetResourceContext()],
              CPUDeltaMatches(renderer5->GetResourceContext(),
                              kTimeBetweenMeasurements));
  EXPECT_THAT(current_measurements_[renderer6->GetResourceContext()],
              CPUDeltaMatches(renderer6->GetResourceContext(),
                              kTimeBetweenMeasurements));
  EXPECT_THAT(current_measurements_[renderer7->GetResourceContext()],
              CPUDeltaMatches(renderer7->GetResourceContext(),
                              kTimeBetweenMeasurements));
  EXPECT_THAT(current_measurements_[renderer8->GetResourceContext()],
              CPUDeltaMatches(renderer8->GetResourceContext(),
                              kTimeBetweenMeasurements));

  // `renderer4` exits at the beginning of the next measurement interval.
  // `renderer5` exits halfway through.
  // `renderer6` exits at the end of the interval.
  SetProcessExited(renderer4.get());
  task_env().FastForwardBy(kTimeBetweenMeasurements / 2);
  SetProcessExited(renderer5.get());

  // Finish the measurement interval.
  task_env().FastForwardBy(kTimeBetweenMeasurements / 2);
  SetProcessExited(renderer6.get());

  // TODO(crbug.com/40889748): Processes that exited at any point during the
  // interval still return their last measurement before the interval, so
  // their delta is always empty. Capture the final CPU usage correctly, and
  // test that the renderers that have exited return their CPU usage for the
  // time they were alive and 0% for the rest of the measurement interval.
  UpdateAndGetCPUMeasurements();

  EXPECT_THAT(
      current_measurements_[renderer4->GetResourceContext()],
      CPUDeltaMatchesWithMeasurementTime(
          renderer4->GetResourceContext(), /*expected_delta=*/base::TimeDelta(),
          /*expected_background_delta=*/base::TimeDelta(),
          previous_update_time));
  EXPECT_THAT(
      current_measurements_[renderer5->GetResourceContext()],
      CPUDeltaMatchesWithMeasurementTime(
          renderer5->GetResourceContext(), /*expected_delta=*/base::TimeDelta(),
          /*expected_background_delta=*/base::TimeDelta(),
          previous_update_time));
  EXPECT_THAT(
      current_measurements_[renderer6->GetResourceContext()],
      CPUDeltaMatchesWithMeasurementTime(
          renderer6->GetResourceContext(), /*expected_delta=*/base::TimeDelta(),
          /*expected_background_delta=*/base::TimeDelta(),
          previous_update_time));

  EXPECT_THAT(current_measurements_[renderer7->GetResourceContext()],
              CPUDeltaMatches(renderer7->GetResourceContext(),
                              kTimeBetweenMeasurements));
  EXPECT_THAT(current_measurements_[renderer8->GetResourceContext()],
              CPUDeltaMatches(renderer8->GetResourceContext(),
                              kTimeBetweenMeasurements));

  // `renderer7` exits just before the StopMonitoring call and `renderer7`
  // exits just after. This should not cause any assertion failures.
  SetProcessExited(renderer7.get());
  cpu_monitor_.StopMonitoring();
  SetProcessExited(renderer8.get());
}

// Tests that varying CPU usage between measurement snapshots is reported
// correctly.
TEST_F(ResourceAttrCPUMonitorTest, VaryingMeasurements) {
  const TestNodeWrapper<ProcessNodeImpl> renderer1 = CreateMockCPURenderer();
  SetProcessId(renderer1.get());
  const TestNodeWrapper<ProcessNodeImpl> renderer2 = CreateMockCPURenderer();
  SetProcessId(renderer2.get());
  const TestNodeWrapper<ProcessNodeImpl> renderer3 = CreateMockCPURenderer();
  SetProcessId(renderer3.get());
  const TestNodeWrapper<ProcessNodeImpl> renderer4 = CreateMockCPURenderer();
  SetProcessId(renderer4.get());

  StartMonitoring();

  // All processes are at 100% for first measurement interval.
  task_env().FastForwardBy(kTimeBetweenMeasurements);
  UpdateAndGetCPUMeasurements();

  EXPECT_THAT(current_measurements_[renderer1->GetResourceContext()],
              CPUDeltaMatches(renderer1->GetResourceContext(),
                              kTimeBetweenMeasurements));
  EXPECT_THAT(current_measurements_[renderer2->GetResourceContext()],
              CPUDeltaMatches(renderer2->GetResourceContext(),
                              kTimeBetweenMeasurements));
  EXPECT_THAT(current_measurements_[renderer3->GetResourceContext()],
              CPUDeltaMatches(renderer3->GetResourceContext(),
                              kTimeBetweenMeasurements));
  EXPECT_THAT(current_measurements_[renderer4->GetResourceContext()],
              CPUDeltaMatches(renderer4->GetResourceContext(),
                              kTimeBetweenMeasurements));

  // `renderer1` drops to 50% CPU usage for the next period.
  // `renderer2` stays at 100% for the first half, 50% for the last half
  // (average 75%).
  // `renderer3` drops to 0% for a time, returns to 100% for half that time,
  // then drops to 0% again (average 25%).
  // `renderer4` drops to 0% at the end of the period. It should still show 100%
  // since no time passes before measuring.
  SetProcessCPUUsage(renderer1.get(), 0.5);
  SetProcessCPUUsage(renderer3.get(), 0.0);
  task_env().FastForwardBy(kTimeBetweenMeasurements / 2);
  SetProcessCPUUsage(renderer2.get(), 0.5);
  SetProcessCPUUsage(renderer3.get(), 1.0);
  task_env().FastForwardBy(kTimeBetweenMeasurements / 4);
  SetProcessCPUUsage(renderer3.get(), 0);

  // Finish next measurement interval.
  task_env().FastForwardBy(kTimeBetweenMeasurements / 4);
  SetProcessCPUUsage(renderer4.get(), 0);

  UpdateAndGetCPUMeasurements();

  EXPECT_THAT(current_measurements_[renderer1->GetResourceContext()],
              CPUDeltaMatches(renderer1->GetResourceContext(),
                              kTimeBetweenMeasurements * 0.5));
  EXPECT_THAT(current_measurements_[renderer2->GetResourceContext()],
              CPUDeltaMatches(renderer2->GetResourceContext(),
                              kTimeBetweenMeasurements * 0.75));
  EXPECT_THAT(current_measurements_[renderer3->GetResourceContext()],
              CPUDeltaMatches(renderer3->GetResourceContext(),
                              kTimeBetweenMeasurements * 0.25));
  EXPECT_THAT(current_measurements_[renderer4->GetResourceContext()],
              CPUDeltaMatches(renderer4->GetResourceContext(),
                              kTimeBetweenMeasurements));
}

// Tests that CPU usage of non-renderers is measured.
TEST_F(ResourceAttrCPUMonitorTest, AllProcessTypes) {
  const std::vector<content::ProcessType> kProcessTypes{
      content::PROCESS_TYPE_BROWSER,        content::PROCESS_TYPE_RENDERER,
      content::PROCESS_TYPE_UTILITY,        content::PROCESS_TYPE_ZYGOTE,
      content::PROCESS_TYPE_SANDBOX_HELPER, content::PROCESS_TYPE_GPU};

  std::map<content::ProcessType, TestNodeWrapper<ProcessNodeImpl>>
      process_nodes;
  std::map<content::ProcessType, double> expected_cpu_percent;

  double next_cpu_percent = 0.9;
  for (const auto process_type : kProcessTypes) {
    auto process = CreateMockCPUProcess(process_type);
    SetProcessId(process.get());
    SetProcessCPUUsage(process.get(), next_cpu_percent);
    process_nodes[process_type] = std::move(process);
    expected_cpu_percent[process_type] = next_cpu_percent;
    next_cpu_percent -= 0.1;
  }

  StartMonitoring();

  task_env().FastForwardBy(kTimeBetweenMeasurements);
  UpdateAndGetCPUMeasurements();
  for (const auto process_type : kProcessTypes) {
    const ProcessContext& process_context =
        process_nodes.at(process_type)->GetResourceContext();
    EXPECT_THAT(current_measurements_[process_context],
                CPUDeltaMatches(process_context,
                                kTimeBetweenMeasurements *
                                    expected_cpu_percent.at(process_type)))
        << "process type " << process_type;
  }
}

// Tests that CPU usage of processes is correctly distributed between frames and
// workers in those processes, and correctly aggregated to pages containing
// frames and workers from multiple processes.
TEST_F(ResourceAttrCPUMonitorTest, CPUDistribution) {
  MockMultiplePagesAndWorkersWithKnownOriginsGraph mock_graph(graph(), kOrigin1,
                                                              kOrigin2);

  // Assign URL's to frames in the graph so that they'll be mapped to
  // OriginInBrowsingInstanceContexts.
  mock_graph.frame->OnNavigationCommitted(
      kUrl1, kOrigin1,
      /*same_document=*/false, /*is_served_from_back_forward_cache=*/false);
  mock_graph.other_frame->OnNavigationCommitted(
      kUrl2, kOrigin2,
      /*same_document=*/false, /*is_served_from_back_forward_cache=*/false);
  mock_graph.child_frame->OnNavigationCommitted(
      kUrl1, kOrigin1,
      /*same_document=*/false, /*is_served_from_back_forward_cache=*/false);

  // The mock browser and utility processes should be measured, but do not
  // contain frames or workers so should not affect the distribution of
  // measurements.
  SetProcessCPUUsage(mock_graph.browser_process.get(), 0.8);

  const TestNodeWrapper<ProcessNodeImpl> utility_process =
      CreateMockCPUProcess(content::PROCESS_TYPE_UTILITY);
  SetProcessId(utility_process.get());
  SetProcessCPUUsage(utility_process.get(), 0.7);

  SetProcessCPUUsage(mock_graph.process.get(), 0.6);
  SetProcessCPUUsage(mock_graph.other_process.get(), 0.5);

  StartMonitoring();
  const auto monitoring_start_time = base::TimeTicks::Now();

  // No measurements if no time has passed.
  UpdateAndGetCPUMeasurements();
  EXPECT_THAT(current_measurements_, IsEmpty());

  task_env().FastForwardBy(kTimeBetweenMeasurements);

  UpdateAndGetCPUMeasurements();

  const FrameContext& frame_context = mock_graph.frame->GetResourceContext();
  const FrameContext& child_frame_context =
      mock_graph.child_frame->GetResourceContext();
  const FrameContext& other_frame_context =
      mock_graph.other_frame->GetResourceContext();
  const PageContext& page_context = mock_graph.page->GetResourceContext();
  const PageContext& other_page_context =
      mock_graph.other_page->GetResourceContext();
  const WorkerContext& worker_context = mock_graph.worker->GetResourceContext();
  const WorkerContext& other_worker_context =
      mock_graph.other_worker->GetResourceContext();
  const ProcessContext& browser_process_context =
      mock_graph.browser_process->GetResourceContext();
  const ProcessContext& utility_process_context =
      utility_process->GetResourceContext();
  const ProcessContext& process_context =
      mock_graph.process->GetResourceContext();
  const ProcessContext& other_process_context =
      mock_graph.other_process->GetResourceContext();

  const auto origin1_in_bi_context =
      OriginInBrowsingInstanceContext(kOrigin1, kBrowsingInstanceForPage);
  const auto origin2_in_bi_context =
      OriginInBrowsingInstanceContext(kOrigin2, kBrowsingInstanceForPage);
  const auto origin1_in_other_bi_context =
      OriginInBrowsingInstanceContext(kOrigin1, kBrowsingInstanceForOtherPage);
  const auto origin2_in_other_bi_context =
      OriginInBrowsingInstanceContext(kOrigin2, kBrowsingInstanceForOtherPage);

  EXPECT_THAT(current_measurements_[browser_process_context],
              AllOf(CPUDeltaMatches(browser_process_context,
                                    kTimeBetweenMeasurements * 0.8),
                    StartTimeMatches(monitoring_start_time)));
  EXPECT_THAT(current_measurements_[utility_process_context],
              AllOf(CPUDeltaMatches(utility_process_context,
                                    kTimeBetweenMeasurements * 0.7),
                    StartTimeMatches(monitoring_start_time)));

  // * `process` splits its 60% CPU usage evenly between `frame`, `other_frame`
  //   and `worker`.
  // * `other_process` splits its 50% CPU usage evenly between `child_frame` and
  //   `other_worker`.
  // See the chart in MockMultiplePagesAndWorkersWithMultipleProcessesGraph.
  base::TimeDelta split_process_cpu_delta = kTimeBetweenMeasurements * 0.2;
  base::TimeDelta other_process_split_cpu_delta =
      kTimeBetweenMeasurements * 0.25;

  EXPECT_THAT(
      current_measurements_[process_context],
      AllOf(CPUDeltaMatches(process_context, kTimeBetweenMeasurements * 0.6),
            StartTimeMatches(monitoring_start_time)));
  EXPECT_THAT(current_measurements_[other_process_context],
              AllOf(CPUDeltaMatches(other_process_context,
                                    kTimeBetweenMeasurements * 0.5),
                    StartTimeMatches(monitoring_start_time)));

  EXPECT_THAT(current_measurements_[frame_context],
              AllOf(CPUDeltaMatches(frame_context, split_process_cpu_delta,
                                    MeasurementAlgorithm::kSplit),
                    StartTimeMatches(monitoring_start_time)));
  EXPECT_THAT(
      current_measurements_[other_frame_context],
      AllOf(CPUDeltaMatches(other_frame_context, split_process_cpu_delta,
                            MeasurementAlgorithm::kSplit),
            StartTimeMatches(monitoring_start_time)));
  EXPECT_THAT(current_measurements_[worker_context],
              AllOf(CPUDeltaMatches(worker_context, split_process_cpu_delta,
                                    MeasurementAlgorithm::kSplit),
                    StartTimeMatches(monitoring_start_time)));

  EXPECT_THAT(
      current_measurements_[child_frame_context],
      AllOf(CPUDeltaMatches(child_frame_context, other_process_split_cpu_delta,
                            MeasurementAlgorithm::kSplit),
            StartTimeMatches(monitoring_start_time)));
  EXPECT_THAT(
      current_measurements_[other_worker_context],
      AllOf(CPUDeltaMatches(other_worker_context, other_process_split_cpu_delta,
                            MeasurementAlgorithm::kSplit),
            StartTimeMatches(monitoring_start_time)));

  // * `page` gets its CPU usage from the sum of `frame` and `worker`.
  // * `other_page` gets the sum of `other_frame`, `child_frame` and
  //   `other_worker`.
  // See the chart in MockMultiplePagesAndWorkersWithMultipleProcessesGraph.
  EXPECT_THAT(
      current_measurements_[page_context],
      AllOf(CPUDeltaMatches(page_context, kTimeBetweenMeasurements * 0.4,
                            MeasurementAlgorithm::kSum),
            StartTimeMatches(monitoring_start_time)));
  EXPECT_THAT(
      current_measurements_[other_page_context],
      AllOf(CPUDeltaMatches(other_page_context, kTimeBetweenMeasurements * 0.7,
                            MeasurementAlgorithm::kSum),
            StartTimeMatches(monitoring_start_time)));

  // See the chart in MockMultiplePagesAndWorkersWithMultipleProcessesGraph
  // that maps each frame and worker to `process` or `other_process`.
  auto expect_origin_in_bi_measurements =
      [&](base::TimeDelta split_process_cpu_delta,
          base::TimeDelta split_other_process_cpu_delta) {
        // `origin1_in_bi_context` gets the sum of `frame` and
        // `worker`, both in `page` with http://a.com. Both are hosted in
        // `process`.
        EXPECT_THAT(current_measurements_[origin1_in_bi_context],
                    AllOf(CPUDeltaMatches(origin1_in_bi_context,
                                          2 * split_process_cpu_delta,
                                          MeasurementAlgorithm::kSum),
                          StartTimeMatches(monitoring_start_time)));

        // `origin2_in_bi_context` has nothing, since nothing in
        // `page` is from http://b.com.
        EXPECT_FALSE(
            base::Contains(current_measurements_, origin2_in_bi_context));

        // `origin1_in_other_bi_context` equals `child_frame`,
        // the only thing in `other_page` from http://a.com. It's hosted in
        // `other_process`.
        EXPECT_THAT(current_measurements_[origin1_in_other_bi_context],
                    AllOf(CPUDeltaMatches(origin1_in_other_bi_context,
                                          split_other_process_cpu_delta,
                                          MeasurementAlgorithm::kSum),
                          StartTimeMatches(monitoring_start_time)));

        // `origin2_in_other_bi_context` gets the sum of
        // `other_frame` (hosted in `process`) and `other_worker` (hosted in
        // `other_process`), both in `other_page` with http://b.com.
        EXPECT_THAT(current_measurements_[origin2_in_other_bi_context],
                    AllOf(CPUDeltaMatches(origin2_in_other_bi_context,
                                          split_process_cpu_delta +
                                              split_other_process_cpu_delta,
                                          MeasurementAlgorithm::kSum),
                          StartTimeMatches(monitoring_start_time)));
      };
  expect_origin_in_bi_measurements(split_process_cpu_delta,
                                   other_process_split_cpu_delta);

  // Modify the CPU usage of each renderer process, ensure all frames and
  // workers are updated.
  SetProcessCPUUsage(mock_graph.process.get(), 0.3);
  SetProcessCPUUsage(mock_graph.other_process.get(), 0.8);
  task_env().FastForwardBy(kTimeBetweenMeasurements);

  UpdateAndGetCPUMeasurements();

  // * `process` splits its 30% CPU usage evenly between `frame`, `other_frame`
  //   and `worker`.
  // * `other_process` splits its 80% CPU usage evenly between `child_frame` and
  //   `other_worker`.
  split_process_cpu_delta = kTimeBetweenMeasurements * 0.1;
  other_process_split_cpu_delta = kTimeBetweenMeasurements * 0.4;

  EXPECT_THAT(current_measurements_[process_context],
              CPUDeltaMatches(process_context, kTimeBetweenMeasurements * 0.3));
  EXPECT_THAT(
      current_measurements_[other_process_context],
      CPUDeltaMatches(other_process_context, kTimeBetweenMeasurements * 0.8));

  EXPECT_THAT(current_measurements_[frame_context],
              CPUDeltaMatches(frame_context, split_process_cpu_delta,
                              MeasurementAlgorithm::kSplit));
  EXPECT_THAT(current_measurements_[other_frame_context],
              CPUDeltaMatches(other_frame_context, split_process_cpu_delta,
                              MeasurementAlgorithm::kSplit));
  EXPECT_THAT(current_measurements_[worker_context],
              CPUDeltaMatches(worker_context, split_process_cpu_delta,
                              MeasurementAlgorithm::kSplit));

  EXPECT_THAT(
      current_measurements_[child_frame_context],
      CPUDeltaMatches(child_frame_context, other_process_split_cpu_delta,
                      MeasurementAlgorithm::kSplit));
  EXPECT_THAT(
      current_measurements_[other_worker_context],
      CPUDeltaMatches(other_worker_context, other_process_split_cpu_delta,
                      MeasurementAlgorithm::kSplit));

  // * `page` gets its CPU usage from the sum of `frame` and `worker`.
  // * `other_page` gets the sum of `other_frame`, `child_frame` and
  //   `other_worker`.
  EXPECT_THAT(current_measurements_[page_context],
              CPUDeltaMatches(page_context, kTimeBetweenMeasurements * 0.2,
                              MeasurementAlgorithm::kSum));
  EXPECT_THAT(
      current_measurements_[other_page_context],
      CPUDeltaMatches(other_page_context, kTimeBetweenMeasurements * 0.9,
                      MeasurementAlgorithm::kSum));

  expect_origin_in_bi_measurements(split_process_cpu_delta,
                                   other_process_split_cpu_delta);

  // Drop CPU usage of `other_process` to 0%. Only advance part of the normal
  // measurement interval, to be sure that the percentage usage doesn't depend
  // on the length of the interval.
  constexpr base::TimeDelta kShortInterval = kTimeBetweenMeasurements / 3;
  SetProcessCPUUsage(mock_graph.other_process.get(), 0.0);
  task_env().FastForwardBy(kShortInterval);

  UpdateAndGetCPUMeasurements();

  // * `process` splits its 30% CPU usage evenly between `frame`, `other_frame`
  //   and `worker`.
  // * `other_process` splits its 0% CPU usage evenly between `child_frame` and
  //   `other_worker`.
  split_process_cpu_delta = kShortInterval * 0.1;
  other_process_split_cpu_delta = base::TimeDelta();

  EXPECT_THAT(current_measurements_[process_context],
              CPUDeltaMatches(process_context, kShortInterval * 0.3));
  EXPECT_THAT(current_measurements_[other_process_context],
              CPUDeltaMatches(other_process_context, base::TimeDelta()));

  EXPECT_THAT(current_measurements_[frame_context],
              CPUDeltaMatches(frame_context, split_process_cpu_delta,
                              MeasurementAlgorithm::kSplit));
  EXPECT_THAT(current_measurements_[other_frame_context],
              CPUDeltaMatches(other_frame_context, split_process_cpu_delta,
                              MeasurementAlgorithm::kSplit));
  EXPECT_THAT(current_measurements_[worker_context],
              CPUDeltaMatches(worker_context, split_process_cpu_delta,
                              MeasurementAlgorithm::kSplit));

  EXPECT_THAT(
      current_measurements_[child_frame_context],
      CPUDeltaMatches(child_frame_context, other_process_split_cpu_delta,
                      MeasurementAlgorithm::kSplit));
  EXPECT_THAT(
      current_measurements_[other_worker_context],
      CPUDeltaMatches(other_worker_context, other_process_split_cpu_delta,
                      MeasurementAlgorithm::kSplit));

  // * `page` gets its CPU usage from the sum of `frame` and `worker`.
  // * `other_page` gets the sum of `other_frame`, `child_frame` and
  //   `other_worker`.
  EXPECT_THAT(current_measurements_[page_context],
              CPUDeltaMatches(page_context, kShortInterval * 0.2,
                              MeasurementAlgorithm::kSum));
  EXPECT_THAT(current_measurements_[other_page_context],
              CPUDeltaMatches(other_page_context, kShortInterval * 0.1,
                              MeasurementAlgorithm::kSum));

  expect_origin_in_bi_measurements(split_process_cpu_delta,
                                   other_process_split_cpu_delta);
}

// Tests that CPU usage of processes is correctly distributed between FrameNodes
// and WorkerNodes that are added and removed between measurements.
TEST_F(ResourceAttrCPUMonitorTest, AddRemoveNodes) {
  MockMultiplePagesAndWorkersWithKnownOriginsGraph mock_graph(graph(), kOrigin1,
                                                              kOrigin2);

  // Assign URL's to frames in the graph so that they'll be mapped to
  // OriginInBrowsingInstanceContexts.
  mock_graph.frame->OnNavigationCommitted(
      kUrl1, kOrigin1,
      /*same_document=*/false, /*is_served_from_back_forward_cache=*/false);
  mock_graph.other_frame->OnNavigationCommitted(
      kUrl2, kOrigin2,
      /*same_document=*/false, /*is_served_from_back_forward_cache=*/false);
  mock_graph.child_frame->OnNavigationCommitted(
      kUrl1, kOrigin1,
      /*same_document=*/false, /*is_served_from_back_forward_cache=*/false);

  SetProcessCPUUsage(mock_graph.process.get(), 0.6);
  SetProcessCPUUsage(mock_graph.other_process.get(), 0.5);

  // Advance the clock before monitoring starts, so that the process launch
  // times can be distinguished from the start of monitoring.
  task_env().FastForwardBy(kTimeBetweenMeasurements);

  StartMonitoring();

  // Assign results to a repeating query so that they're not dropped immediately
  // when nodes are removed.
  constexpr internal::QueryId kDummyQuery;
  cpu_monitor_.RepeatingQueryStarted(kDummyQuery);

  const FrameContext& frame_context = mock_graph.frame->GetResourceContext();
  const FrameContext& child_frame_context =
      mock_graph.child_frame->GetResourceContext();
  const PageContext& page_context = mock_graph.page->GetResourceContext();
  const PageContext& other_page_context =
      mock_graph.other_page->GetResourceContext();
  const ProcessContext& process_context =
      mock_graph.process->GetResourceContext();
  const ProcessContext& other_process_context =
      mock_graph.other_process->GetResourceContext();

  const auto origin1_in_bi_context =
      OriginInBrowsingInstanceContext(kOrigin1, kBrowsingInstanceForPage);
  const auto origin2_in_bi_context =
      OriginInBrowsingInstanceContext(kOrigin2, kBrowsingInstanceForPage);
  const auto origin1_in_other_bi_context =
      OriginInBrowsingInstanceContext(kOrigin1, kBrowsingInstanceForOtherPage);
  const auto origin2_in_other_bi_context =
      OriginInBrowsingInstanceContext(kOrigin2, kBrowsingInstanceForOtherPage);

  // `new_frame1` and `new_worker1` are added just after a measurement.
  // `new_frame2` and `new_worker2` are added between measurements.
  // `new_frame3` and `new_worker3` are added just before a measurement.
  //
  // Frames are added to `process` and workers are added to `other_process`, to
  // test that all processes are measured.
  //
  // New frames are part of `page`. New workers don't have clients, so aren't
  // part of any page.
  auto new_frame1 = CreateFrameNodeAutoId(
      mock_graph.process.get(), mock_graph.page.get(),
      /*parent_frame_node=*/nullptr, kBrowsingInstanceForPage);
  new_frame1->OnNavigationCommitted(
      kUrl1, kOrigin1, /*same_document=*/false,
      /*is_served_from_back_forward_cache=*/false);
  auto new_worker1 = CreateWorkerNodeWithOrigin(
      graph(), mock_graph.other_process.get(), kOrigin1);
  const auto new_frame1_context = new_frame1->GetResourceContext();
  const auto new_worker1_context = new_worker1->GetResourceContext();
  const auto node_added_time1 = base::TimeTicks::Now();

  task_env().FastForwardBy(kTimeBetweenMeasurements / 2);
  auto new_frame2 = CreateFrameNodeAutoId(
      mock_graph.process.get(), mock_graph.page.get(),
      /*parent_frame_node=*/nullptr, kBrowsingInstanceForPage);
  new_frame2->OnNavigationCommitted(
      kUrl2, kOrigin2, /*same_document=*/false,
      /*is_served_from_back_forward_cache=*/false);
  auto new_worker2 = CreateWorkerNodeWithOrigin(
      graph(), mock_graph.other_process.get(), kOrigin2);
  const auto new_frame2_context = new_frame2->GetResourceContext();
  const auto new_worker2_context = new_worker2->GetResourceContext();
  const auto node_added_time2 = base::TimeTicks::Now();

  task_env().FastForwardBy(kTimeBetweenMeasurements / 2);
  auto new_frame3 = CreateFrameNodeAutoId(
      mock_graph.process.get(), mock_graph.page.get(),
      /*parent_frame_node=*/nullptr, kBrowsingInstanceForPage);
  new_frame3->OnNavigationCommitted(
      kUrl2, kOrigin2, /*same_document=*/false,
      /*is_served_from_back_forward_cache=*/false);
  auto new_worker3 = CreateWorkerNodeWithOrigin(
      graph(), mock_graph.other_process.get(), kOrigin2);
  const auto new_frame3_context = new_frame3->GetResourceContext();
  const auto new_worker3_context = new_worker3->GetResourceContext();
  const auto node_added_time3 = base::TimeTicks::Now();

  UpdateAndGetCPUMeasurements(kDummyQuery);

  // For the first half of the period:
  //
  // * `process` split its 60% CPU usage between 4 nodes:
  //   * `frame`, `other_frame`, `worker`, `new_frame1`
  //
  //   * `frame`, `worker` and `new_frame1` are part of `page`.
  //   * `other_frame` is part of `other_page`.
  //   * `frame`, `worker` and `new_frame1` are part of `origin1_in_bi`.
  //   * `other_frame` is part of `origin2_in_other_bi`.
  //
  // * `other_process` splits its 50% CPU usage between 3 nodes:
  //   * `child_frame`, `other_worker`, `new_worker1`
  //
  //   * `child_frame` and `other_worker` are part of `other_page`.
  //   * `child_frame` is part of `origin1_in_other_bi`.
  //   * `other_worker` is part of `origin2_in_other_bi`.
  //
  // For the last half the split is:
  //
  // * `process` splits between 5 nodes:
  //   * `frame`, `other_frame`, `worker`, `new_frame1`, `new_frame2`
  //
  //   * `frame`, `worker`, `new_frame1` and `new_frame2` are part of `page`.
  //   * `other_frame` is part of `other_page`.
  //   * `frame`, `worker` and `new_frame1` are part of `origin1_in_bi`.
  //   * `new_frame2` is part of `origin2_in_bi`.
  //   * `other_frame` is part of `origin2_in_other_bi`.
  //
  // * `other_process` splits between 4 nodes:
  //   * `child_frame`, `other_worker`, `new_worker1`, `new_worker2`
  //
  //   * `child_frame` and `other_worker` are part of `other_page`.
  //   * `child_frame` is part of `origin1_in_other_bi`.
  //   * `other_worker` is part of `origin2_in_other_bi`.
  //
  // `new_frame3` and `new_worker3` were added on the same tick as the
  // measurement so don't contribute to CPU usage.
  constexpr base::TimeDelta process_4way_split =
      kTimeBetweenMeasurements / 2 * 0.6 / 4;
  constexpr base::TimeDelta process_5way_split =
      kTimeBetweenMeasurements / 2 * 0.6 / 5;
  constexpr base::TimeDelta other_process_3way_split =
      kTimeBetweenMeasurements / 2 * 0.5 / 3;
  constexpr base::TimeDelta other_process_4way_split =
      kTimeBetweenMeasurements / 2 * 0.5 / 4;

  constexpr base::TimeDelta expected_page_delta =
      /*first half, 3 nodes*/ 3 * process_4way_split +
      /*second half, 4 nodes*/ 4 * process_5way_split;

  constexpr base::TimeDelta expected_origin1_in_bi_delta =
      /*first half, 3 nodes*/ 3 * process_4way_split +
      /*second half, 3 nodes*/ 3 * process_5way_split;
  constexpr base::TimeDelta expected_origin1_in_other_bi_delta =
      /*first half, 1 node*/ other_process_3way_split +
      /*second half, 1 node*/ other_process_4way_split;
  constexpr base::TimeDelta expected_origin2_in_bi_delta =
      /*first half, 0 nodes*/ base::TimeDelta() +
      /*second half, 1 node*/ process_5way_split;
  constexpr base::TimeDelta expected_origin2_in_other_bi_delta =
      /*first half, 2 nodes*/ process_4way_split + other_process_3way_split +
      /*second half, 2 nodes*/ process_5way_split + other_process_4way_split;

  EXPECT_THAT(current_measurements_[process_context],
              CPUDeltaMatches(process_context, kTimeBetweenMeasurements * 0.6));
  EXPECT_THAT(
      current_measurements_[frame_context],
      CPUDeltaMatches(frame_context, process_4way_split + process_5way_split,
                      MeasurementAlgorithm::kSplit));
  EXPECT_THAT(current_measurements_[new_frame1_context],
              AllOf(CPUDeltaMatches(new_frame1_context,
                                    process_4way_split + process_5way_split,
                                    MeasurementAlgorithm::kSplit),
                    StartTimeMatches(node_added_time1)));
  EXPECT_THAT(current_measurements_[new_frame2_context],
              AllOf(CPUDeltaMatches(new_frame2_context, process_5way_split,
                                    MeasurementAlgorithm::kSplit),
                    StartTimeMatches(node_added_time2)));
  EXPECT_FALSE(base::Contains(current_measurements_, new_frame3_context));

  EXPECT_THAT(
      current_measurements_[other_process_context],
      CPUDeltaMatches(other_process_context, kTimeBetweenMeasurements * 0.5));
  EXPECT_THAT(
      current_measurements_[child_frame_context],
      CPUDeltaMatches(child_frame_context,
                      other_process_3way_split + other_process_4way_split,
                      MeasurementAlgorithm::kSplit));
  EXPECT_THAT(
      current_measurements_[new_worker1_context],
      AllOf(CPUDeltaMatches(new_worker1_context,
                            other_process_3way_split + other_process_4way_split,
                            MeasurementAlgorithm::kSplit),
            StartTimeMatches(node_added_time1)));
  EXPECT_THAT(
      current_measurements_[new_worker2_context],
      AllOf(CPUDeltaMatches(new_worker2_context, other_process_4way_split,
                            MeasurementAlgorithm::kSplit),
            StartTimeMatches(node_added_time2)));
  EXPECT_FALSE(base::Contains(current_measurements_, new_worker3_context));

  EXPECT_THAT(current_measurements_[page_context],
              CPUDeltaMatches(page_context, expected_page_delta,
                              MeasurementAlgorithm::kSum));

  EXPECT_THAT(
      current_measurements_[origin1_in_bi_context],
      CPUDeltaMatches(origin1_in_bi_context, expected_origin1_in_bi_delta,
                      MeasurementAlgorithm::kSum));
  EXPECT_THAT(
      current_measurements_[origin2_in_bi_context],
      CPUDeltaMatches(origin2_in_bi_context, expected_origin2_in_bi_delta,
                      MeasurementAlgorithm::kSum));
  EXPECT_THAT(current_measurements_[origin1_in_other_bi_context],
              CPUDeltaMatches(origin1_in_other_bi_context,
                              expected_origin1_in_other_bi_delta,
                              MeasurementAlgorithm::kSum));
  EXPECT_THAT(current_measurements_[origin2_in_other_bi_context],
              CPUDeltaMatches(origin2_in_other_bi_context,
                              expected_origin2_in_other_bi_delta,
                              MeasurementAlgorithm::kSum));

  new_frame1.reset();
  new_worker1.reset();
  const auto node_removed_time1 = base::TimeTicks::Now();

  task_env().FastForwardBy(kTimeBetweenMeasurements / 2);
  new_frame2.reset();
  new_worker2.reset();
  const auto node_removed_time2 = base::TimeTicks::Now();

  task_env().FastForwardBy(kTimeBetweenMeasurements / 2);
  UpdateAndGetCPUMeasurements(kDummyQuery);

  // `new_frame1` and `new_worker1` were removed on the same tick as the
  // previous measurement, so don't contribute to CPU usage since then.
  //
  // For the first half of this period:
  //
  // * `process` split its 60% CPU usage between 5 nodes:
  //   * `frame`, `other_frame`, `worker`, `new_frame2`, `new_frame3`
  //
  //   * `frame`, `worker`, `new_frame2` and `new_frame3` are part of `page`
  //   * `other_frame` is part of `other_page`.
  //   * `frame` and `worker` are part of `origin1_in_bi`.
  //   * `new_frame2` and `new_frame3` are part of `origin2_in_bi`.
  //   * `other_frame` is part of `origin2_in_other_bi`.
  //
  // * `other_process` splits its 50% CPU usage between 4 nodes:
  //   * `child_frame`, `other_worker`, `new_worker2`, `new_worker3`
  //
  //   * `child_frame` and `other_worker` are part of `other_page`.
  //   * `child_frame` is part of `origin1_in_other_bi`.
  //   * `other_worker` is part of `origin2_in_other_bi`.
  //
  // For the last half the split is:
  //
  // * `process` splits between 4 nodes:
  //   * `frame`, `other_frame`, `worker`, `new_frame3`
  //
  //   * `frame`, `worker` and `new_frame3` are part of `page`
  //   * `other_frame` is part of `other_page`.
  //   * `frame` and `worker` are part of `origin1_in_bi`.
  //   * `new_frame3` is part of `origin2_in_bi`.
  //   * `other_frame` is part of `origin2_in_other_bi`.
  //
  // * `other_process` splits between 3 nodes:
  //   * `child_frame`, `other_worker`, `new_worker3`
  //
  //   * `child_frame` and `other_worker` are part of `other_page`.
  //   * `child_frame` is part of `origin1_in_other_bi`.
  //   * `other_worker` is part of `origin2_in_other_bi`.

  constexpr base::TimeDelta expected_page_delta2 =
      /*first half, 4 nodes*/ 4 * process_5way_split +
      /*second half, 3 nodes*/ 3 * process_4way_split;
  constexpr base::TimeDelta expected_origin1_in_bi_delta2 =
      /*first half, 2 nodes*/ 2 * process_5way_split +
      /*second half, 2 nodes*/ 2 * process_4way_split;
  constexpr base::TimeDelta expected_origin1_in_other_bi_delta2 =
      /*first half, 1 node*/ other_process_4way_split +
      /*second half, 1 node*/ other_process_3way_split;
  constexpr base::TimeDelta expected_origin2_in_bi_delta2 =
      /*first half, 2 nodes*/ 2 * process_5way_split +
      /*second half, 1 nodes*/ 1 * process_4way_split;
  constexpr base::TimeDelta expected_origin2_in_other_bi_delta2 =
      /*first half, 2 nodes*/ process_5way_split + other_process_4way_split +
      /*second half, 2 nodes*/ process_4way_split + other_process_3way_split;

  EXPECT_THAT(current_measurements_[process_context],
              CPUDeltaMatches(process_context, kTimeBetweenMeasurements * 0.6));
  EXPECT_THAT(
      current_measurements_[frame_context],
      CPUDeltaMatches(frame_context, process_5way_split + process_4way_split,
                      MeasurementAlgorithm::kSplit));
  EXPECT_THAT(current_measurements_[new_frame1_context],
              CPUDeltaMatchesWithMeasurementTime(
                  new_frame1_context, /*expected_delta=*/base::TimeDelta(),
                  /*expected_background_delta=*/base::TimeDelta(),
                  node_removed_time1, MeasurementAlgorithm::kSplit));
  EXPECT_THAT(current_measurements_[new_frame2_context],
              CPUDeltaMatchesWithMeasurementTime(
                  new_frame2_context, /*expected_delta=*/process_5way_split,
                  /*expected_background_delta=*/base::TimeDelta(),
                  node_removed_time2, MeasurementAlgorithm::kSplit));
  EXPECT_THAT(current_measurements_[new_frame3_context],
              AllOf(CPUDeltaMatches(new_frame3_context,
                                    process_5way_split + process_4way_split,
                                    MeasurementAlgorithm::kSplit),
                    StartTimeMatches(node_added_time3)));

  EXPECT_THAT(
      current_measurements_[other_process_context],
      CPUDeltaMatches(other_process_context, kTimeBetweenMeasurements * 0.5));
  EXPECT_THAT(
      current_measurements_[child_frame_context],
      CPUDeltaMatches(child_frame_context,
                      other_process_4way_split + other_process_3way_split,
                      MeasurementAlgorithm::kSplit));
  EXPECT_THAT(current_measurements_[new_worker1_context],
              CPUDeltaMatchesWithMeasurementTime(
                  new_worker1_context, /*expected_delta=*/base::TimeDelta(),
                  /*expected_background_delta=*/base::TimeDelta(),
                  node_removed_time1, MeasurementAlgorithm::kSplit));
  EXPECT_THAT(
      current_measurements_[new_worker2_context],
      CPUDeltaMatchesWithMeasurementTime(
          new_worker2_context, /*expected_delta=*/other_process_4way_split,
          /*expected_background_delta=*/base::TimeDelta(), node_removed_time2,
          MeasurementAlgorithm::kSplit));
  EXPECT_THAT(
      current_measurements_[new_worker3_context],
      AllOf(CPUDeltaMatches(new_worker3_context,
                            other_process_4way_split + other_process_3way_split,
                            MeasurementAlgorithm::kSplit),
            StartTimeMatches(node_added_time3)));

  EXPECT_THAT(current_measurements_[page_context],
              CPUDeltaMatches(page_context, expected_page_delta2,
                              MeasurementAlgorithm::kSum));

  EXPECT_THAT(
      current_measurements_[origin1_in_bi_context],
      CPUDeltaMatches(origin1_in_bi_context, expected_origin1_in_bi_delta2,
                      MeasurementAlgorithm::kSum));
  EXPECT_THAT(
      current_measurements_[origin2_in_bi_context],
      CPUDeltaMatches(origin2_in_bi_context, expected_origin2_in_bi_delta2,
                      MeasurementAlgorithm::kSum));
  EXPECT_THAT(current_measurements_[origin1_in_other_bi_context],
              CPUDeltaMatches(origin1_in_other_bi_context,
                              expected_origin1_in_other_bi_delta2,
                              MeasurementAlgorithm::kSum));
  EXPECT_THAT(current_measurements_[origin2_in_other_bi_context],
              CPUDeltaMatches(origin2_in_other_bi_context,
                              expected_origin2_in_other_bi_delta2,
                              MeasurementAlgorithm::kSum));

  cpu_monitor_.RepeatingQueryStopped(kDummyQuery);
}

// Tests that WorkerNode CPU usage is correctly distributed to pages as clients
// are added and removed.
TEST_F(ResourceAttrCPUMonitorTest, AddRemoveWorkerClients) {
  MockMultiplePagesAndWorkersWithKnownOriginsGraph mock_graph(graph(), kOrigin1,
                                                              kOrigin2);

  // Assign URL's to frames in the graph so that they'll be mapped to
  // OriginInBrowsingInstanceContexts.
  mock_graph.frame->OnNavigationCommitted(
      kUrl1, kOrigin1,
      /*same_document=*/false, /*is_served_from_back_forward_cache=*/false);
  mock_graph.other_frame->OnNavigationCommitted(
      kUrl2, kOrigin2,
      /*same_document=*/false, /*is_served_from_back_forward_cache=*/false);
  mock_graph.child_frame->OnNavigationCommitted(
      kUrl1, kOrigin1,
      /*same_document=*/false, /*is_served_from_back_forward_cache=*/false);

  SetProcessCPUUsage(mock_graph.process.get(), 0.6);
  SetProcessCPUUsage(mock_graph.other_process.get(), 0.5);

  StartMonitoring();

  // Use a repeating query to get results for a dead `origin2_in_bi_context`
  // below (non-repeating queries don't get results for dead
  // `OriginInBrowsingInstanceContext`s).
  cpu_monitor_.RepeatingQueryStarted(kQueryId);

  const FrameContext& frame_context = mock_graph.frame->GetResourceContext();
  const FrameContext& child_frame_context =
      mock_graph.child_frame->GetResourceContext();
  const PageContext& page_context = mock_graph.page->GetResourceContext();
  const PageContext& other_page_context =
      mock_graph.other_page->GetResourceContext();

  const auto origin1_in_bi_context =
      OriginInBrowsingInstanceContext(kOrigin1, kBrowsingInstanceForPage);
  const auto origin2_in_bi_context =
      OriginInBrowsingInstanceContext(kOrigin2, kBrowsingInstanceForPage);
  const auto origin1_in_other_bi_context =
      OriginInBrowsingInstanceContext(kOrigin1, kBrowsingInstanceForOtherPage);
  const auto origin2_in_other_bi_context =
      OriginInBrowsingInstanceContext(kOrigin2, kBrowsingInstanceForOtherPage);

  auto new_worker1 =
      CreateWorkerNodeWithOrigin(graph(), mock_graph.process.get(), kOrigin1);
  const auto new_worker1_context = new_worker1->GetResourceContext();
  auto new_worker2 = CreateWorkerNodeWithOrigin(
      graph(), mock_graph.other_process.get(), kOrigin2);
  const auto new_worker2_context = new_worker2->GetResourceContext();

  task_env().FastForwardBy(kTimeBetweenMeasurements);
  UpdateAndGetCPUMeasurements(kQueryId);

  // During this interval:
  //
  // * `process` split its 60% CPU usage between 4 nodes:
  //   * `frame`, `other_frame`, `worker`, `new_worker1`
  //
  //   * `frame` and `worker` are part of `page`
  //   * `other_frame` is part of `other_page`
  //   * `frame` and `worker` are part of `origin1_in_bi`.
  //   * `other_frame` is part of `origin2_in_other_bi`.
  //
  // * `other_process` splits its 50% CPU usage between 3 nodes:
  //   * `child_frame`, `other_worker`, `new_worker2`
  //
  //   * `child_frame` and `other_worker` are part of `other_page`
  //   * `child_frame` is part of `origin1_in_other_bi`.
  //   * `other_worker` is part of `origin2_in_other_bi`.
  constexpr base::TimeDelta process_split = kTimeBetweenMeasurements * 0.6 / 4;
  constexpr base::TimeDelta other_process_split =
      kTimeBetweenMeasurements * 0.5 / 3;

  EXPECT_THAT(current_measurements_[frame_context],
              CPUDeltaMatches(frame_context, process_split,
                              MeasurementAlgorithm::kSplit));
  EXPECT_THAT(current_measurements_[new_worker1_context],
              CPUDeltaMatches(new_worker1_context, process_split,
                              MeasurementAlgorithm::kSplit));

  EXPECT_THAT(current_measurements_[child_frame_context],
              CPUDeltaMatches(child_frame_context, other_process_split,
                              MeasurementAlgorithm::kSplit));
  EXPECT_THAT(current_measurements_[new_worker2_context],
              CPUDeltaMatches(new_worker2_context, other_process_split,
                              MeasurementAlgorithm::kSplit));

  EXPECT_THAT(current_measurements_[page_context],
              CPUDeltaMatches(page_context, 2 * process_split,
                              MeasurementAlgorithm::kSum));
  EXPECT_THAT(current_measurements_[other_page_context],
              CPUDeltaMatches(other_page_context,
                              process_split + 2 * other_process_split,
                              MeasurementAlgorithm::kSum));

  EXPECT_THAT(current_measurements_[origin1_in_bi_context],
              CPUDeltaMatches(origin1_in_bi_context, 2 * process_split,
                              MeasurementAlgorithm::kSum));
  EXPECT_FALSE(base::Contains(current_measurements_, origin2_in_bi_context));
  EXPECT_THAT(current_measurements_[origin1_in_other_bi_context],
              CPUDeltaMatches(origin1_in_other_bi_context, other_process_split,
                              MeasurementAlgorithm::kSum));
  EXPECT_THAT(current_measurements_[origin2_in_other_bi_context],
              CPUDeltaMatches(origin2_in_other_bi_context,
                              process_split + other_process_split,
                              MeasurementAlgorithm::kSum));

  // Half-way through the interval, make `frame` a client of `new_worker1` and
  // `worker` a client of `new_worker2`.
  task_env().FastForwardBy(kTimeBetweenMeasurements / 2);
  new_worker1->AddClientFrame(mock_graph.frame.get());
  new_worker2->AddClientWorker(mock_graph.worker.get());

  task_env().FastForwardBy(kTimeBetweenMeasurements / 2);
  UpdateAndGetCPUMeasurements(kQueryId);

  // The split of CPU between frames and workers should not change. But during
  // the second half of the interval:
  //
  // * `page` contains 4 contexts: `frame`, `worker`, `new_worker1`,
  //       `new_worker2`
  // * `origin1_in_bi` contains 3 contexts: `frame`, `worker`, `new_worker1`
  // * `origin2_in_bi` contains 1 context: `new_worker2`
  constexpr base::TimeDelta expected_page_delta =
      /*first half, 2 nodes*/ (2 * process_split) / 2 +
      /*second half, 4 nodes*/ (3 * process_split + other_process_split) / 2;
  constexpr base::TimeDelta expected_origin1_in_bi_delta =
      /*first half, 2 nodes*/ (2 * process_split) / 2 +
      /*second half, 3 nodes*/ (3 * process_split) / 2;
  constexpr base::TimeDelta expected_origin2_in_bi_delta =
      /*first half, 0 nodes*/ base::TimeDelta() +
      /*second half, 1 node*/ other_process_split / 2;

  EXPECT_THAT(current_measurements_[frame_context],
              CPUDeltaMatches(frame_context, process_split,
                              MeasurementAlgorithm::kSplit));
  EXPECT_THAT(current_measurements_[new_worker1_context],
              CPUDeltaMatches(new_worker1_context, process_split,
                              MeasurementAlgorithm::kSplit));

  EXPECT_THAT(current_measurements_[child_frame_context],
              CPUDeltaMatches(child_frame_context, other_process_split,
                              MeasurementAlgorithm::kSplit));
  EXPECT_THAT(current_measurements_[new_worker2_context],
              CPUDeltaMatches(new_worker2_context, other_process_split,
                              MeasurementAlgorithm::kSplit));

  EXPECT_THAT(current_measurements_[page_context],
              CPUDeltaMatches(page_context, expected_page_delta,
                              MeasurementAlgorithm::kSum));
  EXPECT_THAT(current_measurements_[other_page_context],
              CPUDeltaMatches(other_page_context,
                              process_split + 2 * other_process_split,
                              MeasurementAlgorithm::kSum));

  EXPECT_THAT(
      current_measurements_[origin1_in_bi_context],
      CPUDeltaMatches(origin1_in_bi_context, expected_origin1_in_bi_delta,
                      MeasurementAlgorithm::kSum));
  EXPECT_THAT(
      current_measurements_[origin2_in_bi_context],
      CPUDeltaMatches(origin2_in_bi_context, expected_origin2_in_bi_delta,
                      MeasurementAlgorithm::kSum));
  EXPECT_THAT(current_measurements_[origin1_in_other_bi_context],
              CPUDeltaMatches(origin1_in_other_bi_context, other_process_split,
                              MeasurementAlgorithm::kSum));
  EXPECT_THAT(current_measurements_[origin2_in_other_bi_context],
              CPUDeltaMatches(origin2_in_other_bi_context,
                              process_split + other_process_split,
                              MeasurementAlgorithm::kSum));

  // Half-way through the interval, make `other_worker` a client of
  // `new_worker2` instead of `worker`.
  task_env().FastForwardBy(kTimeBetweenMeasurements / 2);
  new_worker2->RemoveClientWorker(mock_graph.worker.get());
  new_worker2->AddClientWorker(mock_graph.other_worker.get());
  const base::TimeTicks client_changed_time = base::TimeTicks::Now();
  task_env().FastForwardBy(kTimeBetweenMeasurements / 2);
  UpdateAndGetCPUMeasurements(kQueryId);

  // The first half of the interval is unchanged (`page` contains 4 contexts,
  // `other_page` contains 3).
  //
  // During the second half of the interval:
  //
  // * `page` contains 3 contexts: `frame`, `worker`, `new_worker1` (all in
  //       `process`)
  // * `other_page` contains 4 contexts: `other_frame` (in `process),
  //       `child_frame`, `other_worker`, `new_worker2` (in `other_process`)
  // * `origin1_in_bi` is unchanged with 3 contexts: `frame`, `worker`,
  //       `new_worker1`
  // * `origin2_in_bi` contains no contexts.
  // * `origin1_in_other_bi` is unchanged with 1 context: `child_frame`
  // * `origin2_in_other_bi` contains 3 contexts: `other_frame`,
  //       `other_worker`, `new_worker2`
  constexpr base::TimeDelta expected_page_delta2 =
      /*first half, 4 nodes*/ (3 * process_split + other_process_split) / 2 +
      /*second half, 3 nodes*/ (3 * process_split) / 2;
  constexpr base::TimeDelta expected_origin1_in_bi_delta2 = 3 * process_split;
  constexpr base::TimeDelta expected_origin2_in_bi_delta2 =
      /*first half, 1 node*/ other_process_split / 2 +
      /*second half, 0 nodes*/ base::TimeDelta();
  constexpr base::TimeDelta expected_other_page_delta =
      /*first half, 3 nodes*/ (process_split + 2 * other_process_split) / 2 +
      /*second half, 4 nodes*/ (process_split + 3 * other_process_split) / 2;
  constexpr base::TimeDelta expected_origin1_in_other_bi_delta =
      other_process_split;
  constexpr base::TimeDelta expected_origin2_in_other_bi_delta =
      /*first half, 2 nodes*/ (process_split + other_process_split) / 2 +
      /*second half, 3 nodes*/ (process_split + 2 * other_process_split) / 2;

  EXPECT_THAT(current_measurements_[page_context],
              CPUDeltaMatches(page_context, expected_page_delta2,
                              MeasurementAlgorithm::kSum));
  EXPECT_THAT(current_measurements_[other_page_context],
              CPUDeltaMatches(other_page_context, expected_other_page_delta,
                              MeasurementAlgorithm::kSum));

  EXPECT_THAT(
      current_measurements_[origin1_in_bi_context],
      CPUDeltaMatches(origin1_in_bi_context, expected_origin1_in_bi_delta2,
                      MeasurementAlgorithm::kSum));
  // The measurement of `origin2_in_bi_context` doesn't update
  // after the client list of `new_worker2` changes.
  EXPECT_THAT(current_measurements_[origin2_in_bi_context],
              CPUDeltaMatchesWithMeasurementTime(
                  origin2_in_bi_context,
                  /*expected_delta=*/expected_origin2_in_bi_delta2,
                  /*expected_background_delta=*/base::TimeDelta(),
                  client_changed_time, MeasurementAlgorithm::kSum));
  EXPECT_THAT(current_measurements_[origin1_in_other_bi_context],
              CPUDeltaMatches(origin1_in_other_bi_context,
                              expected_origin1_in_other_bi_delta,
                              MeasurementAlgorithm::kSum));
  EXPECT_THAT(current_measurements_[origin2_in_other_bi_context],
              CPUDeltaMatches(origin2_in_other_bi_context,
                              expected_origin2_in_other_bi_delta,
                              MeasurementAlgorithm::kSum));

  // Test workers with multiple clients, and multiple paths to the same
  // FrameNode or PageNode.
  mock_graph.other_worker->AddClientWorker(new_worker1.get());
  new_worker2->AddClientWorker(new_worker1.get());

  // Now the clients are:
  //
  // `new_worker1` -> `frame`
  // `worker` -> `frame` (see mock_graphs.cc)
  // `other_worker` -> `child_frame` (see mock_graphs.cc)
  // `other_worker` -> `new_worker1` -> `frame`
  // `new_worker2` -> `other_worker` -> `child_frame`
  // `new_worker2` -> `other_worker` -> `new_worker1` -> `frame`
  // `new_worker2` -> `new_worker1` -> `frame`
  //
  // * `page` contains 5 contexts: `frame` and all workers with `frame` as a
  //   client:
  //   * `frame`, `new_worker1`, `worker` (in `process`), `other_worker`,
  //     `new_worker2` (in `other_process`)
  // * `other_page` contains 4 contexts: `other_frame`, `child_frame`, and all
  //   workers with either of them as a client:
  //   * `other_frame` (in `process), `child_frame`, `other_worker`,
  //   `new_worker2`
  //     (in `other_process`)
  // * `origin1_in_bi` contains 3 contexts: `frame`, `new_worker1`, `worker`
  // * `origin2_in_bi` contains 2 contexts: `other_worker`, `new_worker2`
  // * `origin1_in_other_bi` contains 1 context: `child_frame`
  // * `origin2_in_other_bi` contains 3 contexts: `other_frame`,
  //   `other_worker`, `new_worker2`
  constexpr base::TimeDelta expected_page_delta3 =
      3 * process_split + 2 * other_process_split;
  constexpr base::TimeDelta expected_origin1_in_bi_delta3 = 3 * process_split;
  constexpr base::TimeDelta expected_origin2_in_bi_delta3 =
      2 * other_process_split;
  constexpr base::TimeDelta expected_other_page_delta2 =
      process_split + 3 * other_process_split;
  constexpr base::TimeDelta expected_origin1_in_other_bi_delta2 =
      other_process_split;
  constexpr base::TimeDelta expected_origin2_in_other_bi_delta2 =
      process_split + 2 * other_process_split;

  task_env().FastForwardBy(kTimeBetweenMeasurements);
  UpdateAndGetCPUMeasurements(kQueryId);

  EXPECT_THAT(current_measurements_[page_context],
              CPUDeltaMatches(page_context, expected_page_delta3,
                              MeasurementAlgorithm::kSum));
  EXPECT_THAT(current_measurements_[other_page_context],
              CPUDeltaMatches(other_page_context, expected_other_page_delta2,
                              MeasurementAlgorithm::kSum));

  EXPECT_THAT(
      current_measurements_[origin1_in_bi_context],
      CPUDeltaMatches(origin1_in_bi_context, expected_origin1_in_bi_delta3,
                      MeasurementAlgorithm::kSum));
  EXPECT_THAT(
      current_measurements_[origin2_in_bi_context],
      CPUDeltaMatches(origin2_in_bi_context, expected_origin2_in_bi_delta3,
                      MeasurementAlgorithm::kSum));
  EXPECT_THAT(current_measurements_[origin1_in_other_bi_context],
              CPUDeltaMatches(origin1_in_other_bi_context,
                              expected_origin1_in_other_bi_delta2,
                              MeasurementAlgorithm::kSum));
  EXPECT_THAT(current_measurements_[origin2_in_other_bi_context],
              CPUDeltaMatches(origin2_in_other_bi_context,
                              expected_origin2_in_other_bi_delta2,
                              MeasurementAlgorithm::kSum));

  // Break the link between `new_worker2` and `new_worker1`. `new_worker2`
  // should still be in `page` because a path to `frame` still exists:
  // * `new_worker2` -> `other_worker` -> `new_worker1` -> `frame`.
  // Therefore none of the expectations will change.
  new_worker2->RemoveClientWorker(new_worker1.get());

  task_env().FastForwardBy(kTimeBetweenMeasurements);
  UpdateAndGetCPUMeasurements(kQueryId);

  EXPECT_THAT(current_measurements_[page_context],
              CPUDeltaMatches(page_context, expected_page_delta3,
                              MeasurementAlgorithm::kSum));
  EXPECT_THAT(current_measurements_[other_page_context],
              CPUDeltaMatches(other_page_context, expected_other_page_delta2,
                              MeasurementAlgorithm::kSum));

  EXPECT_THAT(
      current_measurements_[origin1_in_bi_context],
      CPUDeltaMatches(origin1_in_bi_context, expected_origin1_in_bi_delta3,
                      MeasurementAlgorithm::kSum));
  EXPECT_THAT(
      current_measurements_[origin2_in_bi_context],
      CPUDeltaMatches(origin2_in_bi_context, expected_origin2_in_bi_delta3,
                      MeasurementAlgorithm::kSum));
  EXPECT_THAT(current_measurements_[origin1_in_other_bi_context],
              CPUDeltaMatches(origin1_in_other_bi_context,
                              expected_origin1_in_other_bi_delta2,
                              MeasurementAlgorithm::kSum));
  EXPECT_THAT(current_measurements_[origin2_in_other_bi_context],
              CPUDeltaMatches(origin2_in_other_bi_context,
                              expected_origin2_in_other_bi_delta2,
                              MeasurementAlgorithm::kSum));

  // Need to remove all clients before deleting WorkerNodes
  auto remove_clients = [](TestNodeWrapper<WorkerNodeImpl>& worker) {
    for (FrameNodeImpl* client : worker->client_frames()) {
      worker->RemoveClientFrame(client);
    }
    for (WorkerNodeImpl* client : worker->client_workers()) {
      worker->RemoveClientWorker(client);
    }
  };
  remove_clients(new_worker1);
  remove_clients(new_worker2);

  // Only remove the clients that were manually added to `worker` and
  // `other_worker`. The `mock_graph` destructor will remove the others, and
  // CHECK if they aren't there.
  mock_graph.other_worker->RemoveClientWorker(new_worker1.get());

  cpu_monitor_.RepeatingQueryStopped(kQueryId);
}

// Tests that CPU usage of processes is correctly distributed between
// OriginInBrowsingInstanceContexts when a frame origin changes between
// measurements.
TEST_F(ResourceAttrCPUMonitorTest, NavigateChangesOrigin) {
  MockMultiplePagesAndWorkersWithKnownOriginsGraph mock_graph(graph(), kOrigin1,
                                                              kOrigin2);

  // Assign URL's to some frames in the graph so that they'll be mapped to
  // OriginInBrowsingInstanceContexts.
  mock_graph.other_frame->OnNavigationCommitted(
      kUrl2, kOrigin2,
      /*same_document=*/false, /*is_served_from_back_forward_cache=*/false);
  mock_graph.child_frame->OnNavigationCommitted(
      kUrl1, kOrigin1,
      /*same_document=*/false, /*is_served_from_back_forward_cache=*/false);

  SetProcessCPUUsage(mock_graph.process.get(), 0.6);
  SetProcessCPUUsage(mock_graph.other_process.get(), 0.5);

  StartMonitoring();

  // Use a repeating query to get results for a dead
  // `origin2_in_other_bi_context` below (non-repeating queries don't get
  // results for dead `OriginInBrowsingInstanceContext`s).
  cpu_monitor_.RepeatingQueryStarted(kQueryId);

  const ProcessContext& process_context =
      mock_graph.process->GetResourceContext();
  const ProcessContext& other_process_context =
      mock_graph.other_process->GetResourceContext();
  const PageContext& page_context = mock_graph.page->GetResourceContext();
  const PageContext& other_page_context =
      mock_graph.other_page->GetResourceContext();

  const auto origin1_in_bi_context =
      OriginInBrowsingInstanceContext(kOrigin1, kBrowsingInstanceForPage);
  const auto origin2_in_bi_context =
      OriginInBrowsingInstanceContext(kOrigin2, kBrowsingInstanceForPage);
  const auto origin1_in_other_bi_context =
      OriginInBrowsingInstanceContext(kOrigin1, kBrowsingInstanceForOtherPage);
  const auto origin2_in_other_bi_context =
      OriginInBrowsingInstanceContext(kOrigin2, kBrowsingInstanceForOtherPage);

  // Navigate frames partway through the measurement.
  task_env().FastForwardBy(kTimeBetweenMeasurements / 3);

  // No origin -> kOrigin2.
  mock_graph.frame->OnNavigationCommitted(
      kUrl2, kOrigin2,
      /*same_document=*/false, /*is_served_from_back_forward_cache=*/false);
  // kOrigin2 -> kOrigin1.
  mock_graph.other_frame->OnNavigationCommitted(
      kUrl1, kOrigin1,
      /*same_document=*/false, /*is_served_from_back_forward_cache=*/false);

  // Same-document navigation should not change the origin (kOrigin1 ->
  // kOrigin1).
  mock_graph.child_frame->OnNavigationCommitted(
      GURL("http://a.com#fragment"), kOrigin1,
      /*same_document=*/false, /*is_served_from_back_forward_cache=*/false);

  task_env().FastForwardBy(kTimeBetweenMeasurements * 2 / 3);
  UpdateAndGetCPUMeasurements(kQueryId);

  // * `process` split its 60% CPU usage between 3 nodes:
  //   * `frame`, `other_frame`, `worker`
  //   * `frame` and `worker` are part of `page`.
  //   * `other_frame` is part of `other_page`.
  // * `other_process` splits its 50% CPU usage between 2 nodes:
  //   * `child_frame`, `other_worker` (both part of `other_page`).
  //
  // For the first 1/3 of the period:
  //
  //   * `origin1_in_bi` contains 1 node: `worker`.
  //   * `origin2_in_bi` contains 0 nodes.
  //   * `origin1_in_other_bi` contains 1 node: `child_frame`.
  //   * `origin2_in_other_bi` contains 2 nodes: `other_frame`,
  //     `other_worker`.
  //
  // For the last 2/3:
  //
  //   * `origin1_in_bi` contains 1 node: `worker`.
  //   * `origin2_in_bi` contains 1 node: `frame`.
  //   * `origin1_in_other_bi` contains 2 nodes: `child_frame`, `other_frame`.
  //   * `origin2_in_other_bi` contains 1 node: `other_worker`.
  constexpr base::TimeDelta process_split = kTimeBetweenMeasurements * 0.6 / 3;
  constexpr base::TimeDelta other_process_split =
      kTimeBetweenMeasurements * 0.5 / 2;

  constexpr base::TimeDelta expected_page_delta = 2 * process_split;
  constexpr base::TimeDelta expected_other_page_delta =
      process_split + 2 * other_process_split;

  constexpr base::TimeDelta expected_origin1_in_bi_delta = process_split;
  constexpr base::TimeDelta expected_origin2_in_bi_delta =
      /*first 1/3, 0 nodes*/ base::TimeDelta() +
      /*last 2/3, 1 node*/ process_split * 2 / 3;
  constexpr base::TimeDelta expected_origin1_in_other_bi_delta =
      /*first 1/3, 1 node*/ other_process_split / 3 +
      /*last 2/3, 2 nodes*/ (process_split + other_process_split) * 2 / 3;
  constexpr base::TimeDelta expected_origin2_in_other_bi_delta =
      /*first 1/3, 2 nodes*/ (process_split + other_process_split) / 3 +
      /*last 2/3, 1 node*/ other_process_split * 2 / 3;

  EXPECT_THAT(current_measurements_[process_context],
              CPUDeltaMatches(process_context, kTimeBetweenMeasurements * 0.6));
  EXPECT_THAT(
      current_measurements_[other_process_context],
      CPUDeltaMatches(other_process_context, kTimeBetweenMeasurements * 0.5));

  EXPECT_THAT(current_measurements_[page_context],
              CPUDeltaMatches(page_context, expected_page_delta,
                              MeasurementAlgorithm::kSum));
  EXPECT_THAT(current_measurements_[other_page_context],
              CPUDeltaMatches(page_context, expected_other_page_delta,
                              MeasurementAlgorithm::kSum));

  EXPECT_THAT(
      current_measurements_[origin1_in_bi_context],
      CPUDeltaMatches(origin1_in_bi_context, expected_origin1_in_bi_delta,
                      MeasurementAlgorithm::kSum));
  EXPECT_THAT(
      current_measurements_[origin2_in_bi_context],
      CPUDeltaMatches(origin2_in_bi_context, expected_origin2_in_bi_delta,
                      MeasurementAlgorithm::kSum));
  EXPECT_THAT(current_measurements_[origin1_in_other_bi_context],
              CPUDeltaMatches(origin1_in_other_bi_context,
                              expected_origin1_in_other_bi_delta,
                              MeasurementAlgorithm::kSum));
  EXPECT_THAT(current_measurements_[origin2_in_other_bi_context],
              CPUDeltaMatches(origin2_in_other_bi_context,
                              expected_origin2_in_other_bi_delta,
                              MeasurementAlgorithm::kSum));

  cpu_monitor_.RepeatingQueryStopped(kQueryId);
}

// Tests that `cumulative_background_cpu` is correctly maintained, including
// when process priority changes during a measurement interval.
TEST_F(ResourceAttrCPUMonitorTest, BackgroundCPU) {
  performance_manager::MockMultiplePagesAndWorkersWithMultipleProcessesGraph
      mock_graph(graph());

  mock_graph.process->set_priority(base::TaskPriority::USER_BLOCKING);
  mock_graph.other_process->set_priority(base::TaskPriority::USER_BLOCKING);

  SetProcessCPUUsage(mock_graph.process.get(), 0.6);
  SetProcessCPUUsage(mock_graph.other_process.get(), 0.5);

  StartMonitoring();

  const ProcessContext& process_context =
      mock_graph.process->GetResourceContext();
  const ProcessContext& other_process_context =
      mock_graph.other_process->GetResourceContext();
  const FrameContext& frame_context = mock_graph.frame->GetResourceContext();
  const FrameContext& child_frame_context =
      mock_graph.child_frame->GetResourceContext();

  // Set process' priority to `BEST_EFFORT` at 1/3 of the measurement interval.
  task_env().FastForwardBy(kTimeBetweenMeasurements / 3);
  mock_graph.process->set_priority(base::TaskPriority::BEST_EFFORT);

  // Set process' priority to `USER_BLOCKING` at 2/3 of the measurement
  // interval.
  task_env().FastForwardBy(kTimeBetweenMeasurements / 3);
  mock_graph.process->set_priority(base::TaskPriority::USER_BLOCKING);

  task_env().FastForwardBy(kTimeBetweenMeasurements / 3);
  UpdateAndGetCPUMeasurements();

  {
    constexpr base::TimeDelta process_delta = kTimeBetweenMeasurements * 0.6;
    constexpr base::TimeDelta process_background_delta = process_delta / 3;
    constexpr base::TimeDelta other_process_delta =
        kTimeBetweenMeasurements * 0.5;
    constexpr base::TimeDelta other_process_background_delta =
        base::TimeDelta();

    // Verify that process background CPU time is correctly measured.
    EXPECT_THAT(current_measurements_[process_context],
                CPUDeltaWithBackgroundMatches(process_context, process_delta,
                                              process_background_delta));
    EXPECT_THAT(current_measurements_[other_process_context],
                CPUDeltaWithBackgroundMatches(other_process_context,
                                              other_process_delta,
                                              other_process_background_delta));

    // Verify that process background CPU time is correctly split.
    //
    // * `process` splits its 60% CPU usage evenly between `frame`,
    //   `other_frame` and `worker`.
    // * `other_process` splits its 50% CPU usage evenly between `child_frame`
    //   and `other_worker`.
    // See the chart in MockMultiplePagesAndWorkersWithMultipleProcessesGraph.
    constexpr base::TimeDelta process_delta_split = process_delta / 3;
    constexpr base::TimeDelta process_background_delta_split =
        process_background_delta / 3;
    constexpr base::TimeDelta other_process_delta_split =
        other_process_delta / 2;
    constexpr base::TimeDelta other_process_background_delta_split =
        other_process_background_delta / 2;

    EXPECT_THAT(
        current_measurements_[frame_context],
        CPUDeltaWithBackgroundMatches(frame_context, process_delta_split,
                                      process_background_delta_split,
                                      MeasurementAlgorithm::kSplit));
    EXPECT_THAT(current_measurements_[child_frame_context],
                CPUDeltaWithBackgroundMatches(
                    child_frame_context, other_process_delta_split,
                    other_process_background_delta_split,
                    MeasurementAlgorithm::kSplit));
  }

  // Set other process' priority to `BEST_EFFORT` for a full measurement
  // interval.
  mock_graph.other_process->set_priority(base::TaskPriority::BEST_EFFORT);
  task_env().FastForwardBy(kTimeBetweenMeasurements);
  UpdateAndGetCPUMeasurements();

  {
    // Verify that process background CPU time is correctly measured.
    constexpr base::TimeDelta process_delta = kTimeBetweenMeasurements * 0.6;
    constexpr base::TimeDelta process_background_delta = base::TimeDelta();
    constexpr base::TimeDelta other_process_delta =
        kTimeBetweenMeasurements * 0.5;
    constexpr base::TimeDelta other_process_background_delta =
        other_process_delta;

    EXPECT_THAT(current_measurements_[process_context],
                CPUDeltaWithBackgroundMatches(process_context, process_delta,
                                              process_background_delta));
    EXPECT_THAT(current_measurements_[other_process_context],
                CPUDeltaWithBackgroundMatches(other_process_context,
                                              other_process_delta,
                                              other_process_background_delta));

    // Don't verify that process background CPU time is correctly split, as that
    // would be redundant.
  }
}

// Test that CPU time is accumulated correctly when an
// `OriginInBrowsingInstanceContext` dies and is revived, even when there are
// concurrent queries.
TEST_F(ResourceAttrCPUMonitorTest, OriginInBrowsingInstanceContextLifetime) {
  performance_manager::MockSinglePageInSingleProcessGraph mock_graph(graph());

  constexpr double kCPUProportion = 0.5;
  SetProcessCPUUsage(mock_graph.process.get(), kCPUProportion);

  StartMonitoring();

  cpu_monitor_.RepeatingQueryStarted(kQueryId);
  cpu_monitor_.RepeatingQueryStarted(kOtherQueryId);

  const OriginInBrowsingInstanceContext kOrigin1Context(
      kOrigin1, kBrowsingInstanceForPage);
  const OriginInBrowsingInstanceContext kOrigin2Context(
      kOrigin2, kBrowsingInstanceForPage);

  mock_graph.frame->OnNavigationCommitted(
      kUrl1, kOrigin1,
      /*same_document=*/false, /*is_served_from_back_forward_cache=*/false);
  task_env().FastForwardBy(kTimeBetweenMeasurements / 2);
  mock_graph.frame->OnNavigationCommitted(
      kUrl2, kOrigin2,
      /*same_document=*/false, /*is_served_from_back_forward_cache=*/false);
  task_env().FastForwardBy(kTimeBetweenMeasurements / 2);

  {
    // Query observes:
    // - Origin 1: 1/2 interval (dead at time of measurement)
    // - Origin 2: 1/2 interval
    // Because the origin changed midway through the measurement.
    auto measurement = cpu_monitor_.UpdateAndGetCPUMeasurements(kQueryId);
    EXPECT_EQ(measurement[kOrigin1Context].cpu_time_result->cumulative_cpu,
              kCPUProportion * kTimeBetweenMeasurements * 0.5);
    EXPECT_EQ(measurement[kOrigin2Context].cpu_time_result->cumulative_cpu,
              kCPUProportion * kTimeBetweenMeasurements * 0.5);
  }

  task_env().FastForwardBy(kTimeBetweenMeasurements);

  {
    // Other query observes:
    // - Origin 1: 1/2 interval (dead at time of measurement)
    // - Origin 2: 3/2 interval
    // The fact that antother query already observed the CPU usage for origin 1
    // and that the context is dead since then does not affect the results.
    auto measurement = cpu_monitor_.UpdateAndGetCPUMeasurements(kOtherQueryId);
    EXPECT_EQ(measurement[kOrigin1Context].cpu_time_result->cumulative_cpu,
              kCPUProportion * kTimeBetweenMeasurements * 0.5);
    EXPECT_EQ(measurement[kOrigin2Context].cpu_time_result->cumulative_cpu,
              kCPUProportion * kTimeBetweenMeasurements * 1.5);
  }

  mock_graph.frame->OnNavigationCommitted(
      kUrl1, kOrigin1,
      /*same_document=*/false, /*is_served_from_back_forward_cache=*/false);
  task_env().FastForwardBy(kTimeBetweenMeasurements);

  {
    // Query observes:
    // - Origin 1: 3/2 interval
    // - Origin 2: 3/2 interval (dead at time of measurement)
    // The cumulative CPU usage for origin 1 is not reset because there was a
    // result for that context at the last measurement. The fact that the
    // context was transiently dead does not affect the results.
    auto measurement = cpu_monitor_.UpdateAndGetCPUMeasurements(kQueryId);
    EXPECT_EQ(measurement[kOrigin1Context].cpu_time_result->cumulative_cpu,
              kCPUProportion * kTimeBetweenMeasurements * 1.5);
    EXPECT_EQ(measurement[kOrigin2Context].cpu_time_result->cumulative_cpu,
              kCPUProportion * kTimeBetweenMeasurements * 1.5);
  }

  {
    // Other query observes:
    // - Origin 1: 3/2 interval
    // - Origin 2: 3/2 interval (dead at time of measurement)
    auto measurement = cpu_monitor_.UpdateAndGetCPUMeasurements(kOtherQueryId);
    EXPECT_EQ(measurement[kOrigin1Context].cpu_time_result->cumulative_cpu,
              kCPUProportion * kTimeBetweenMeasurements * 1.5);
    EXPECT_EQ(measurement[kOrigin2Context].cpu_time_result->cumulative_cpu,
              kCPUProportion * kTimeBetweenMeasurements * 1.5);
  }

  task_env().FastForwardBy(kTimeBetweenMeasurements);

  {
    // Query observes:
    // - Origin 1: 5/2 interval
    // - Origin 2: no result (dead at time of measurement)
    // A context that was dead at the last measurement and not revived since
    // then is not included in results.
    auto measurement = cpu_monitor_.UpdateAndGetCPUMeasurements(kQueryId);
    EXPECT_EQ(measurement[kOrigin1Context].cpu_time_result->cumulative_cpu,
              kCPUProportion * kTimeBetweenMeasurements * 2.5);
    EXPECT_FALSE(base::Contains(measurement, kOrigin2Context));
  }

  {
    // Other query observes:
    // - Origin 1: 5/2 interval
    // - Origin 2: no result
    auto measurement = cpu_monitor_.UpdateAndGetCPUMeasurements(kOtherQueryId);
    EXPECT_EQ(measurement[kOrigin1Context].cpu_time_result->cumulative_cpu,
              kCPUProportion * kTimeBetweenMeasurements * 2.5);
    EXPECT_FALSE(base::Contains(measurement, kOrigin2Context));
  }

  // Revive the context for origin 2.
  mock_graph.frame->OnNavigationCommitted(
      kUrl2, kOrigin2,
      /*same_document=*/false, /*is_served_from_back_forward_cache=*/false);
  task_env().FastForwardBy(kTimeBetweenMeasurements);

  {
    // Query observes:
    // - Origin 1: 5/2 interval
    // - Origin 2: 1/2 interval
    // The cumultative CPU usage for origin 2 is reset because the context
    // wasn't in the last returned result.
    auto measurement = cpu_monitor_.UpdateAndGetCPUMeasurements(kQueryId);
    EXPECT_EQ(measurement[kOrigin1Context].cpu_time_result->cumulative_cpu,
              kCPUProportion * kTimeBetweenMeasurements * 2.5);
    EXPECT_EQ(measurement[kOrigin2Context].cpu_time_result->cumulative_cpu,
              kCPUProportion * kTimeBetweenMeasurements * 1.0);
  }

  cpu_monitor_.RepeatingQueryStopped(kQueryId);
  cpu_monitor_.RepeatingQueryStopped(kOtherQueryId);
}

// Tests that errors returned from ProcessMetrics are correctly ignored.
TEST_F(ResourceAttrCPUMonitorTest, MeasurementError) {
  const TestNodeWrapper<ProcessNodeImpl> renderer1 = CreateMockCPURenderer();
  SetProcessId(renderer1.get());
  const TestNodeWrapper<ProcessNodeImpl> renderer2 = CreateMockCPURenderer();
  SetProcessId(renderer2.get());

  // Advance the clock before monitoring starts, so that the process launch
  // times can be distinguished from the start of monitoring.
  task_env().FastForwardBy(kTimeBetweenMeasurements);

  StartMonitoring();
  const auto monitoring_start_time = base::TimeTicks::Now();

  // `renderer1` measures 100% CPU usage. `renderer2` and `renderer3` have
  // errors before the first measurement. `renderer3` is created after
  // monitoring starts.
  task_env().FastForwardBy(kTimeBetweenMeasurements / 2);
  const TestNodeWrapper<ProcessNodeImpl> renderer3 = CreateMockCPURenderer();
  SetProcessId(renderer3.get());
  SetProcessCPUUsageError(renderer2.get(), ProcessCPUUsageError::kSystemError);
  SetProcessCPUUsageError(renderer3.get(), ProcessCPUUsageError::kSystemError);

  // Finish the measurement period.
  task_env().FastForwardBy(kTimeBetweenMeasurements / 2);
  UpdateAndGetCPUMeasurements();
  const auto previous_measurement_time = base::TimeTicks::Now();

  EXPECT_THAT(current_measurements_[renderer1->GetResourceContext()],
              AllOf(CPUDeltaMatches(renderer1->GetResourceContext(),
                                    kTimeBetweenMeasurements),
                    StartTimeMatches(monitoring_start_time)));
  EXPECT_FALSE(
      base::Contains(current_measurements_, renderer2->GetResourceContext()));
  EXPECT_FALSE(
      base::Contains(current_measurements_, renderer3->GetResourceContext()));

  // `renderer1` starts returning errors.
  SetProcessCPUUsageError(renderer1.get(), ProcessCPUUsageError::kSystemError);

  task_env().FastForwardBy(kTimeBetweenMeasurements);
  UpdateAndGetCPUMeasurements();

  // After an error the previous measurement should be returned unchanged.
  EXPECT_THAT(
      current_measurements_[renderer1->GetResourceContext()],
      CPUDeltaMatchesWithMeasurementTime(
          renderer1->GetResourceContext(), /*expected_delta=*/base::TimeDelta(),
          /*expected_background_delta=*/base::TimeDelta(),
          previous_measurement_time));
  EXPECT_FALSE(
      base::Contains(current_measurements_, renderer2->GetResourceContext()));
  EXPECT_FALSE(
      base::Contains(current_measurements_, renderer3->GetResourceContext()));

  SetProcessCPUUsageError(renderer1.get(), std::nullopt);
  SetProcessCPUUsageError(renderer2.get(), std::nullopt);
  SetProcessCPUUsageError(renderer3.get(), std::nullopt);

  task_env().FastForwardBy(kTimeBetweenMeasurements);
  UpdateAndGetCPUMeasurements();

  // The cumulative CPU usage to date includes the previous intervals which
  // weren't recorded due to the errors.
  EXPECT_THAT(current_measurements_[renderer1->GetResourceContext()],
              CPUDeltaMatches(renderer1->GetResourceContext(),
                              kTimeBetweenMeasurements * 2));
  EXPECT_THAT(current_measurements_[renderer2->GetResourceContext()],
              AllOf(CPUDeltaMatches(renderer2->GetResourceContext(),
                                    kTimeBetweenMeasurements * 3),
                    StartTimeMatches(monitoring_start_time)));
  // `renderer3` was created halfway through the first interval.
  EXPECT_THAT(current_measurements_[renderer3->GetResourceContext()],
              AllOf(CPUDeltaMatches(renderer3->GetResourceContext(),
                                    kTimeBetweenMeasurements * 2.5),
                    StartTimeMatches(monitoring_start_time +
                                     kTimeBetweenMeasurements / 2)));
}

// Tests the CPUProportionTracker helper class.
TEST_F(ResourceAttrCPUMonitorTest, CPUProportionTracker) {
  // Since the CPU monitor has trouble measuring processes on exit, create some
  // long-lived processes. The test will create and delete frames in a process
  // to measure contexts that are added and removed during measurement periods.
  // The frames will not share the process so they get all the process CPU.
  auto create_process = [&](double cpu_usage) {
    TestNodeWrapper<ProcessNodeImpl> renderer = CreateMockCPURenderer();
    SetProcessId(renderer.get());
    SetProcessCPUUsage(renderer.get(), cpu_usage);
    return renderer;
  };
  const TestNodeWrapper<ProcessNodeImpl> process_90 = create_process(0.9);
  const TestNodeWrapper<ProcessNodeImpl> process_80 = create_process(0.8);
  const TestNodeWrapper<ProcessNodeImpl> process_70 = create_process(0.7);
  const TestNodeWrapper<ProcessNodeImpl> process_60 = create_process(0.6);
  const TestNodeWrapper<ProcessNodeImpl> process_50 = create_process(0.5);
  const TestNodeWrapper<ProcessNodeImpl> process_40 = create_process(0.4);
  const TestNodeWrapper<PageNodeImpl> page_node = CreateNode<PageNodeImpl>();

  // Create a tracker that only looks at frames, so that the results are easier
  // to compare.
  CPUProportionTracker proportion_tracker(
      base::BindRepeating(&ContextIs<FrameContext>));
  StartMonitoring();

  // Assign results to a repeating query so that they're not dropped immediately
  // when nodes are removed.
  constexpr internal::QueryId kDummyQuery;
  cpu_monitor_.RepeatingQueryStarted(kDummyQuery);

  std::map<ResourceContext, double> expected_results;

  // Context that existed before CPUProportionTracker started.
  // Uses 50% CPU for the entire interval = 0.5.
  std::optional<TestNodeWrapper<FrameNodeImpl>> existing_frame1 =
      CreateFrameNodeAutoId(process_50.get(), page_node.get());
  expected_results[existing_frame1.value()->GetResourceContext()] = 0.5;

  // Another context that existed before CPUProportionTracker, and will exit
  // half-way through the interval.
  // Uses 40% CPU for half the interval = 0.2.
  std::optional<TestNodeWrapper<FrameNodeImpl>> existing_frame2 =
      CreateFrameNodeAutoId(process_40.get(), page_node.get());
  expected_results[existing_frame2.value()->GetResourceContext()] = 0.2;

  task_env().FastForwardBy(kTimeBetweenMeasurements);

  // Test the first interval, where the CPUProportionTracker has no history.
  proportion_tracker.StartFirstInterval(base::TimeTicks::Now(),
                                        GetCPUQueryResults(kDummyQuery));

  // Context exists for entire interval.
  // Uses 90% CPU for entire interval = 0.9.
  std::optional<TestNodeWrapper<FrameNodeImpl>> frame1 =
      CreateFrameNodeAutoId(process_90.get(), page_node.get());
  expected_results[frame1.value()->GetResourceContext()] = 0.9;

  // Context exists at start of interval, destroyed half-way through.
  // Uses 80% CPU for half the interval = 0.4.
  std::optional<TestNodeWrapper<FrameNodeImpl>> frame2 =
      CreateFrameNodeAutoId(process_80.get(), page_node.get());
  expected_results[frame2.value()->GetResourceContext()] = 0.4;

  task_env().FastForwardBy(kTimeBetweenMeasurements / 2);
  const auto half_first_interval = base::TimeTicks::Now();
  frame2.reset();
  existing_frame2.reset();

  // Context created half-way through measurement interval.
  // Uses 70% CPU for half the interval = 0.35.
  const TestNodeWrapper<FrameNodeImpl> frame3 =
      CreateFrameNodeAutoId(process_70.get(), page_node.get());
  expected_results[frame3->GetResourceContext()] = 0.35;

  // Context created half-way through measurement interval, destroyed 3/4 of the
  // way through.
  // Uses 60% CPU for 1/4 of the interval = 0.15.
  std::optional<TestNodeWrapper<FrameNodeImpl>> frame4 =
      CreateFrameNodeAutoId(process_60.get(), page_node.get());
  expected_results[frame4.value()->GetResourceContext()] = 0.15;

  task_env().FastForwardBy(kTimeBetweenMeasurements / 4);
  frame4.reset();

  // Destroy existing_frame1 at end of interval. Should still appear in
  // `expected_results` as existing for the whole interval since this is the
  // same tick as the measurement.
  task_env().FastForwardBy(kTimeBetweenMeasurements / 4);
  existing_frame1.reset();

  EXPECT_EQ(expected_results,
            proportion_tracker.StartNextInterval(
                base::TimeTicks::Now(), GetCPUQueryResults(kDummyQuery)));

  // Make sure the same scenarios also work for a second interval, where
  // CPUProportionTracker has history.
  std::map<ResourceContext, double> expected_results2;

  // existing_frame1 was destroyed at the start of the interval so is not
  // included in `expected_results2`.

  // frame3 existed before the interval.
  // Uses 70% CPU for the entire interval = 0.7.
  expected_results2[frame3->GetResourceContext()] = 0.7;

  // New context created at start of interval.
  // Uses 80% CPU for the entire interval = 0.8.
  const TestNodeWrapper<FrameNodeImpl> frame5 =
      CreateFrameNodeAutoId(process_80.get(), page_node.get());
  expected_results2[frame5->GetResourceContext()] = 0.8;

  // frame1 exists at start of interval, destroyed half-way through.
  // Uses 90% CPU for half the interval = 0.45.
  expected_results2[frame1.value()->GetResourceContext()] = 0.45;

  task_env().FastForwardBy(kTimeBetweenMeasurements / 2);
  frame1.reset();

  // New context created half-way through measurement interval.
  // Uses 60% CPU for half the interval = 0.3.
  const TestNodeWrapper<FrameNodeImpl> frame6 =
      CreateFrameNodeAutoId(process_60.get(), page_node.get());
  expected_results2[frame6->GetResourceContext()] = 0.3;

  // New context created half-way through measurement interval, destroyed 3/4 of
  // the way through. Uses 50% CPU for 1/4 of the interval = 0.125.
  std::optional<TestNodeWrapper<FrameNodeImpl>> frame7 =
      CreateFrameNodeAutoId(process_50.get(), page_node.get());
  expected_results2[frame7.value()->GetResourceContext()] = 0.125;

  task_env().FastForwardBy(kTimeBetweenMeasurements / 4);
  frame7.reset();

  task_env().FastForwardBy(kTimeBetweenMeasurements / 4);

  // Fake that the result included a node with `start_time` during the first
  // interval, which CPUProportionTracker didn't see during that interval. This
  // can happen in production if a WorkerNode that existed at `start_time` is
  // added to a PageNode later, moving the page's `start_time` back.
  // Since there's no baseline for the node, it shouldn't be included yet.
  TestNodeWrapper<FrameNodeImpl> frame8 =
      CreateFrameNodeAutoId(process_40.get(), page_node.get());
  auto add_fake_result = [&](QueryResultMap results,
                             base::TimeTicks measurement_time) {
    results[frame8->GetResourceContext()] = QueryResults{
        .cpu_time_result = CPUTimeResult{
            .metadata = ResultMetadata(
                measurement_time, MeasurementAlgorithm::kDirectMeasurement),
            .start_time = half_first_interval,
            .cumulative_cpu = (measurement_time - half_first_interval) * 0.4,
        }};
    return results;
  };
  EXPECT_EQ(expected_results2,
            proportion_tracker.StartNextInterval(
                base::TimeTicks::Now(),
                add_fake_result(GetCPUQueryResults(kDummyQuery),
                                base::TimeTicks::Now())));

  // Third interval. The fake `frame8` should now be included using 40% CPU for
  // the entire interval.
  task_env().FastForwardBy(kTimeBetweenMeasurements);
  EXPECT_THAT(proportion_tracker.StartNextInterval(
                  base::TimeTicks::Now(),
                  add_fake_result(GetCPUQueryResults(kDummyQuery),
                                  base::TimeTicks::Now())),
              Contains(Pair(frame8->GetResourceContext(), 0.4)));

  cpu_monitor_.RepeatingQueryStopped(kDummyQuery);
}

namespace {

resource_attribution::CPUTimeResult CreateCPUTimeResult(
    base::TimeDelta cumulative_cpu,
    base::TimeDelta cumulative_background_cpu) {
  return resource_attribution::CPUTimeResult{
      .metadata = resource_attribution::ResultMetadata(
          /* measurement_time=*/base::TimeTicks::Now(),
          resource_attribution::MeasurementAlgorithm::kSum),
      .start_time = base::TimeTicks(),
      .cumulative_cpu = cumulative_cpu,
      .cumulative_background_cpu = cumulative_background_cpu};
}

}  // namespace

// Tests the CPUProportionTracker helper class when configured to use cumulative
// background CPU instead of cumulative CPU.
TEST_F(ResourceAttrCPUMonitorTest, CPUProportionTrackerBackground) {
  CPUProportionTracker tracker(
      base::NullCallback(),
      CPUProportionTracker::CPUProportionType::kBackground);

  const OriginInBrowsingInstanceContext context(kOrigin1,
                                                kBrowsingInstanceForPage);

  {
    resource_attribution::QueryResultMap cpu_result_map;
    cpu_result_map[context] = resource_attribution::QueryResults{
        .cpu_time_result =
            CreateCPUTimeResult(base::Seconds(60), base::Seconds(60))};
    tracker.StartFirstInterval(base::TimeTicks::Now(), cpu_result_map);
  }

  task_env().FastForwardBy(base::Seconds(60));

  resource_attribution::QueryResultMap cpu_result_map;
  cpu_result_map[context] = resource_attribution::QueryResults{
      .cpu_time_result =
          CreateCPUTimeResult(/*cumulative_cpu=*/base::Seconds(120),
                              /*cumulative_background_cpu=*/base::Seconds(90))};
  auto cpu_proportion_map =
      tracker.StartNextInterval(base::TimeTicks::Now(), cpu_result_map);

  EXPECT_EQ(cpu_proportion_map.size(), 1U);
  EXPECT_EQ(cpu_proportion_map[context], 0.5);
}

// Tests that multiple CPUProportionTrackers with different schedules are
// independent. Also tests trackers with and without a context filter.
TEST_F(ResourceAttrCPUMonitorTest, MultipleCPUProportionTrackers) {
  performance_manager::MockMultiplePagesWithMultipleProcessesGraph mock_graph(
      graph());
  SetProcessCPUUsage(mock_graph.process.get(), 1.0);
  SetProcessCPUUsage(mock_graph.other_process.get(), 1.0);

  // Helper to return expected results for all nodes in `mock_graph`.
  auto get_all_expected_results = [&](double process_cpu) {
    // `other_process` is fixed at 100%.
    const double other_process_cpu = 1.0;
    // `frame` and `other_frame` get 1/2 of `process`.
    const double frame_cpu = process_cpu / 2;
    const double other_frame_cpu = process_cpu / 2;
    // `child_frame` gets all of `other_process`.
    const double child_frame_cpu = other_process_cpu;

    return std::map<ResourceContext, double>{
        {mock_graph.process->GetResourceContext(), process_cpu},
        {mock_graph.other_process->GetResourceContext(), other_process_cpu},
        {mock_graph.frame->GetResourceContext(), frame_cpu},
        {mock_graph.other_frame->GetResourceContext(), other_frame_cpu},
        {mock_graph.child_frame->GetResourceContext(), child_frame_cpu},
        // `page` contains only `frame`.
        {mock_graph.page->GetResourceContext(), frame_cpu},
        // `other_page` contains `other_frame` and `child_frame`.
        {mock_graph.other_page->GetResourceContext(),
         other_frame_cpu + child_frame_cpu},
        // `browser_process` is fixed at 100%.
        {mock_graph.browser_process->GetResourceContext(), 1.0},
    };
  };

  // T = 0
  StartMonitoring();

  // Tracker that watches all contexts, with a 1 minute interval.
  CPUProportionTracker all_tracker;
  all_tracker.StartFirstInterval(base::TimeTicks::Now(), GetCPUQueryResults());

  // T = 15s
  // `process` CPU drops to 50%.
  task_env().FastForwardBy(base::Seconds(15));
  SetProcessCPUUsage(mock_graph.process.get(), 0.5);

  // Tracker that watches only processes. It starts 15 seconds later, with a 30
  // second interval.
  CPUProportionTracker process_tracker(
      base::BindRepeating(&ContextIs<ProcessContext>));
  process_tracker.StartFirstInterval(base::TimeTicks::Now(),
                                     GetCPUQueryResults());

  // `other_process` and `browser_process` CPU are fixed at 100%. `process` CPU
  // will vary.
  std::map<ResourceContext, double> expected_process_results{
      {mock_graph.other_process->GetResourceContext(), 1.0},
      {mock_graph.browser_process->GetResourceContext(), 1.0},
  };

  // T = 30s
  // `process` CPU drops to 40%.
  task_env().FastForwardBy(base::Seconds(15));
  SetProcessCPUUsage(mock_graph.process.get(), 0.4);

  // T = 45s
  // End of `process_tracker` 1st interval.
  task_env().FastForwardBy(base::Seconds(15));

  // `process` used 50% CPU for first half, 40% for second half.
  expected_process_results[mock_graph.process->GetResourceContext()] =
      0.5 / 2 + 0.4 / 2;
  EXPECT_EQ(expected_process_results,
            process_tracker.StartNextInterval(base::TimeTicks::Now(),
                                              GetCPUQueryResults()));

  // T = 60s
  // End of `all_tracker` 1st interval.
  task_env().FastForwardBy(base::Seconds(15));

  // `process` used 100% CPU for 1/4, 50% for 1/4, 40% for 1/2.
  EXPECT_EQ(get_all_expected_results(1.0 / 4 + 0.5 / 4 + 0.4 / 2),
            all_tracker.StartNextInterval(base::TimeTicks::Now(),
                                          GetCPUQueryResults()));

  // T = 75s
  // End of `process_tracker` 2nd interval.
  task_env().FastForwardBy(base::Seconds(15));

  // `process` used 40% CPU for whole interval.
  expected_process_results[mock_graph.process->GetResourceContext()] = 0.4;
  EXPECT_EQ(expected_process_results,
            process_tracker.StartNextInterval(base::TimeTicks::Now(),
                                              GetCPUQueryResults()));

  // T = 90s
  // `process` CPU returns to 100%.
  task_env().FastForwardBy(base::Seconds(15));
  SetProcessCPUUsage(mock_graph.process.get(), 1.0);

  // T = 105s
  // End of `process_tracker` 3rd interval.
  task_env().FastForwardBy(base::Seconds(15));

  // `process` used 40% CPU for first half, 100% for second half.
  expected_process_results[mock_graph.process->GetResourceContext()] =
      0.4 / 2 + 1.0 / 2;
  EXPECT_EQ(expected_process_results,
            process_tracker.StartNextInterval(base::TimeTicks::Now(),
                                              GetCPUQueryResults()));

  // T = 120s
  // End of `all_tracker` 2nd interval.
  task_env().FastForwardBy(base::Seconds(15));

  // `process` used 40% of CPU for first half, 100% for second half.
  EXPECT_EQ(get_all_expected_results(0.4 / 2 + 1.0 / 2),
            all_tracker.StartNextInterval(base::TimeTicks::Now(),
                                          GetCPUQueryResults()));
}

// A test that creates real processes, to verify that measurement works with the
// timing of real node creation.
class ResourceAttrCPUMonitorTimingTest
    : public performance_manager::PerformanceManagerTestHarness {
 protected:
  using Super = performance_manager::PerformanceManagerTestHarness;

  void SetUp() override {
    GetGraphFeatures().EnableResourceAttributionScheduler();
    Super::SetUp();
    performance_manager::RunInGraph([&](performance_manager::Graph* graph) {
      cpu_monitor_ = std::make_unique<CPUMeasurementMonitor>();
      cpu_monitor_->StartMonitoring(graph);
    });
  }

  void TearDown() override {
    performance_manager::RunInGraph([&] { cpu_monitor_.reset(); });
    Super::TearDown();
  }

  // Ensure some time passes to measure.
  void LetTimePass() {
    base::TestWaitableEvent().TimedWait(TestTimeouts::tiny_timeout());
  }

  std::unique_ptr<CPUMeasurementMonitor> cpu_monitor_;
};

TEST_F(ResourceAttrCPUMonitorTimingTest, ProcessLifetime) {
  SetContents(CreateTestWebContents());
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://www.example.com/"));

  const auto frame_context =
      FrameContext::FromRenderFrameHost(main_rfh()).value();
  base::WeakPtr<ProcessNode> process_node =
      PerformanceManager::GetProcessNodeForRenderProcessHost(process());
  base::WeakPtr<ProcessNode> browser_process_node =
      PerformanceManager::GetProcessNodeForBrowserProcess();

  // Assign results to a repeating query so that they're not dropped
  // immediately when nodes are removed.
  constexpr internal::QueryId kDummyQuery;

  // Since process() returns a MockRenderProcessHost, ProcessNode is created
  // but has no pid. (Equivalent to the time between OnProcessNodeAdded and
  // OnProcessLifetimeChange.)
  LetTimePass();
  performance_manager::RunInGraph([&] {
    cpu_monitor_->RepeatingQueryStarted(kDummyQuery);

    ASSERT_TRUE(process_node);
    EXPECT_EQ(process_node->GetProcessId(), base::kNullProcessId);

    // "Browser" process is the test harness, which already has a pid.
    ASSERT_TRUE(browser_process_node);
    EXPECT_NE(browser_process_node->GetProcessId(), base::kNullProcessId);

    // Renderer process can't be measured yet, browser can.
    const auto measurements =
        cpu_monitor_->UpdateAndGetCPUMeasurements(kDummyQuery);
    EXPECT_FALSE(
        base::Contains(measurements, process_node->GetResourceContext()));
    EXPECT_FALSE(base::Contains(measurements, frame_context));
    EXPECT_TRUE(base::Contains(measurements,
                               browser_process_node->GetResourceContext()));
  });

  // Assign a real process to the ProcessNode. (Will call
  // OnProcessLifetimeChange and start monitoring.)
  auto set_process_on_pm_sequence = [&process_node] {
    ASSERT_TRUE(process_node);
    ProcessNodeImpl::FromNode(process_node.get())
        ->SetProcess(base::Process::Current(), base::TimeTicks::Now());
    EXPECT_NE(process_node->GetProcessId(), base::kNullProcessId);
  };
  performance_manager::RunInGraph(set_process_on_pm_sequence);

  // Let some time pass so there's CPU to measure after monitoring starts.
  LetTimePass();

  auto get_cumulative_cpu =
      [](const QueryResultMap& measurements,
         const ResourceContext& context) -> base::TimeDelta {
    return measurements.at(context).cpu_time_result->cumulative_cpu;
  };

  base::TimeDelta cumulative_process_cpu;
  base::TimeDelta cumulative_browser_process_cpu;
  base::TimeDelta cumulative_frame_cpu;
  performance_manager::RunInGraph([&] {
    ASSERT_TRUE(process_node);
    ASSERT_TRUE(browser_process_node);
    EXPECT_TRUE(process_node->GetProcess().IsValid());
    EXPECT_TRUE(browser_process_node->GetProcess().IsValid());

    // Both processes can be measured now.
    const auto measurements =
        cpu_monitor_->UpdateAndGetCPUMeasurements(kDummyQuery);

    ASSERT_TRUE(
        base::Contains(measurements, process_node->GetResourceContext()));
    cumulative_process_cpu =
        get_cumulative_cpu(measurements, process_node->GetResourceContext());
    EXPECT_FALSE(cumulative_process_cpu.is_negative());

    ASSERT_TRUE(base::Contains(measurements,
                               browser_process_node->GetResourceContext()));
    cumulative_browser_process_cpu = get_cumulative_cpu(
        measurements, browser_process_node->GetResourceContext());
    EXPECT_FALSE(cumulative_browser_process_cpu.is_negative());

    ASSERT_TRUE(base::Contains(measurements, frame_context));
    cumulative_frame_cpu = get_cumulative_cpu(measurements, frame_context);
    EXPECT_FALSE(cumulative_frame_cpu.is_negative());
  });

  // Simulate that the renderer process died.
  process()->SimulateRenderProcessExit(
      base::TERMINATION_STATUS_NORMAL_TERMINATION, 0);
  LetTimePass();
  performance_manager::RunInGraph([&] {
    // Process is no longer running, so can't be measured.
    ASSERT_TRUE(process_node);
    EXPECT_FALSE(process_node->GetProcess().IsValid());

    // CPUMeasurementMonitor will return the last measured usage of the process
    // and its main frame for one query with ID kDummyQuery after the FrameNode
    // is deleted.
    const auto measurements =
        cpu_monitor_->UpdateAndGetCPUMeasurements(kDummyQuery);

    ASSERT_TRUE(
        base::Contains(measurements, process_node->GetResourceContext()));
    const base::TimeDelta new_process_cpu =
        get_cumulative_cpu(measurements, process_node->GetResourceContext());
    EXPECT_GE(new_process_cpu, cumulative_process_cpu);
    cumulative_process_cpu = new_process_cpu;

    ASSERT_TRUE(base::Contains(measurements, frame_context));
    const base::TimeDelta new_frame_cpu =
        get_cumulative_cpu(measurements, frame_context);
    EXPECT_GE(new_frame_cpu, cumulative_frame_cpu);
    cumulative_frame_cpu = new_frame_cpu;
  });

  // Assign a new process to the same ProcessNode. This should add the CPU usage
  // of the new process to the existing CPU usage of the process. The frame
  // should NOT be included in the new result, since it's no longer live.
  // (Navigating the renderer will create a new frame tree in that process.)
  EXPECT_FALSE(main_rfh()->IsRenderFrameLive());
  EXPECT_TRUE(process()->MayReuseHost());
  performance_manager::RunInGraph(set_process_on_pm_sequence);

  LetTimePass();
  performance_manager::RunInGraph([&] {
    ASSERT_TRUE(process_node);
    EXPECT_TRUE(process_node->GetProcess().IsValid());

    const auto measurements =
        cpu_monitor_->UpdateAndGetCPUMeasurements(kDummyQuery);

    ASSERT_TRUE(
        base::Contains(measurements, process_node->GetResourceContext()));
    const base::TimeDelta new_process_cpu =
        get_cumulative_cpu(measurements, process_node->GetResourceContext());
    EXPECT_GE(new_process_cpu, cumulative_process_cpu);
    cumulative_process_cpu = new_process_cpu;

    EXPECT_FALSE(base::Contains(measurements, frame_context));

    cpu_monitor_->RepeatingQueryStopped(kDummyQuery);
  });
}

}  // namespace resource_attribution
