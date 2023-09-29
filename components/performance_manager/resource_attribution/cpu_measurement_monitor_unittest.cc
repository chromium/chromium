// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/resource_attribution/cpu_measurement_monitor.h"

#include <map>
#include <memory>
#include <ostream>
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
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/test/test_waitable_event.h"
#include "base/time/time.h"
#include "components/performance_manager/embedder/graph_features.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/graph/worker_node_impl.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/public/resource_attribution/frame_context_registry.h"
#include "components/performance_manager/public/resource_attribution/query_results.h"
#include "components/performance_manager/public/resource_attribution/resource_contexts.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/test_support/mock_graphs.h"
#include "components/performance_manager/test_support/performance_manager_test_harness.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/process_type.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/navigation_simulator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/gurl.h"

namespace performance_manager::resource_attribution {

// Test result printers. These need to go in the same namespace as the type
// being printed.

std::ostream& operator<<(std::ostream& os, const ResultMetadata& metadata) {
  return os << "measurement_time:" << metadata.measurement_time;
}

std::ostream& operator<<(std::ostream& os, const CPUTimeResult& result) {
  return os << "cpu:" << result.cumulative_cpu
            << ",start_time:" << result.start_time
            << ",metadata:" << result.metadata << " ("
            << (result.metadata.measurement_time - result.start_time) << ")";
}

std::ostream& operator<<(std::ostream& os, const ResourceContext& context) {
  absl::visit([&os](const auto& token) { os << token.ToString(); }, context);
  return os;
}

namespace {

using ::testing::AllOf;
using ::testing::Conditional;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::Optional;

constexpr base::TimeDelta kTimeBetweenMeasurements = base::Minutes(5);

// State of a simulated process for CPU measurements.
class SimulatedCPUMeasurementDelegate final
    : public CPUMeasurementMonitor::CPUMeasurementDelegate {
 public:
  struct CPUUsagePeriod {
    base::TimeTicks start_time;
    base::TimeTicks end_time;
    double cpu_usage;
  };

  explicit SimulatedCPUMeasurementDelegate(
      base::OnceClosure unregister_callback)
      : unregister_callback_(std::move(unregister_callback)) {}

  ~SimulatedCPUMeasurementDelegate() final {
    std::move(unregister_callback_).Run();
  }

  // Returns the simulated CPU usage of the process by summing
  // `cpu_usage_periods`.
  base::TimeDelta GetCumulativeCPUUsage() final;

  // List of periods of varying CPU usage.
  std::vector<CPUUsagePeriod> cpu_usage_periods;

  // If not nullopt, GetCumulativeCPUUsage() will ignore `cpu_usage_periods` and
  // return this value to simulate an error.
  absl::optional<base::TimeDelta> usage_error;

 private:
  base::OnceClosure unregister_callback_;
};

base::TimeDelta SimulatedCPUMeasurementDelegate::GetCumulativeCPUUsage() {
  if (usage_error.has_value()) {
    return usage_error.value();
  }
  base::TimeDelta cumulative_usage;
  for (const auto& usage_period : cpu_usage_periods) {
    CHECK(!usage_period.start_time.is_null());
    // The last interval in the list will have no end time.
    const base::TimeTicks end_time = usage_period.end_time.is_null()
                                         ? base::TimeTicks::Now()
                                         : usage_period.end_time;
    CHECK(end_time >= usage_period.start_time);
    cumulative_usage +=
        (end_time - usage_period.start_time) * usage_period.cpu_usage;
  }
  return cumulative_usage;
}

void RunOnPMSequence(base::OnceClosure closure) {
  base::RunLoop run_loop;
  PerformanceManager::CallOnGraph(
      FROM_HERE, base::BindLambdaForTesting([&run_loop, &closure] {
        std::move(closure).Run();
        run_loop.Quit();
      }));
  run_loop.Run();
}

void RunOnPMSequence(base::OnceCallback<void(Graph*)> callback) {
  base::RunLoop run_loop;
  PerformanceManager::CallOnGraph(
      FROM_HERE,
      base::BindLambdaForTesting([&run_loop, &callback](Graph* graph) {
        std::move(callback).Run(graph);
        run_loop.Quit();
      }));
  run_loop.Run();
}

}  // namespace

// A test that creates mock processes to simulate exact CPU usage.
class CPUMeasurementMonitorTest : public GraphTestHarness {
 protected:
  using Super = GraphTestHarness;

  void SetUp() override {
    GetGraphFeatures().EnableResourceAttributionRegistries();
    Super::SetUp();
    cpu_monitor_.SetCPUMeasurementDelegateFactoryForTesting(base::BindRepeating(
        &CPUMeasurementMonitorTest::CPUMeasurementDelegateFactory,
        base::Unretained(this)));
  }

  // Creates a renderer process and starts mocking its CPU measurements. By
  // default the process will use 100% CPU as long as it's alive.
  TestNodeWrapper<ProcessNodeImpl> CreateMockCPURenderer() {
    // CreateNode's default arguments create a renderer process node.
    auto process_node = CreateNode<ProcessNodeImpl>();
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
    SetProcessCPUUsageError(process_node, base::TimeDelta());
  }

  void SetProcessCPUUsage(const ProcessNodeImpl* process_node, double usage) {
    SimulatedCPUMeasurementDelegate::CPUUsagePeriod usage_period{
        .start_time = base::TimeTicks::Now(),
        .cpu_usage = usage,
    };
    auto& delegate = GetOrCreateCPUMeasurementDelegate(process_node);
    if (!delegate.cpu_usage_periods.empty()) {
      delegate.cpu_usage_periods.back().end_time = usage_period.start_time;
    }
    delegate.cpu_usage_periods.push_back(std::move(usage_period));
  }

  void SetProcessCPUUsageError(const ProcessNodeImpl* process_node,
                               absl::optional<base::TimeDelta> usage_error) {
    GetOrCreateCPUMeasurementDelegate(process_node).usage_error = usage_error;
  }

  std::unique_ptr<SimulatedCPUMeasurementDelegate>
  CreateSimulatedCPUMeasurementDelegate(const ProcessNode* process_node) {
    CHECK(!base::Contains(pending_cpu_delegates_, process_node));
    CHECK(!base::Contains(simulated_cpu_delegates_, process_node));
    auto delegate = std::make_unique<SimulatedCPUMeasurementDelegate>(
        // Clear pointers to this delegate when it's deleted.
        base::BindLambdaForTesting([this, process_node] {
          this->simulated_cpu_delegates_.erase(process_node);
        }));
    simulated_cpu_delegates_.emplace(process_node, delegate.get());
    return delegate;
  }

