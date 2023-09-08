// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/resource_attribution/cpu_measurement_monitor.h"

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

std::ostream& operator<<(std::ostream& os,
                         const ResourceUsageResultMetadata& metadata) {
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

// A struct holding node pointers for a renderer process containing a single
// page with a single main frame.
struct SinglePageRendererNodes {
  TestNodeWrapper<ProcessNodeImpl> process_node;
  TestNodeWrapper<PageNodeImpl> page_node;
  TestNodeWrapper<FrameNodeImpl> frame_node;

  // Shorter aliases for each node's ResourceContext.
  ProcessContext process_context;
  PageContext page_context;
  FrameContext frame_context;
};

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
    Super::SetUp();

    mock_graph_ =
        std::make_unique<MockMultiplePagesAndWorkersWithMultipleProcessesGraph>(
            graph());
    mock_utility_process_ =
        CreateNode<ProcessNodeImpl>(content::PROCESS_TYPE_UTILITY);
    mock_utility_process_->SetProcess(base::Process::Current(),
                                      /*launch_time=*/base::TimeTicks::Now());

    cpu_monitor_.SetCPUMeasurementDelegateFactoryForTesting(base::BindRepeating(
        &CPUMeasurementMonitorTest::CPUMeasurementDelegateFactory,
        base::Unretained(this)));
  }

  // Creates a renderer process containing a single page and frame. CPU
  // measurements for the renderer are all assigned to that frame for easy
  // validation. By default the process will use 100% CPU as long as it's alive.
  SinglePageRendererNodes CreateSimpleCPUTrackingRenderer() {
    // CreateNode's default arguments create a renderer process node.
    auto process_node = CreateNode<ProcessNodeImpl>();
    ProcessContext process_context = process_node->resource_context();
    auto page_node = CreateNode<PageNodeImpl>();
    PageContext page_context = page_node->resource_context();
    auto frame_node =
        CreateFrameNodeAutoId(process_node.get(), page_node.get());
    FrameContext frame_context = frame_node->resource_context();

    // By default simulate 100% CPU usage in the renderer. To override this call
    // SetProcessCPUUsage again before advancing the clock.
    SetProcessCPUUsage(process_node.get(), 1.0);

    return SinglePageRendererNodes{
        .process_node = std::move(process_node),
        .page_node = std::move(page_node),
        .frame_node = std::move(frame_node),
        .process_context = std::move(process_context),
        .page_context = std::move(page_context),
        .frame_context = std::move(frame_context),
    };
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
                               base::TimeDelta usage_error) {
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
    last_page_cpu_usage_ = {};
    current_measurements_ = {};
    current_page_cpu_usage_ = {};
    cpu_monitor_.StartMonitoring(graph());
  }

  // Calls UpdateAndGetCPUMeasurements() on the CPUMeasurementMonitor under
  // test, and caches the results. Also caches the results of
  // EstimatePageCPUUsage() for all pages in the mock graph.
  void UpdateAndGetCPUMeasurements() {
    last_measurements_ = current_measurements_;
    last_page_cpu_usage_ = current_page_cpu_usage_;

    current_measurements_ = cpu_monitor_.UpdateAndGetCPUMeasurements();
    current_page_cpu_usage_[mock_graph_->page->resource_context()] =
        CPUMeasurementMonitor::EstimatePageCPUUsage(mock_graph_->page.get(),
                                                    current_measurements_);
    current_page_cpu_usage_[mock_graph_->other_page->resource_context()] =
        CPUMeasurementMonitor::EstimatePageCPUUsage(
            mock_graph_->other_page.get(), current_measurements_);
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
    const auto last_it = last_measurements_.find(context);
    if (last_it != last_measurements_.end()) {
      expected_cpu += last_it->second.cumulative_cpu;
    }
    return AllOf(
        Field("metadata", &CPUTimeResult::metadata,
              Field("measurement_time",
                    &ResourceUsageResultMetadata::measurement_time,
                    expected_measurement_time)),
        Field("cumulative_cpu", &CPUTimeResult::cumulative_cpu, expected_cpu),
        // `start_time` should not change. If this was the first measurement,
        // allow any non-null `start_time`.
        Field("start_time", &CPUTimeResult::start_time,
              Conditional(last_it != last_measurements_.end(),
                          last_it->second.start_time, Not(base::TimeTicks()))));
  }

  // GMock matcher expecting that a given CPUTimeResult has the given
  // `expected_start_time`.
  auto StartTimeMatches(base::TimeTicks expected_start_time) const {
    return Field("start_time", &CPUTimeResult::start_time, expected_start_time);
  }

  // Returns the change in CPU estimates of `page_context` since the previous
  // call to StartMonitoring() or UpdateAndGetCPUMeasurements(), or nullopt if
  // `page_context` has no cached estimates.
  absl::optional<base::TimeDelta> GetPageCPUUsageDelta(
      const PageContext& page_context) const {
    const auto current_it = current_page_cpu_usage_.find(page_context);
    if (current_it == current_page_cpu_usage_.end()) {
      // No measurement for this context.
      return absl::nullopt;
    }
    const auto last_it = last_page_cpu_usage_.find(page_context);
    if (last_it == last_page_cpu_usage_.end()) {
      // First measurement for this context.
      return current_it->second;
    }
    return current_it->second - last_it->second;
  }

  CPUMeasurementMonitor cpu_monitor_;

  TestNodeWrapper<ProcessNodeImpl> mock_utility_process_;

  // Map of ProcessNode to CPUMeasurementDelegate that simulates that process.
  // The delegates are owned by `cpu_monitor_` or `pending_cpu_delegates_`.
  std::map<const ProcessNode*, SimulatedCPUMeasurementDelegate*>
      simulated_cpu_delegates_;

  // CPUMeasurementDelegates that have been created but not passed to
  // `cpu_monitor_` yet.
  std::map<const ProcessNode*, std::unique_ptr<SimulatedCPUMeasurementDelegate>>
      pending_cpu_delegates_;

  std::unique_ptr<MockMultiplePagesAndWorkersWithMultipleProcessesGraph>
      mock_graph_;

  // Cached results from UpdateAndGetCPUMeasurements(). Most tests will validate
  // the difference between the "last" and "current" measurements, which is
  // easier to follow than the full cumulative measurements at any given time.
  std::map<ResourceContext, CPUTimeResult> last_measurements_;
  std::map<ResourceContext, CPUTimeResult> current_measurements_;
  std::map<PageContext, base::TimeDelta> last_page_cpu_usage_;
  std::map<PageContext, base::TimeDelta> current_page_cpu_usage_;
};