  std::unique_ptr<CPUMeasurementMonitor::CPUMeasurementDelegate>
  CPUMeasurementDelegateFactory(const ProcessNode* process_node) {
    auto it = pending_cpu_delegates_.find(process_node);
    if (it != pending_cpu_delegates_.end()) {
      auto delegate = std::move(it->second);
      pending_cpu_delegates_.erase(it);
      return delegate;
    }
    return CreateSimulatedCPUMeasurementDelegate(process_node);
  }

  SimulatedCPUMeasurementDelegate& GetOrCreateCPUMeasurementDelegate(
      const ProcessNodeImpl* process_node) {
    auto it = simulated_cpu_delegates_.find(process_node);
    if (it != simulated_cpu_delegates_.end()) {
      return *(it->second);
    }
    CHECK(!base::Contains(pending_cpu_delegates_, process_node));
    auto new_delegate = CreateSimulatedCPUMeasurementDelegate(process_node);
    auto* delegate_ptr = new_delegate.get();
    CHECK_EQ(simulated_cpu_delegates_.at(process_node), delegate_ptr);
    pending_cpu_delegates_.emplace(process_node, std::move(new_delegate));
    return *delegate_ptr;
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
  void UpdateAndGetCPUMeasurements() {
    last_measurements_ = current_measurements_;
    current_measurements_ = cpu_monitor_.UpdateAndGetCPUMeasurements();
  }

  // GMock matcher expecting that a given CPUTimeResult equals
  // `last_measurements_[context] + expected_delta`. That is, since the last
  // time `context` was tested, expect that `expected_delta` was added to its
  // CPU measurement, which was taken at `expected_measurement_time`.
  auto CPUDeltaMatches(const ResourceContext& context,
                       base::TimeDelta expected_delta,
                       base::TimeTicks expected_measurement_time =
                           base::TimeTicks::Now()) const {
    base::TimeDelta expected_cpu = expected_delta;
    base::TimeTicks expected_start_time;
    const auto last_it = last_measurements_.find(context);
    if (last_it != last_measurements_.end()) {
      expected_cpu += last_it->second.cumulative_cpu;
      expected_start_time = last_it->second.start_time;
    }
    return AllOf(
        Field("metadata", &CPUTimeResult::metadata,
              Field("measurement_time", &ResultMetadata::measurement_time,
                    expected_measurement_time)),
        Field("cumulative_cpu", &CPUTimeResult::cumulative_cpu, expected_cpu),
        // `start_time` should not change. If this was the first measurement,
        // allow any non-null `start_time`. Note Conditional() doesn't
        // short-circuit, so the first branch will always be evaluated and can't
        // dereference `last_it`, which is why `expected_start_time` is put in a
        // temporary.
        Field("start_time", &CPUTimeResult::start_time,
              Conditional(last_it != last_measurements_.end(),
                          expected_start_time, Not(base::TimeTicks()))));
  }

  // GMock matcher expecting that a given CPUTimeResult has the given
  // `expected_start_time`.
  auto StartTimeMatches(base::TimeTicks expected_start_time) const {
    return Field("start_time", &CPUTimeResult::start_time, expected_start_time);
  }

  CPUMeasurementMonitor cpu_monitor_;

  // Map of ProcessNode to CPUMeasurementDelegate that simulates that process.
  // The delegates are owned by `cpu_monitor_` or `pending_cpu_delegates_`.
  std::map<const ProcessNode*, SimulatedCPUMeasurementDelegate*>
      simulated_cpu_delegates_;

  // CPUMeasurementDelegates that have been created but not passed to
  // `cpu_monitor_` yet.
  std::map<const ProcessNode*, std::unique_ptr<SimulatedCPUMeasurementDelegate>>
      pending_cpu_delegates_;

  // Cached results from UpdateAndGetCPUMeasurements(). Most tests will validate
  // the difference between the "last" and "current" measurements, which is
  // easier to follow than the full cumulative measurements at any given time.
  std::map<ResourceContext, CPUTimeResult> last_measurements_;
  std::map<ResourceContext, CPUTimeResult> current_measurements_;
};

// Tests that renderers created at various points around CPU measurement
// snapshots are handled correctly.
TEST_F(CPUMeasurementMonitorTest, CreateTiming) {
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
  ASSERT_EQ(renderer2->process_id(), base::kNullProcessId);

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
                              early_exit_renderer->resource_context()));
  EXPECT_THAT(current_measurements_[renderer1->resource_context()],
              AllOf(CPUDeltaMatches(renderer1->resource_context(),
                                    kTimeBetweenMeasurements),
                    StartTimeMatches(renderer1_start_time)));
  EXPECT_THAT(current_measurements_[renderer2->resource_context()],
              AllOf(CPUDeltaMatches(renderer2->resource_context(),
                                    kTimeBetweenMeasurements / 2),
                    StartTimeMatches(renderer2_start_time)));
  EXPECT_THAT(current_measurements_[renderer3->resource_context()],
              AllOf(CPUDeltaMatches(renderer3->resource_context(),
                                    kTimeBetweenMeasurements / 2),
                    StartTimeMatches(renderer3_start_time)));
  EXPECT_FALSE(
      base::Contains(current_measurements_, renderer4->resource_context()));
  EXPECT_FALSE(
      base::Contains(current_measurements_, renderer5->resource_context()));

  SetProcessId(renderer5.get());
  const auto renderer5_start_time = base::TimeTicks::Now();

  // Finish next measurement interval.
  task_env().FastForwardBy(kTimeBetweenMeasurements);

  // All nodes existed for entire measurement interval.
  UpdateAndGetCPUMeasurements();

  EXPECT_THAT(
      current_measurements_[renderer1->resource_context()],
      CPUDeltaMatches(renderer1->resource_context(), kTimeBetweenMeasurements));
  EXPECT_THAT(
      current_measurements_[renderer2->resource_context()],
      CPUDeltaMatches(renderer2->resource_context(), kTimeBetweenMeasurements));
  EXPECT_THAT(
      current_measurements_[renderer3->resource_context()],
      CPUDeltaMatches(renderer3->resource_context(), kTimeBetweenMeasurements));
  EXPECT_THAT(current_measurements_[renderer4->resource_context()],
              AllOf(CPUDeltaMatches(renderer4->resource_context(),
                                    kTimeBetweenMeasurements),
                    StartTimeMatches(renderer4_start_time)));
  EXPECT_THAT(current_measurements_[renderer5->resource_context()],
              AllOf(CPUDeltaMatches(renderer5->resource_context(),
                                    kTimeBetweenMeasurements),
                    StartTimeMatches(renderer5_start_time)));
}

// Tests that renderers exiting at various points around CPU measurement
// snapshots are handled correctly.
TEST_F(CPUMeasurementMonitorTest, ExitTiming) {
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
      base::Contains(current_measurements_, renderer1->resource_context()));
  EXPECT_FALSE(
      base::Contains(current_measurements_, renderer2->resource_context()));
  EXPECT_FALSE(
      base::Contains(current_measurements_, renderer3->resource_context()));

  // Remaining renderers are using 100% CPU.
  EXPECT_THAT(
      current_measurements_[renderer4->resource_context()],
      CPUDeltaMatches(renderer4->resource_context(), kTimeBetweenMeasurements));
  EXPECT_THAT(
      current_measurements_[renderer5->resource_context()],
      CPUDeltaMatches(renderer5->resource_context(), kTimeBetweenMeasurements));
  EXPECT_THAT(
      current_measurements_[renderer6->resource_context()],
      CPUDeltaMatches(renderer6->resource_context(), kTimeBetweenMeasurements));
  EXPECT_THAT(
      current_measurements_[renderer7->resource_context()],
      CPUDeltaMatches(renderer7->resource_context(), kTimeBetweenMeasurements));
  EXPECT_THAT(
      current_measurements_[renderer8->resource_context()],
      CPUDeltaMatches(renderer8->resource_context(), kTimeBetweenMeasurements));

  // `renderer4` exits at the beginning of the next measurement interval.
  // `renderer5` exits halfway through.
  // `renderer6` exits at the end of the interval.
  SetProcessExited(renderer4.get());
  task_env().FastForwardBy(kTimeBetweenMeasurements / 2);
  SetProcessExited(renderer5.get());

  // Finish the measurement interval.
  task_env().FastForwardBy(kTimeBetweenMeasurements / 2);
  SetProcessExited(renderer6.get());

  // TODO(crbug.com/1410503): Processes that exited at any point during the
  // interval still return their last measurement before the interval, so
  // their delta is always empty. Capture the final CPU usage correctly, and
  // test that the renderers that have exited return their CPU usage for the
  // time they were alive and 0% for the rest of the measurement interval.
  UpdateAndGetCPUMeasurements();

  EXPECT_THAT(current_measurements_[renderer4->resource_context()],
              CPUDeltaMatches(renderer4->resource_context(), base::TimeDelta(),
                              previous_update_time));
  EXPECT_THAT(current_measurements_[renderer5->resource_context()],
              CPUDeltaMatches(renderer5->resource_context(), base::TimeDelta(),
                              previous_update_time));
  EXPECT_THAT(current_measurements_[renderer6->resource_context()],
              CPUDeltaMatches(renderer6->resource_context(), base::TimeDelta(),
                              previous_update_time));

  EXPECT_THAT(
      current_measurements_[renderer7->resource_context()],
      CPUDeltaMatches(renderer7->resource_context(), kTimeBetweenMeasurements));
  EXPECT_THAT(
      current_measurements_[renderer8->resource_context()],
      CPUDeltaMatches(renderer8->resource_context(), kTimeBetweenMeasurements));

  // `renderer7` exits just before the StopMonitoring call and `renderer7`
  // exits just after. This should not cause any assertion failures.
  SetProcessExited(renderer7.get());
  cpu_monitor_.StopMonitoring();
  SetProcessExited(renderer8.get());
}

// Tests that varying CPU usage between measurement snapshots is reported
// correctly.
TEST_F(CPUMeasurementMonitorTest, VaryingMeasurements) {
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

  EXPECT_THAT(
      current_measurements_[renderer1->resource_context()],
      CPUDeltaMatches(renderer1->resource_context(), kTimeBetweenMeasurements));
  EXPECT_THAT(
      current_measurements_[renderer2->resource_context()],
      CPUDeltaMatches(renderer2->resource_context(), kTimeBetweenMeasurements));
  EXPECT_THAT(
      current_measurements_[renderer3->resource_context()],
      CPUDeltaMatches(renderer3->resource_context(), kTimeBetweenMeasurements));
  EXPECT_THAT(
      current_measurements_[renderer4->resource_context()],
      CPUDeltaMatches(renderer4->resource_context(), kTimeBetweenMeasurements));

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

  EXPECT_THAT(current_measurements_[renderer1->resource_context()],
              CPUDeltaMatches(renderer1->resource_context(),
                              kTimeBetweenMeasurements * 0.5));
  EXPECT_THAT(current_measurements_[renderer2->resource_context()],
              CPUDeltaMatches(renderer2->resource_context(),
                              kTimeBetweenMeasurements * 0.75));
  EXPECT_THAT(current_measurements_[renderer3->resource_context()],
              CPUDeltaMatches(renderer3->resource_context(),
                              kTimeBetweenMeasurements * 0.25));
  EXPECT_THAT(
      current_measurements_[renderer4->resource_context()],
      CPUDeltaMatches(renderer4->resource_context(), kTimeBetweenMeasurements));
}