// Tests that renderers created at various points around CPU measurement
// snapshots are handled correctly.
TEST_F(CPUMeasurementMonitorTest, CreateTiming) {
  // Renderer in existence before StartMonitoring().
  const SinglePageRendererNodes renderer1 = CreateSimpleCPUTrackingRenderer();
  SetProcessId(renderer1.process_node.get());

  // Renderer starts and exits before StartMonitoring().
  const SinglePageRendererNodes early_exit_renderer =
      CreateSimpleCPUTrackingRenderer();
  SetProcessId(early_exit_renderer.process_node.get());
  SetProcessExited(early_exit_renderer.process_node.get());

  // Renderer creation racing with StartMonitoring(). Its pid will not be
  // available until after monitoring starts.
  const SinglePageRendererNodes renderer2 = CreateSimpleCPUTrackingRenderer();
  ASSERT_EQ(renderer2.process_node->process_id(), base::kNullProcessId);

  // `renderer1` begins measurement as soon as StartMonitoring is called.
  // `renderer2` begins measurement when its pid is available.
  StartMonitoring();
  const auto renderer1_start_time = base::TimeTicks::Now();

  task_env().FastForwardBy(kTimeBetweenMeasurements / 2);
  SetProcessId(renderer2.process_node.get());
  const auto renderer2_start_time = base::TimeTicks::Now();

  // Renderer created halfway through the measurement interval.
  const SinglePageRendererNodes renderer3 = CreateSimpleCPUTrackingRenderer();
  SetProcessId(renderer3.process_node.get());
  const auto renderer3_start_time = base::TimeTicks::Now();

  // Renderer creation racing with UpdateAndGetCPUMeasurements(). `renderer4`'s
  // pid will become available on the same tick the measurement is taken,
  // `renderer5`'s pid will become available after the measurement.
  const SinglePageRendererNodes renderer4 = CreateSimpleCPUTrackingRenderer();
  const SinglePageRendererNodes renderer5 = CreateSimpleCPUTrackingRenderer();

  // Finish next measurement interval.
  task_env().FastForwardBy(kTimeBetweenMeasurements / 2);
  SetProcessId(renderer4.process_node.get());
  const auto renderer4_start_time = base::TimeTicks::Now();

  // `renderer1` existed for the entire measurement period.
  // `renderer2` existed for all of it, but was only measured for the last half,
  // after its pid became available.
  // `renderer3` only existed for the last half.
  // `renderer4` existed for the measurement but no time passed so it had 0% CPU
  // usage.
  // `renderer5` is not measured yet.
  UpdateAndGetCPUMeasurements();

  EXPECT_FALSE(
      base::Contains(current_measurements_, early_exit_renderer.frame_context));
  EXPECT_THAT(
      current_measurements_[renderer1.frame_context],
      AllOf(CPUDeltaMatches(renderer1.frame_context, kTimeBetweenMeasurements),
            StartTimeMatches(renderer1_start_time)));
  EXPECT_THAT(current_measurements_[renderer2.frame_context],
              AllOf(CPUDeltaMatches(renderer2.frame_context,
                                    kTimeBetweenMeasurements / 2),
                    StartTimeMatches(renderer2_start_time)));
  EXPECT_THAT(current_measurements_[renderer3.frame_context],
              AllOf(CPUDeltaMatches(renderer3.frame_context,
                                    kTimeBetweenMeasurements / 2),
                    StartTimeMatches(renderer3_start_time)));
  EXPECT_FALSE(base::Contains(current_measurements_, renderer4.frame_context));
  EXPECT_FALSE(base::Contains(current_measurements_, renderer5.frame_context));

  SetProcessId(renderer5.process_node.get());
  const auto renderer5_start_time = base::TimeTicks::Now();

  // Finish next measurement interval.
  task_env().FastForwardBy(kTimeBetweenMeasurements);

  // All nodes existed for entire measurement interval.
  UpdateAndGetCPUMeasurements();

  EXPECT_THAT(
      current_measurements_[renderer1.frame_context],
      CPUDeltaMatches(renderer1.frame_context, kTimeBetweenMeasurements));
  EXPECT_THAT(
      current_measurements_[renderer2.frame_context],
      CPUDeltaMatches(renderer2.frame_context, kTimeBetweenMeasurements));
  EXPECT_THAT(
      current_measurements_[renderer3.frame_context],
      CPUDeltaMatches(renderer3.frame_context, kTimeBetweenMeasurements));
  EXPECT_THAT(
      current_measurements_[renderer4.frame_context],
      AllOf(CPUDeltaMatches(renderer4.frame_context, kTimeBetweenMeasurements),
            StartTimeMatches(renderer4_start_time)));
  EXPECT_THAT(
      current_measurements_[renderer5.frame_context],
      AllOf(CPUDeltaMatches(renderer5.frame_context, kTimeBetweenMeasurements),
            StartTimeMatches(renderer5_start_time)));
}