// Tests that CPU usage of processes is correctly distributed between frames and
// workers in those processes, and correctly aggregated to pages containing
// frames and workers from multiple processes.
TEST_F(CPUMeasurementMonitorTest, CPUDistribution) {
  MockUtilityAndMultipleRenderProcessesGraph mock_graph(graph());

  // Track CPU usage of the mock utility process to make sure that measuring it
  // doesn't crash. Currently only measurements of renderer processes are
  // stored anywhere, so there are no other expectations to verify.
  SetProcessCPUUsage(mock_graph.utility_process.get(), 0.7);

  SetProcessCPUUsage(mock_graph.process.get(), 0.6);
  SetProcessCPUUsage(mock_graph.other_process.get(), 0.5);

  StartMonitoring();
  const auto monitoring_start_time = base::TimeTicks::Now();

  // No measurements if no time has passed.
  UpdateAndGetCPUMeasurements();
  EXPECT_THAT(current_measurements_, IsEmpty());

  task_env().FastForwardBy(kTimeBetweenMeasurements);

  UpdateAndGetCPUMeasurements();

  const FrameContext& frame_context = mock_graph.frame->resource_context();
  const FrameContext& child_frame_context =
      mock_graph.child_frame->resource_context();
  const FrameContext& other_frame_context =
      mock_graph.other_frame->resource_context();
  const PageContext& page_context = mock_graph.page->resource_context();
  const PageContext& other_page_context =
      mock_graph.other_page->resource_context();
  const WorkerContext& worker_context = mock_graph.worker->resource_context();
  const WorkerContext& other_worker_context =
      mock_graph.other_worker->resource_context();
  const ProcessContext& process_context =
      mock_graph.process->resource_context();
  const ProcessContext& other_process_context =
      mock_graph.other_process->resource_context();

  // `process` splits its 60% CPU usage evenly between `frame`, `other_frame`
  // and `worker`. `other_process` splits its 50% CPU usage evenly between
  // `child_frame` and `other_worker`. See the chart in
  // MockMultiplePagesAndWorkersWithMultipleProcessesGraph.
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
              AllOf(CPUDeltaMatches(frame_context, split_process_cpu_delta),
                    StartTimeMatches(monitoring_start_time)));
  EXPECT_THAT(
      current_measurements_[other_frame_context],
      AllOf(CPUDeltaMatches(other_frame_context, split_process_cpu_delta),
            StartTimeMatches(monitoring_start_time)));
  EXPECT_THAT(current_measurements_[worker_context],
              AllOf(CPUDeltaMatches(worker_context, split_process_cpu_delta),
                    StartTimeMatches(monitoring_start_time)));

  EXPECT_THAT(
      current_measurements_[child_frame_context],
      AllOf(CPUDeltaMatches(child_frame_context, other_process_split_cpu_delta),
            StartTimeMatches(monitoring_start_time)));
  EXPECT_THAT(current_measurements_[other_worker_context],
              AllOf(CPUDeltaMatches(other_worker_context,
                                    other_process_split_cpu_delta),
                    StartTimeMatches(monitoring_start_time)));

  // `page` gets its CPU usage from the sum of `frame` and `worker`.
  // `other_page` gets the sum of `other_frame`, `child_frame` and
  // `other_worker`. See the chart in
  // MockMultiplePagesAndWorkersWithMultipleProcessesGraph.
  EXPECT_THAT(
      current_measurements_[page_context],
      AllOf(CPUDeltaMatches(page_context, kTimeBetweenMeasurements * 0.4),
            StartTimeMatches(monitoring_start_time)));
  EXPECT_THAT(
      current_measurements_[other_page_context],
      AllOf(CPUDeltaMatches(other_page_context, kTimeBetweenMeasurements * 0.7),
            StartTimeMatches(monitoring_start_time)));

  // Modify the CPU usage of each process, ensure all frames and workers are
  // updated.
  SetProcessCPUUsage(mock_graph.process.get(), 0.3);
  SetProcessCPUUsage(mock_graph.other_process.get(), 0.8);
  task_env().FastForwardBy(kTimeBetweenMeasurements);

  UpdateAndGetCPUMeasurements();

  // `process` splits its 30% CPU usage evenly between `frame`, `other_frame`
  // and `worker`. `other_process` splits its 80% CPU usage evenly between
  // `child_frame` and `other_worker`.
  split_process_cpu_delta = kTimeBetweenMeasurements * 0.1;
  other_process_split_cpu_delta = kTimeBetweenMeasurements * 0.4;

  EXPECT_THAT(current_measurements_[process_context],
              CPUDeltaMatches(process_context, kTimeBetweenMeasurements * 0.3));
  EXPECT_THAT(
      current_measurements_[other_process_context],
      CPUDeltaMatches(other_process_context, kTimeBetweenMeasurements * 0.8));

  EXPECT_THAT(current_measurements_[frame_context],
              CPUDeltaMatches(frame_context, split_process_cpu_delta));
  EXPECT_THAT(current_measurements_[other_frame_context],
              CPUDeltaMatches(other_frame_context, split_process_cpu_delta));
  EXPECT_THAT(current_measurements_[worker_context],
              CPUDeltaMatches(worker_context, split_process_cpu_delta));

  EXPECT_THAT(
      current_measurements_[child_frame_context],
      CPUDeltaMatches(child_frame_context, other_process_split_cpu_delta));
  EXPECT_THAT(
      current_measurements_[other_worker_context],
      CPUDeltaMatches(other_worker_context, other_process_split_cpu_delta));

  // `page` gets its CPU usage from the sum of `frame` and `worker`.
  // `other_page` gets the sum of `other_frame`, `child_frame` and
  // `other_worker`.
  EXPECT_THAT(current_measurements_[page_context],
              CPUDeltaMatches(page_context, kTimeBetweenMeasurements * 0.2));
  EXPECT_THAT(
      current_measurements_[other_page_context],
      CPUDeltaMatches(other_page_context, kTimeBetweenMeasurements * 0.9));

  // Drop CPU usage of `other_process` to 0%. Only advance part of the normal
  // measurement interval, to be sure that the percentage usage doesn't depend
  // on the length of the interval.
  constexpr base::TimeDelta kShortInterval = kTimeBetweenMeasurements / 3;
  SetProcessCPUUsage(mock_graph.other_process.get(), 0.0);
  task_env().FastForwardBy(kShortInterval);

  UpdateAndGetCPUMeasurements();

  // `process` splits its 30% CPU usage evenly between `frame`, `other_frame`
  // and `worker`. `other_process` splits its 0% CPU usage evenly between
  // `child_frame` and `other_worker`.
  split_process_cpu_delta = kShortInterval * 0.1;
  other_process_split_cpu_delta = base::TimeDelta();

  EXPECT_THAT(current_measurements_[process_context],
              CPUDeltaMatches(process_context, kShortInterval * 0.3));
  EXPECT_THAT(current_measurements_[other_process_context],
              CPUDeltaMatches(other_process_context, base::TimeDelta()));

  EXPECT_THAT(current_measurements_[frame_context],
              CPUDeltaMatches(frame_context, split_process_cpu_delta));
  EXPECT_THAT(current_measurements_[other_frame_context],
              CPUDeltaMatches(other_frame_context, split_process_cpu_delta));
  EXPECT_THAT(current_measurements_[worker_context],
              CPUDeltaMatches(worker_context, split_process_cpu_delta));

  EXPECT_THAT(
      current_measurements_[child_frame_context],
      CPUDeltaMatches(child_frame_context, other_process_split_cpu_delta));
  EXPECT_THAT(
      current_measurements_[other_worker_context],
      CPUDeltaMatches(other_worker_context, other_process_split_cpu_delta));

  // `page` gets its CPU usage from the sum of `frame` and `worker`.
  // `other_page` gets the sum of `other_frame`, `child_frame` and
  // `other_worker`.
  EXPECT_THAT(current_measurements_[page_context],
              CPUDeltaMatches(page_context, kShortInterval * 0.2));
  EXPECT_THAT(current_measurements_[other_page_context],
              CPUDeltaMatches(other_page_context, kShortInterval * 0.1));
}

// Tests that CPU usage of processes is correctly distributed between FrameNodes
// and WorkerNodes that are added and removed between measurements.
TEST_F(CPUMeasurementMonitorTest, AddRemoveNodes) {
  MockMultiplePagesAndWorkersWithMultipleProcessesGraph mock_graph(graph());

  SetProcessCPUUsage(mock_graph.process.get(), 0.6);
  SetProcessCPUUsage(mock_graph.other_process.get(), 0.5);

  // Advance the clock before monitoring starts, so that the process launch
  // times can be distinguished from the start of monitoring.
  task_env().FastForwardBy(kTimeBetweenMeasurements);

  StartMonitoring();

  const FrameContext& frame_context = mock_graph.frame->resource_context();
  const FrameContext& child_frame_context =
      mock_graph.child_frame->resource_context();
  const PageContext& page_context = mock_graph.page->resource_context();
  const ProcessContext& process_context =
      mock_graph.process->resource_context();
  const ProcessContext& other_process_context =
      mock_graph.other_process->resource_context();

  // `new_frame1` and `new_worker1` are added just after a measurement.
  // `new_frame2` and `new_worker2` are added between measurements.
  // `new_frame3` and `new_worker3` are added just before a measurement.
  //
  // Frames are added to `process` and workers are added to `other_process`, to
  // test that all processes are measured.
  //
  // Frames are part of `page`. Workers don't have clients, so aren't part of
  // any page.
  auto new_frame1 =
      CreateFrameNodeAutoId(mock_graph.process.get(), mock_graph.page.get());
  auto new_worker1 = CreateNode<WorkerNodeImpl>(
      WorkerNode::WorkerType::kDedicated, mock_graph.other_process.get());
  const auto new_frame1_context = new_frame1->resource_context();
  const auto new_worker1_context = new_worker1->resource_context();
  const auto node_added_time1 = base::TimeTicks::Now();

  task_env().FastForwardBy(kTimeBetweenMeasurements / 2);
  auto new_frame2 =
      CreateFrameNodeAutoId(mock_graph.process.get(), mock_graph.page.get());
  auto new_worker2 = CreateNode<WorkerNodeImpl>(
      WorkerNode::WorkerType::kDedicated, mock_graph.other_process.get());
  const auto new_frame2_context = new_frame2->resource_context();
  const auto new_worker2_context = new_worker2->resource_context();
  const auto node_added_time2 = base::TimeTicks::Now();

  task_env().FastForwardBy(kTimeBetweenMeasurements / 2);
  auto new_frame3 =
      CreateFrameNodeAutoId(mock_graph.process.get(), mock_graph.page.get());
  auto new_worker3 = CreateNode<WorkerNodeImpl>(
      WorkerNode::WorkerType::kDedicated, mock_graph.other_process.get());
  const auto new_frame3_context = new_frame3->resource_context();
  const auto new_worker3_context = new_worker3->resource_context();
  const auto node_added_time3 = base::TimeTicks::Now();

  UpdateAndGetCPUMeasurements();

  // For the first half of the period:
  // * `process` split its 60% CPU usage between 4 nodes:
  //   * `frame`, `other_frame`, `worker`, `new_frame1`
  //   * `frame`, `worker` and `new_frame1` are part of `page`
  // * `other_process` splits its 50% CPU usage between 3 nodes:
  //   * `child_frame`, `other_worker`, `new_worker1`
  //
  // For the last half the split is:
  // * `process` splits between 5 nodes:
  //   * `frame`, `other_frame`, `worker`, `new_frame1`, `new_frame2`
  //   * `frame`, `worker`, `new_frame1` and `new_frame2` are part of `page`
  // * `other_process` splits between 4 nodes:
  //   * `child_frame`, `other_worker`, `new_worker1`, `new_worker2`
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

  EXPECT_THAT(current_measurements_[process_context],
              CPUDeltaMatches(process_context, kTimeBetweenMeasurements * 0.6));
  EXPECT_THAT(
      current_measurements_[frame_context],
      CPUDeltaMatches(frame_context, process_4way_split + process_5way_split));
  EXPECT_THAT(current_measurements_[new_frame1_context],
              AllOf(CPUDeltaMatches(new_frame1_context,
                                    process_4way_split + process_5way_split),
                    StartTimeMatches(node_added_time1)));
  EXPECT_THAT(current_measurements_[new_frame2_context],
              AllOf(CPUDeltaMatches(new_frame2_context, process_5way_split),
                    StartTimeMatches(node_added_time2)));
  EXPECT_FALSE(base::Contains(current_measurements_, new_frame3_context));

  EXPECT_THAT(
      current_measurements_[other_process_context],
      CPUDeltaMatches(other_process_context, kTimeBetweenMeasurements * 0.5));
  EXPECT_THAT(
      current_measurements_[child_frame_context],
      CPUDeltaMatches(child_frame_context,
                      other_process_3way_split + other_process_4way_split));
  EXPECT_THAT(
      current_measurements_[new_worker1_context],
      AllOf(CPUDeltaMatches(new_worker1_context, other_process_3way_split +
                                                     other_process_4way_split),
            StartTimeMatches(node_added_time1)));
  EXPECT_THAT(
      current_measurements_[new_worker2_context],
      AllOf(CPUDeltaMatches(new_worker2_context, other_process_4way_split),
            StartTimeMatches(node_added_time2)));
  EXPECT_FALSE(base::Contains(current_measurements_, new_worker3_context));

  EXPECT_THAT(current_measurements_[page_context],
              CPUDeltaMatches(page_context, expected_page_delta));

  new_frame1.reset();
  new_worker1.reset();
  const auto node_removed_time1 = base::TimeTicks::Now();

  task_env().FastForwardBy(kTimeBetweenMeasurements / 2);
  new_frame2.reset();
  new_worker2.reset();
  const auto node_removed_time2 = base::TimeTicks::Now();

  task_env().FastForwardBy(kTimeBetweenMeasurements / 2);
  UpdateAndGetCPUMeasurements();

  // `new_frame1` and `new_worker1` were removed on the same tick as the
  // previous measurement, so don't contribute to CPU usage since then.
  //
  // For the first half of this period:
  // * `process` split its 60% CPU usage between 5 nodes:
  //   * `frame`, `other_frame`, `worker`, `new_frame2`, `new_frame3`
  //   * `frame`, `worker`, `new_frame2` and `new_frame3` are part of `page`
  // * `other_process` splits its 50% CPU usage between 4 nodes:
  //   * `child_frame`, `other_worker`, `new_worker2`, `new_worker3`
  //
  // For the last half the split is:
  // * `process` splits between 4 nodes:
  //   * `frame`, `other_frame`, `worker`, `new_frame3`
  //   * `frame`, `worker` and `new_frame3` are part of `page`
  // * `other_process` splits between 3 nodes:
  //   * `child_frame`, `other_worker`, `new_worker3`
  constexpr base::TimeDelta expected_page_delta2 =
      /*first half, 4 nodes*/ 4 * process_5way_split +
      /*second half, 3 nodes*/ 3 * process_4way_split;

  EXPECT_THAT(current_measurements_[process_context],
              CPUDeltaMatches(process_context, kTimeBetweenMeasurements * 0.6));
  EXPECT_THAT(
      current_measurements_[frame_context],
      CPUDeltaMatches(frame_context, process_5way_split + process_4way_split));
  EXPECT_THAT(
      current_measurements_[new_frame1_context],
      CPUDeltaMatches(new_frame1_context, base::TimeDelta(),
                      /*expected_measurement_time=*/node_removed_time1));
  EXPECT_THAT(
      current_measurements_[new_frame2_context],
      CPUDeltaMatches(new_frame2_context, process_5way_split,
                      /*expected_measurement_time=*/node_removed_time2));
  EXPECT_THAT(current_measurements_[new_frame3_context],
              AllOf(CPUDeltaMatches(new_frame3_context,
                                    process_5way_split + process_4way_split),
                    StartTimeMatches(node_added_time3)));

  EXPECT_THAT(
      current_measurements_[other_process_context],
      CPUDeltaMatches(other_process_context, kTimeBetweenMeasurements * 0.5));
  EXPECT_THAT(
      current_measurements_[child_frame_context],
      CPUDeltaMatches(child_frame_context,
                      other_process_4way_split + other_process_3way_split));
  EXPECT_THAT(
      current_measurements_[new_worker1_context],
      CPUDeltaMatches(new_worker1_context, base::TimeDelta(),
                      /*expected_measurement_time=*/node_removed_time1));
  EXPECT_THAT(
      current_measurements_[new_worker2_context],
      CPUDeltaMatches(new_worker2_context, other_process_4way_split,
                      /*expected_measurement_time=*/node_removed_time2));
  EXPECT_THAT(
      current_measurements_[new_worker3_context],
      AllOf(CPUDeltaMatches(new_worker3_context, other_process_4way_split +
                                                     other_process_3way_split),
            StartTimeMatches(node_added_time3)));

  EXPECT_THAT(current_measurements_[page_context],
              CPUDeltaMatches(page_context, expected_page_delta2));
}