// Tests that renderers exiting at various points around CPU measurement
// snapshots are handled correctly.
TEST_F(CPUMeasurementMonitorTest, ExitTiming) {
  const SinglePageRendererNodes renderer1 = CreateSimpleCPUTrackingRenderer();
  SetProcessId(renderer1.process_node.get());
  const SinglePageRendererNodes renderer2 = CreateSimpleCPUTrackingRenderer();
  SetProcessId(renderer2.process_node.get());
  const SinglePageRendererNodes renderer3 = CreateSimpleCPUTrackingRenderer();
  SetProcessId(renderer3.process_node.get());
  const SinglePageRendererNodes renderer4 = CreateSimpleCPUTrackingRenderer();
  SetProcessId(renderer4.process_node.get());
  const SinglePageRendererNodes renderer5 = CreateSimpleCPUTrackingRenderer();
  SetProcessId(renderer5.process_node.get());
  const SinglePageRendererNodes renderer6 = CreateSimpleCPUTrackingRenderer();
  SetProcessId(renderer6.process_node.get());
  const SinglePageRendererNodes renderer7 = CreateSimpleCPUTrackingRenderer();
  SetProcessId(renderer7.process_node.get());
  const SinglePageRendererNodes renderer8 = CreateSimpleCPUTrackingRenderer();
  SetProcessId(renderer8.process_node.get());

  StartMonitoring();

  // Test renderers that exit before UpdateAndGetCPUMeasurements is ever called:
  // `renderer1` exits at the beginning of the first measurement interval.
  // `renderer2` exits halfway through.
  // `renderer3` exits at the end of the interval.
  SetProcessExited(renderer1.process_node.get());
  task_env().FastForwardBy(kTimeBetweenMeasurements / 2);
  SetProcessExited(renderer2.process_node.get());

  // Finish the measurement interval.
  task_env().FastForwardBy(kTimeBetweenMeasurements / 2);
  SetProcessExited(renderer3.process_node.get());

  UpdateAndGetCPUMeasurements();
  const auto previous_update_time = base::TimeTicks::Now();

  // Renderers that have exited were never measured.
  EXPECT_FALSE(base::Contains(current_measurements_, renderer1.frame_context));
  EXPECT_FALSE(base::Contains(current_measurements_, renderer2.frame_context));
  EXPECT_FALSE(base::Contains(current_measurements_, renderer3.frame_context));

  // Remaining renderers are using 100% CPU.
  EXPECT_THAT(
      current_measurements_[renderer4.frame_context],
      CPUDeltaMatches(renderer4.frame_context, kTimeBetweenMeasurements));
  EXPECT_THAT(
      current_measurements_[renderer5.frame_context],
      CPUDeltaMatches(renderer5.frame_context, kTimeBetweenMeasurements));
  EXPECT_THAT(
      current_measurements_[renderer6.frame_context],
      CPUDeltaMatches(renderer6.frame_context, kTimeBetweenMeasurements));
  EXPECT_THAT(
      current_measurements_[renderer7.frame_context],
      CPUDeltaMatches(renderer7.frame_context, kTimeBetweenMeasurements));
  EXPECT_THAT(
      current_measurements_[renderer8.frame_context],
      CPUDeltaMatches(renderer8.frame_context, kTimeBetweenMeasurements));

  // `renderer4` exits at the beginning of the next measurement interval.
  // `renderer5` exits halfway through.
  // `renderer6` exits at the end of the interval.
  SetProcessExited(renderer4.process_node.get());
  task_env().FastForwardBy(kTimeBetweenMeasurements / 2);
  SetProcessExited(renderer5.process_node.get());

  // Finish the measurement interval.
  task_env().FastForwardBy(kTimeBetweenMeasurements / 2);
  SetProcessExited(renderer6.process_node.get());

  // TODO(crbug.com/1410503): Processes that exited at any point during the
  // interval still return their last measurement before the interval, so
  // their delta is always empty. Capture the final CPU usage correctly, and
  // test that the renderers that have exited return their CPU usage for the
  // time they were alive and 0% for the rest of the measurement interval.
  UpdateAndGetCPUMeasurements();

  EXPECT_THAT(current_measurements_[renderer4.frame_context],
              CPUDeltaMatches(renderer4.frame_context, base::TimeDelta(),
                              previous_update_time));
  EXPECT_THAT(current_measurements_[renderer5.frame_context],
              CPUDeltaMatches(renderer5.frame_context, base::TimeDelta(),
                              previous_update_time));
  EXPECT_THAT(current_measurements_[renderer6.frame_context],
              CPUDeltaMatches(renderer6.frame_context, base::TimeDelta(),
                              previous_update_time));

  EXPECT_THAT(
      current_measurements_[renderer7.frame_context],
      CPUDeltaMatches(renderer7.frame_context, kTimeBetweenMeasurements));
  EXPECT_THAT(
      current_measurements_[renderer8.frame_context],
      CPUDeltaMatches(renderer8.frame_context, kTimeBetweenMeasurements));

  // `renderer7` exits just before the StopMonitoring call and `renderer7`
  // exits just after. This should not cause any assertion failures.
  SetProcessExited(renderer7.process_node.get());
  cpu_monitor_.StopMonitoring();
  SetProcessExited(renderer8.process_node.get());
}