// Tests that WorkerNode CPU usage is correctly distributed to pages as clients
// are added and removed.
TEST_F(CPUMeasurementMonitorTest, AddRemoveWorkerClients) {
  MockMultiplePagesAndWorkersWithMultipleProcessesGraph mock_graph(graph());

  SetProcessCPUUsage(mock_graph.process.get(), 0.6);
  SetProcessCPUUsage(mock_graph.other_process.get(), 0.5);

  StartMonitoring();

  const FrameContext& frame_context = mock_graph.frame->resource_context();
  const FrameContext& child_frame_context =
      mock_graph.child_frame->resource_context();
  const PageContext& page_context = mock_graph.page->resource_context();
  const PageContext& other_page_context =
      mock_graph.other_page->resource_context();

  auto new_worker1 = CreateNode<WorkerNodeImpl>(
      WorkerNode::WorkerType::kDedicated, mock_graph.process.get());
  const auto new_worker1_context = new_worker1->resource_context();
  auto new_worker2 = CreateNode<WorkerNodeImpl>(
      WorkerNode::WorkerType::kDedicated, mock_graph.other_process.get());
  const auto new_worker2_context = new_worker2->resource_context();

  task_env().FastForwardBy(kTimeBetweenMeasurements);
  UpdateAndGetCPUMeasurements();

  // During this interval:
  // * `process` split its 60% CPU usage between 4 nodes:
  //   * `frame`, `other_frame`, `worker`, `new_worker1`
  //   * `frame` and `worker` are part of `page`
  //   * `other_frame` is part of `other_page`
  // * `other_process` splits its 50% CPU usage between 3 nodes:
  //   * `child_frame`, `other_worker`, `new_worker2`
  //   * `child_frame` and `other_worker` are part of `other_page`
  constexpr base::TimeDelta process_split = kTimeBetweenMeasurements * 0.6 / 4;
  constexpr base::TimeDelta other_process_split =
      kTimeBetweenMeasurements * 0.5 / 3;

  EXPECT_THAT(current_measurements_[frame_context],
              CPUDeltaMatches(frame_context, process_split));
  EXPECT_THAT(current_measurements_[new_worker1_context],
              CPUDeltaMatches(new_worker1_context, process_split));

  EXPECT_THAT(current_measurements_[child_frame_context],
              CPUDeltaMatches(child_frame_context, other_process_split));
  EXPECT_THAT(current_measurements_[new_worker2_context],
              CPUDeltaMatches(new_worker2_context, other_process_split));

  EXPECT_THAT(current_measurements_[page_context],
              CPUDeltaMatches(page_context, 2 * process_split));
  EXPECT_THAT(current_measurements_[other_page_context],
              CPUDeltaMatches(other_page_context,
                              process_split + 2 * other_process_split));

  // Half-way through the interval, make `frame` a client of `new_worker1` and
  // `worker` a client of `new_worker2`.
  task_env().FastForwardBy(kTimeBetweenMeasurements / 2);
  new_worker1->AddClientFrame(mock_graph.frame.get());
  new_worker2->AddClientWorker(mock_graph.worker.get());

  task_env().FastForwardBy(kTimeBetweenMeasurements / 2);
  UpdateAndGetCPUMeasurements();

  // The split of CPU between frames and workers should not change. But, during
  // the second half of the interval, `page` contains 4 contexts:
  // * `frame`, `worker`, `new_worker1`, `new_worker2`
  constexpr base::TimeDelta expected_page_delta =
      /*first half, 2 nodes*/ (2 * process_split) / 2 +
      /*second half, 4 nodes*/ (3 * process_split + other_process_split) / 2;

  EXPECT_THAT(current_measurements_[frame_context],
              CPUDeltaMatches(frame_context, process_split));
  EXPECT_THAT(current_measurements_[new_worker1_context],
              CPUDeltaMatches(new_worker1_context, process_split));

  EXPECT_THAT(current_measurements_[child_frame_context],
              CPUDeltaMatches(child_frame_context, other_process_split));
  EXPECT_THAT(current_measurements_[new_worker2_context],
              CPUDeltaMatches(new_worker2_context, other_process_split));

  EXPECT_THAT(current_measurements_[page_context],
              CPUDeltaMatches(page_context, expected_page_delta));
  EXPECT_THAT(current_measurements_[other_page_context],
              CPUDeltaMatches(other_page_context,
                              process_split + 2 * other_process_split));

  // Half-way through the interval, make `other_worker` a client of
  // `new_worker2` instead of `worker`.
  task_env().FastForwardBy(kTimeBetweenMeasurements / 2);
  new_worker2->RemoveClientWorker(mock_graph.worker.get());
  new_worker2->AddClientWorker(mock_graph.other_worker.get());

  task_env().FastForwardBy(kTimeBetweenMeasurements / 2);
  UpdateAndGetCPUMeasurements();

  // The first half of the interval is unchanged (`page` contains 4 contexts,
  // `other_page` contains 3).
  //
  // During the second half of the interval, `page` contains 3 contexts:
  // * `frame`, `worker`, `new_worker1` (all in `process`)
  // And `other_page` contains 4 contexts:
  // * `other_frame` (in `process), `child_frame`, `other_worker`, `new_worker2`
  //   (in `other_process`)
  constexpr base::TimeDelta expected_page_delta2 =
      /*first half, 4 nodes*/ (3 * process_split + other_process_split) / 2 +
      /*second half, 3 nodes*/ (3 * process_split) / 2;
  constexpr base::TimeDelta expected_other_page_delta =
      /*first half, 3 nodes*/ (process_split + 2 * other_process_split) / 2 +
      /*second half, 4 nodes*/ (process_split + 3 * other_process_split) / 2;

  EXPECT_THAT(current_measurements_[page_context],
              CPUDeltaMatches(page_context, expected_page_delta2));
  EXPECT_THAT(current_measurements_[other_page_context],
              CPUDeltaMatches(other_page_context, expected_other_page_delta));

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
  // Now `page` contains 5 contexts (`frame` and all workers with `frame` as a
  // client:
  // * `frame`, `new_worker1`, `worker` (in `process`), `other_worker`,
  //   `new_worker2` (in `other_process`)
  // And `other_page` contains 4 contexts (`other_frame`, `child_frame`, and all
  // workers with either of them as a client:
  // * `other_frame` (in `process), `child_frame`, `other_worker`, `new_worker2`
  //   (in `other_process`)
  constexpr base::TimeDelta expected_page_delta3 =
      3 * process_split + 2 * other_process_split;
  constexpr base::TimeDelta expected_other_page_delta2 =
      process_split + 3 * other_process_split;

  task_env().FastForwardBy(kTimeBetweenMeasurements);
  UpdateAndGetCPUMeasurements();

  EXPECT_THAT(current_measurements_[page_context],
              CPUDeltaMatches(page_context, expected_page_delta3));
  EXPECT_THAT(current_measurements_[other_page_context],
              CPUDeltaMatches(other_page_context, expected_other_page_delta2));

  // Break the link between `new_worker2` and `new_worker1`. `new_worker2`
  // should still be in `page` because a path to `frame` still exists:
  // * `new_worker2` -> `other_worker` -> `new_worker1` -> `frame`
  new_worker2->RemoveClientWorker(new_worker1.get());

  task_env().FastForwardBy(kTimeBetweenMeasurements);
  UpdateAndGetCPUMeasurements();

  EXPECT_THAT(current_measurements_[page_context],
              CPUDeltaMatches(page_context, expected_page_delta3));
  EXPECT_THAT(current_measurements_[other_page_context],
              CPUDeltaMatches(other_page_context, expected_other_page_delta2));

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
}

// Tests that errors returned from ProcessMetrics are correctly ignored.
TEST_F(CPUMeasurementMonitorTest, MeasurementError) {
  const TestNodeWrapper<ProcessNodeImpl> renderer1 = CreateMockCPURenderer();
  SetProcessId(renderer1.get());
  const TestNodeWrapper<ProcessNodeImpl> renderer2 = CreateMockCPURenderer();
  SetProcessId(renderer2.get());
  const TestNodeWrapper<ProcessNodeImpl> renderer3 = CreateMockCPURenderer();
  SetProcessId(renderer3.get());

  // Advance the clock before monitoring starts, so that the process launch
  // times can be distinguished from the start of monitoring.
  task_env().FastForwardBy(kTimeBetweenMeasurements);

  StartMonitoring();
  const auto monitoring_start_time = base::TimeTicks::Now();

  // `renderer1` and `renderer2` measure 100% CPU usage. `renderer3` and
  // `renderer4` have errors before the first measurement. `renderer4` is
  // created after monitoring starts.
  task_env().FastForwardBy(kTimeBetweenMeasurements / 2);
  const TestNodeWrapper<ProcessNodeImpl> renderer4 = CreateMockCPURenderer();
  SetProcessId(renderer4.get());
  SetProcessCPUUsageError(renderer3.get(), base::TimeDelta::Min());
  SetProcessCPUUsageError(renderer4.get(), base::TimeDelta::Min());

  // Finish the measurement period.
  task_env().FastForwardBy(kTimeBetweenMeasurements / 2);
  UpdateAndGetCPUMeasurements();
  const auto previous_measurement_time = base::TimeTicks::Now();

  EXPECT_THAT(current_measurements_[renderer1->resource_context()],
              AllOf(CPUDeltaMatches(renderer1->resource_context(),
                                    kTimeBetweenMeasurements),
                    StartTimeMatches(monitoring_start_time)));
  EXPECT_THAT(current_measurements_[renderer2->resource_context()],
              AllOf(CPUDeltaMatches(renderer2->resource_context(),
                                    kTimeBetweenMeasurements),
                    StartTimeMatches(monitoring_start_time)));
  EXPECT_FALSE(
      base::Contains(current_measurements_, renderer3->resource_context()));
  EXPECT_FALSE(
      base::Contains(current_measurements_, renderer4->resource_context()));

  // Most platforms returns a zero TimeDelta on error.
  SetProcessCPUUsageError(renderer1.get(), base::TimeDelta());
  // Linux returns a negative TimeDelta on error.
  SetProcessCPUUsageError(renderer2.get(), base::TimeDelta::Min());

  task_env().FastForwardBy(kTimeBetweenMeasurements);
  UpdateAndGetCPUMeasurements();

  // After an error the previous measurement should be returned unchanged.
  EXPECT_THAT(current_measurements_[renderer1->resource_context()],
              CPUDeltaMatches(renderer1->resource_context(), base::TimeDelta(),
                              previous_measurement_time));
  EXPECT_THAT(current_measurements_[renderer2->resource_context()],
              CPUDeltaMatches(renderer2->resource_context(), base::TimeDelta(),
                              previous_measurement_time));
  EXPECT_FALSE(
      base::Contains(current_measurements_, renderer3->resource_context()));
  EXPECT_FALSE(
      base::Contains(current_measurements_, renderer4->resource_context()));

  SetProcessCPUUsageError(renderer1.get(), absl::nullopt);
  SetProcessCPUUsageError(renderer2.get(), absl::nullopt);
  SetProcessCPUUsageError(renderer3.get(), absl::nullopt);
  SetProcessCPUUsageError(renderer4.get(), absl::nullopt);

  task_env().FastForwardBy(kTimeBetweenMeasurements);
  UpdateAndGetCPUMeasurements();

  // The cumulative CPU usage to date includes the previous intervals which
  // weren't recorded due to the errors.
  EXPECT_THAT(current_measurements_[renderer1->resource_context()],
              CPUDeltaMatches(renderer1->resource_context(),
                              kTimeBetweenMeasurements * 2));
  EXPECT_THAT(current_measurements_[renderer2->resource_context()],
              CPUDeltaMatches(renderer2->resource_context(),
                              kTimeBetweenMeasurements * 2));
  EXPECT_THAT(current_measurements_[renderer3->resource_context()],
              AllOf(CPUDeltaMatches(renderer3->resource_context(),
                                    kTimeBetweenMeasurements * 3),
                    StartTimeMatches(monitoring_start_time)));
  // `renderer4` was created halfway through the first interval.
  EXPECT_THAT(current_measurements_[renderer4->resource_context()],
              AllOf(CPUDeltaMatches(renderer4->resource_context(),
                                    kTimeBetweenMeasurements * 2.5),
                    StartTimeMatches(monitoring_start_time +
                                     kTimeBetweenMeasurements / 2)));
}