// Tests that varying CPU usage between measurement snapshots is reported
// correctly.
TEST_F(CPUMeasurementMonitorTest, VaryingMeasurements) {
  const SinglePageRendererNodes renderer1 = CreateSimpleCPUTrackingRenderer();
  SetProcessId(renderer1.process_node.get());
  const SinglePageRendererNodes renderer2 = CreateSimpleCPUTrackingRenderer();
  SetProcessId(renderer2.process_node.get());
  const SinglePageRendererNodes renderer3 = CreateSimpleCPUTrackingRenderer();
  SetProcessId(renderer3.process_node.get());
  const SinglePageRendererNodes renderer4 = CreateSimpleCPUTrackingRenderer();
  SetProcessId(renderer4.process_node.get());

  StartMonitoring();

  // All processes are at 100% for first measurement interval.
  task_env().FastForwardBy(kTimeBetweenMeasurements);
  UpdateAndGetCPUMeasurements();

  EXPECT_THAT(
      current_measurements_[renderer1.frame_context],
      CPUDeltaMatches(renderer1.frame_context, kTimeBetweenMeasurements));
  EXPECT_THAT(
      current_measurements_[renderer2.frame_context],
      CPUDeltaMatches(renderer2.frame_context, kTimeBetweenMeasurements));
  EXPECT_THAT(
      current_measurements_[renderer3.frame_context],
      CPUDeltaMatches(renderer3.frame_context, kTimeBetweenMeasurements));
  EXPECT_THAT(
      current_measurements_[renderer4.frame_context],
      CPUDeltaMatches(renderer4.frame_context, kTimeBetweenMeasurements));

  // `renderer1` drops to 50% CPU usage for the next period.
  // `renderer2` stays at 100% for the first half, 50% for the last half
  // (average 75%).
  // `renderer3` drops to 0% for a time, returns to 100% for half that time,
  // then drops to 0% again (average 25%).
  // `renderer4` drops to 0% at the end of the period. It should still show 100%
  // since no time passes before measuring.
  SetProcessCPUUsage(renderer1.process_node.get(), 0.5);
  SetProcessCPUUsage(renderer3.process_node.get(), 0.0);
  task_env().FastForwardBy(kTimeBetweenMeasurements / 2);
  SetProcessCPUUsage(renderer2.process_node.get(), 0.5);
  SetProcessCPUUsage(renderer3.process_node.get(), 1.0);
  task_env().FastForwardBy(kTimeBetweenMeasurements / 4);
  SetProcessCPUUsage(renderer3.process_node.get(), 0);

  // Finish next measurement interval.
  task_env().FastForwardBy(kTimeBetweenMeasurements / 4);
  SetProcessCPUUsage(renderer4.process_node.get(), 0);

  UpdateAndGetCPUMeasurements();

  EXPECT_THAT(
      current_measurements_[renderer1.frame_context],
      CPUDeltaMatches(renderer1.frame_context, kTimeBetweenMeasurements * 0.5));
  EXPECT_THAT(current_measurements_[renderer2.frame_context],
              CPUDeltaMatches(renderer2.frame_context,
                              kTimeBetweenMeasurements * 0.75));
  EXPECT_THAT(current_measurements_[renderer3.frame_context],
              CPUDeltaMatches(renderer3.frame_context,
                              kTimeBetweenMeasurements * 0.25));
  EXPECT_THAT(
      current_measurements_[renderer4.frame_context],
      CPUDeltaMatches(renderer4.frame_context, kTimeBetweenMeasurements));
}