// A test that creates real processes, to verify that measurement works with the
// timing of real node creation.
class CPUMeasurementMonitorTimingTest : public PerformanceManagerTestHarness {
 protected:
  using Super = PerformanceManagerTestHarness;

  void SetUp() override {
    GetGraphFeatures().EnableResourceAttributionRegistries();
    Super::SetUp();
    RunOnPMSequence(base::BindLambdaForTesting([&](Graph* graph) {
      cpu_monitor_ = std::make_unique<CPUMeasurementMonitor>();
      cpu_monitor_->StartMonitoring(graph);
    }));
  }

  void TearDown() override {
    RunOnPMSequence(base::BindLambdaForTesting([&] { cpu_monitor_.reset(); }));
    Super::TearDown();
  }

  // Ensure some time passes to measure.
  void LetTimePass() {
    base::TestWaitableEvent().TimedWait(TestTimeouts::tiny_timeout());
  }

  std::unique_ptr<CPUMeasurementMonitor> cpu_monitor_;
};

TEST_F(CPUMeasurementMonitorTimingTest, ProcessLifetime) {
  SetContents(CreateTestWebContents());
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://www.example.com/"));

  const FrameContext frame_context =
      FrameContextRegistry::ContextForRenderFrameHost(main_rfh()).value();
  base::WeakPtr<ProcessNode> process_node =
      PerformanceManager::GetProcessNodeForRenderProcessHost(process());

  // Since process() returns a MockRenderProcessHost, ProcessNode is created
  // but has no pid. (Equivalent to the time between OnProcessNodeAdded and
  // OnProcessLifetimeChange.)
  LetTimePass();
  RunOnPMSequence(base::BindLambdaForTesting([&] {
    ASSERT_TRUE(process_node);
    EXPECT_EQ(process_node->GetProcessId(), base::kNullProcessId);

    // Process can't be measured yet.
    const auto measurements = cpu_monitor_->UpdateAndGetCPUMeasurements();
    EXPECT_FALSE(
        base::Contains(measurements, process_node->GetResourceContext()));
    EXPECT_FALSE(base::Contains(measurements, frame_context));
  }));

  // Assign a real process to the ProcessNode. (Will call
  // OnProcessLifetimeChange and start monitoring.)
  auto set_process_on_pm_sequence = [&process_node] {
    ASSERT_TRUE(process_node);
    ProcessNodeImpl::FromNode(process_node.get())
        ->SetProcess(base::Process::Current(), base::TimeTicks::Now());
    EXPECT_NE(process_node->GetProcessId(), base::kNullProcessId);
  };
  RunOnPMSequence(base::BindLambdaForTesting(set_process_on_pm_sequence));

  // Let some time pass so there's CPU to measure after monitoring starts.
  LetTimePass();

  base::TimeDelta cumulative_process_cpu;
  base::TimeDelta cumulative_frame_cpu;
  RunOnPMSequence(base::BindLambdaForTesting([&] {
    ASSERT_TRUE(process_node);
    EXPECT_TRUE(process_node->GetProcess().IsValid());

    // Process can be measured now.
    const auto measurements = cpu_monitor_->UpdateAndGetCPUMeasurements();

    ASSERT_TRUE(
        base::Contains(measurements, process_node->GetResourceContext()));
    cumulative_process_cpu =
        measurements.at(process_node->GetResourceContext()).cumulative_cpu;
    EXPECT_FALSE(cumulative_process_cpu.is_negative());

    ASSERT_TRUE(base::Contains(measurements, frame_context));
    cumulative_frame_cpu = measurements.at(frame_context).cumulative_cpu;
    EXPECT_FALSE(cumulative_frame_cpu.is_negative());
  }));

  // Simulate that the process died.
  process()->SimulateRenderProcessExit(
      base::TERMINATION_STATUS_NORMAL_TERMINATION, 0);
  LetTimePass();
  RunOnPMSequence(base::BindLambdaForTesting([&] {
    // Process is no longer running, so can't be measured.
    ASSERT_TRUE(process_node);
    EXPECT_FALSE(process_node->GetProcess().IsValid());

    // CPUMeasurementMonitor will continue to return the last measured usage of
    // the process and its main frame.
    // TODO(crbug.com/1410503): Capture the final CPU usage correctly, and after
    // the main FrameNode is deleted, only cache it for the length of one query.
    const auto measurements = cpu_monitor_->UpdateAndGetCPUMeasurements();

    ASSERT_TRUE(
        base::Contains(measurements, process_node->GetResourceContext()));
    const base::TimeDelta new_process_cpu =
        measurements.at(process_node->GetResourceContext()).cumulative_cpu;
    EXPECT_GE(new_process_cpu, cumulative_process_cpu);
    cumulative_process_cpu = new_process_cpu;

    ASSERT_TRUE(base::Contains(measurements, frame_context));
    const base::TimeDelta new_frame_cpu =
        measurements.at(frame_context).cumulative_cpu;
    EXPECT_GE(new_frame_cpu, cumulative_frame_cpu);
    cumulative_frame_cpu = new_frame_cpu;
  }));

  // Assign a new process to the same renderer. This should add the CPU usage of
  // the new process to the existing CPU usage.
  EXPECT_TRUE(process()->MayReuseHost());
  RunOnPMSequence(base::BindLambdaForTesting(set_process_on_pm_sequence));

  LetTimePass();
  RunOnPMSequence(base::BindLambdaForTesting([&] {
    ASSERT_TRUE(process_node);
    EXPECT_TRUE(process_node->GetProcess().IsValid());

    const auto measurements = cpu_monitor_->UpdateAndGetCPUMeasurements();

    ASSERT_TRUE(
        base::Contains(measurements, process_node->GetResourceContext()));
    const base::TimeDelta new_process_cpu =
        measurements.at(process_node->GetResourceContext()).cumulative_cpu;
    EXPECT_GE(new_process_cpu, cumulative_process_cpu);
    cumulative_process_cpu = new_process_cpu;

    ASSERT_TRUE(base::Contains(measurements, frame_context));
    const base::TimeDelta new_frame_cpu =
        measurements.at(frame_context).cumulative_cpu;
    EXPECT_GE(new_frame_cpu, cumulative_frame_cpu);
    cumulative_frame_cpu = new_frame_cpu;
  }));
}

}  // namespace performance_manager::resource_attribution