// Tests that CPU usage of processes is correctly distributed between frames and
// workers in those processes, and correctly aggregated to pages containing
// frames and workers from multiple processes.
TEST_F(CPUMeasurementMonitorTest, CPUDistribution) {
  // Track CPU usage of the mock utility process to make sure that measuring it
  // doesn't crash. Currently only measurements of renderer processes are
  // stored anywhere, so there are no other expectations to verify.
  SetProcessCPUUsage(mock_utility_process_.get(), 0.7);

  SetProcessCPUUsage(mock_graph_->process.get(), 0.6);
  SetProcessCPUUsage(mock_graph_->other_process.get(), 0.5);

  StartMonitoring();
  const auto monitoring_start_time = base::TimeTicks::Now();

  // No measurements if no time has passed.
  UpdateAndGetCPUMeasurements();
  EXPECT_THAT(current_measurements_, IsEmpty());

  task_env().FastForwardBy(kTimeBetweenMeasurements);

  UpdateAndGetCPUMeasurements();

  const FrameContext& frame_context = mock_graph_->frame->resource_context();
  const FrameContext& child_frame_context =
      mock_graph_->child_frame->resource_context();
  const FrameContext& other_frame_context =
      mock_graph_->other_frame->resource_context();
  const WorkerContext& worker_context = mock_graph_->worker->resource_context();
  const WorkerContext& other_worker_context =
      mock_graph_->other_worker->resource_context();

  // `process` splits its 60% CPU usage evenly between `frame`, `other_frame`
  // and `worker`. `other_process` splits its 50% CPU usage evenly between
  // `child_frame` and `other_worker`. See the chart in
  // MockMultiplePagesAndWorkersWithMultipleProcessesGraph.
  base::TimeDelta split_process_cpu_delta = kTimeBetweenMeasurements * 0.2;
  base::TimeDelta other_process_split_cpu_delta =
      kTimeBetweenMeasurements * 0.25;

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
  EXPECT_THAT(GetPageCPUUsageDelta(mock_graph_->page->resource_context()),
              Optional(kTimeBetweenMeasurements * 0.4));
  EXPECT_THAT(GetPageCPUUsageDelta(mock_graph_->other_page->resource_context()),
              Optional(kTimeBetweenMeasurements * 0.7));

  // Modify the CPU usage of each process, ensure all frames and workers are
  // updated.
  SetProcessCPUUsage(mock_graph_->process.get(), 0.3);
  SetProcessCPUUsage(mock_graph_->other_process.get(), 0.8);
  task_env().FastForwardBy(kTimeBetweenMeasurements);

  UpdateAndGetCPUMeasurements();

  // `process` splits its 30% CPU usage evenly between `frame`, `other_frame`
  // and `worker`. `other_process` splits its 80% CPU usage evenly between
  // `child_frame` and `other_worker`.
  split_process_cpu_delta = kTimeBetweenMeasurements * 0.1;
  other_process_split_cpu_delta = kTimeBetweenMeasurements * 0.4;

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
  EXPECT_THAT(GetPageCPUUsageDelta(mock_graph_->page->resource_context()),
              Optional(kTimeBetweenMeasurements * 0.2));
  EXPECT_THAT(GetPageCPUUsageDelta(mock_graph_->other_page->resource_context()),
              Optional(kTimeBetweenMeasurements * 0.9));

  // Drop CPU usage of `other_process` to 0%. Only advance part of the normal
  // measurement interval, to be sure that the percentage usage doesn't depend
  // on the length of the interval.
  constexpr base::TimeDelta kShortInterval = kTimeBetweenMeasurements / 3;
  SetProcessCPUUsage(mock_graph_->other_process.get(), 0.0);
  task_env().FastForwardBy(kShortInterval);

  UpdateAndGetCPUMeasurements();

  // `process` splits its 30% CPU usage evenly between `frame`, `other_frame`
  // and `worker`. `other_process` splits its 0% CPU usage evenly between
  // `child_frame` and `other_worker`.
  split_process_cpu_delta = kShortInterval * 0.1;
  other_process_split_cpu_delta = base::TimeDelta();

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
  EXPECT_THAT(GetPageCPUUsageDelta(mock_graph_->page->resource_context()),
              kShortInterval * 0.2);
  EXPECT_THAT(GetPageCPUUsageDelta(mock_graph_->other_page->resource_context()),
              kShortInterval * 0.1);
}

// Tests that errors returned from ProcessMetrics are correctly ignored.
TEST_F(CPUMeasurementMonitorTest, MeasurementError) {
  const SinglePageRendererNodes renderer1 = CreateSimpleCPUTrackingRenderer();
  SetProcessId(renderer1.process_node.get());
  const SinglePageRendererNodes renderer2 = CreateSimpleCPUTrackingRenderer();
  SetProcessId(renderer2.process_node.get());
  const SinglePageRendererNodes renderer3 = CreateSimpleCPUTrackingRenderer();
  SetProcessId(renderer3.process_node.get());

  StartMonitoring();
  const auto monitoring_start_time = base::TimeTicks::Now();

  task_env().FastForwardBy(kTimeBetweenMeasurements);

  UpdateAndGetCPUMeasurements();
  const auto previous_measurement_time = base::TimeTicks::Now();

  EXPECT_THAT(
      current_measurements_[renderer1.frame_context],
      AllOf(CPUDeltaMatches(renderer1.frame_context, kTimeBetweenMeasurements),
            StartTimeMatches(monitoring_start_time)));
  EXPECT_THAT(
      current_measurements_[renderer2.frame_context],
      AllOf(CPUDeltaMatches(renderer2.frame_context, kTimeBetweenMeasurements),
            StartTimeMatches(monitoring_start_time)));
  EXPECT_THAT(
      current_measurements_[renderer3.frame_context],
      AllOf(CPUDeltaMatches(renderer3.frame_context, kTimeBetweenMeasurements),
            StartTimeMatches(monitoring_start_time)));

  SetProcessCPUUsage(renderer1.process_node.get(), 0.5);
  SetProcessCPUUsage(renderer2.process_node.get(), 0.5);
  SetProcessCPUUsage(renderer3.process_node.get(), 0.5);

  // Most platforms returns a zero TimeDelta on error.
  SetProcessCPUUsageError(renderer1.process_node.get(), base::TimeDelta());
  // Linux returns a negative TimeDelta on error.
  SetProcessCPUUsageError(renderer2.process_node.get(), base::TimeDelta::Min());

  task_env().FastForwardBy(kTimeBetweenMeasurements);

  // After an error the previous measurement should be returned unchanged.
  UpdateAndGetCPUMeasurements();

  EXPECT_THAT(current_measurements_[renderer1.frame_context],
              CPUDeltaMatches(renderer1.frame_context, base::TimeDelta(),
                              previous_measurement_time));
  EXPECT_THAT(current_measurements_[renderer2.frame_context],
              CPUDeltaMatches(renderer2.frame_context, base::TimeDelta(),
                              previous_measurement_time));
  EXPECT_THAT(
      current_measurements_[renderer3.frame_context],
      CPUDeltaMatches(renderer3.frame_context, kTimeBetweenMeasurements * 0.5));
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
    EXPECT_FALSE(base::Contains(cpu_monitor_->UpdateAndGetCPUMeasurements(),
                                frame_context));
  }));

  // Assign a real process to the ProcessNode. (Will call
  // OnProcessLifetimeChange.)
  LetTimePass();
  RunOnPMSequence(base::BindLambdaForTesting([&] {
    ASSERT_TRUE(process_node);
    ProcessNodeImpl::FromNode(process_node.get())
        ->SetProcess(base::Process::Current(), base::TimeTicks::Now());
    EXPECT_NE(process_node->GetProcessId(), base::kNullProcessId);

    // Process can be measured now.
    EXPECT_TRUE(base::Contains(cpu_monitor_->UpdateAndGetCPUMeasurements(),
                               frame_context));
  }));

  // Simulate that the process died.
  LetTimePass();
  process()->SimulateRenderProcessExit(
      base::TERMINATION_STATUS_NORMAL_TERMINATION, 0);
  RunOnPMSequence(base::BindLambdaForTesting([&](Graph* graph) {
    // Process is no longer running, so can't be measured.
    // TODO(crbug.com/1410503): Capture the final CPU usage correctly.
    ASSERT_TRUE(process_node);
    EXPECT_FALSE(process_node->GetProcess().IsValid());
    // Depending on the order that observers fire, the main frame may or may not
    // have been deleted already. If it's deleted CPUMeasurementMonitor will
    // return its last measured usage.
    EXPECT_TRUE(base::Contains(cpu_monitor_->UpdateAndGetCPUMeasurements(),
                               frame_context));
  }));
}

}  // namespace performance_manager::resource_attribution
