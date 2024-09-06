// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/metrics/page_resource_monitor.h"

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/system/sys_info.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/performance_manager/embedder/graph_features.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/performance_manager/public/resource_attribution/cpu_measurement_delegate.h"
#include "components/performance_manager/public/resource_attribution/memory_measurement_delegate.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/test_support/mock_graphs.h"
#include "components/performance_manager/test_support/resource_attribution/measurement_delegates.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/metrics/public/mojom/ukm_interface.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager::metrics {

namespace {

using PageMeasurementAlgorithm = PageResourceMonitor::PageMeasurementAlgorithm;
using PageMeasurementBackgroundState =
    PageResourceMonitor::PageMeasurementBackgroundState;

using CPUMeasurementDelegate = resource_attribution::CPUMeasurementDelegate;
using FakeMemoryMeasurementDelegateFactory =
    resource_attribution::FakeMemoryMeasurementDelegateFactory;
using MemoryMeasurementDelegate =
    resource_attribution::MemoryMeasurementDelegate;
using SimulatedCPUMeasurementDelegate =
    resource_attribution::SimulatedCPUMeasurementDelegate;
using SimulatedCPUMeasurementDelegateFactory =
    resource_attribution::SimulatedCPUMeasurementDelegateFactory;

class PageResourceMonitorUnitTest : public GraphTestHarness {
 public:
  PageResourceMonitorUnitTest() = default;
  ~PageResourceMonitorUnitTest() override = default;

  PageResourceMonitorUnitTest(const PageResourceMonitorUnitTest&) = delete;
  PageResourceMonitorUnitTest& operator=(const PageResourceMonitorUnitTest&) =
      delete;

  void SetUp() override {
    GetGraphFeatures().EnableResourceAttributionScheduler();
    GraphTestHarness::SetUp();

    // Return 50% CPU used by default.
    cpu_delegate_factory_.SetDefaultCPUUsage(0.5);
    CPUMeasurementDelegate::SetDelegateFactoryForTesting(
        graph(), &cpu_delegate_factory_);
    MemoryMeasurementDelegate::SetDelegateFactoryForTesting(
        graph(), &memory_delegate_factory_);

    std::unique_ptr<PageResourceMonitor> monitor =
        std::make_unique<PageResourceMonitor>(enable_system_cpu_probe_);
    monitor_ = monitor.get();
    graph()->PassToGraph(std::move(monitor));
    ResetUkmRecorder();
    last_collection_time_ = base::TimeTicks::Now();
  }

  void TearDown() override {
    test_ukm_recorder_.reset();
    GraphTestHarness::TearDown();
  }

 protected:
  ukm::TestUkmRecorder* test_ukm_recorder() { return test_ukm_recorder_.get(); }
  PageResourceMonitor* monitor() { return monitor_; }

  SimulatedCPUMeasurementDelegate& GetCPUDelegate(const ProcessNode* node) {
    return cpu_delegate_factory_.GetDelegate(node);
  }

  FakeMemoryMeasurementDelegateFactory& GetMemoryDelegate() {
    return memory_delegate_factory_;
  }

  // Advances the clock to trigger the PageResourceUsage UKM.
  void TriggerCollectPageResourceUsage();

  // Advances the mock clock slightly to give enough time to make asynchronous
  // measurements after TriggerCollectPageResourceUsage(). If
  // `include_system_cpu` is true, also waits for some real time to let the
  // system CpuProbe collect data.
  void WaitForMetrics(bool include_system_cpu = false);

  void ResetUkmRecorder() {
    test_ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }

  // Waits for metrics collection and tests whether the BackgroundState logged
  // for each ukm::SourceId matches the given expectation, then clears the
  // collected UKM's for the next slice.
  void WaitForMetricsAndTestBackgroundStates(
      std::map<ukm::SourceId, PageMeasurementBackgroundState> expected_states);

  // Subclasses can override this before calling
  // PageResourceMonitorUnitTest::SetUp() to simulate an environment where
  // CPUProbe::Create() returns nullptr.
  bool enable_system_cpu_probe_ = true;

  raw_ptr<PageResourceMonitor> monitor_ = nullptr;

 private:
  // Advances the mock clock to `target_time`.
  void WaitUntil(base::TimeTicks target_time) {
    ASSERT_GT(target_time, base::TimeTicks::Now());
    task_env().FastForwardBy(target_time - base::TimeTicks::Now());
  }

  std::unique_ptr<ukm::TestUkmRecorder> test_ukm_recorder_;

  // The last time TriggerCollectPageResourceUsage() was called.
  base::TimeTicks last_collection_time_;

  // Factories to return fake measurement delegates. These must be deleted after
  // `monitor_` to ensure that they outlive all delegates they create.
  SimulatedCPUMeasurementDelegateFactory cpu_delegate_factory_;
  FakeMemoryMeasurementDelegateFactory memory_delegate_factory_;
};

void PageResourceMonitorUnitTest::TriggerCollectPageResourceUsage() {
  WaitUntil(last_collection_time_ + monitor_->GetCollectionDelayForTesting());
  last_collection_time_ = base::TimeTicks::Now();
}

void PageResourceMonitorUnitTest::WaitForMetrics(bool include_system_cpu) {
  if (include_system_cpu) {
    // Page CPU can use the mock clock, but CPUProbe needs real time to pass.
    base::PlatformThread::Sleep(TestTimeouts::tiny_timeout());
  }
  // GraphTestHarness uses ThreadPoolExecutionMode::QUEUED, so RunLoop only
  // pumps the main thread. FastForwardBy also pumps ThreadPool threads. With
  // the mock clock, any timeout will pump the threads enough to post the tasks
  // that gather and record metrics.
  task_env().FastForwardBy(TestTimeouts::tiny_timeout());
}

void PageResourceMonitorUnitTest::WaitForMetricsAndTestBackgroundStates(
    std::map<ukm::SourceId, PageMeasurementBackgroundState> expected_states) {
  WaitForMetrics();
  auto entries = test_ukm_recorder()->GetEntriesByName(
      ukm::builders::PerformanceManager_PageResourceUsage2::kEntryName);
  // Expect 1 entry per page.
  EXPECT_EQ(entries.size(), expected_states.size());
  for (const ukm::mojom::UkmEntry* entry : entries) {
    test_ukm_recorder()->ExpectEntryMetric(
        entry, "BackgroundState",
        static_cast<int64_t>(expected_states.at(entry->source_id)));
  }
  ResetUkmRecorder();
}

TEST_F(PageResourceMonitorUnitTest, TestPageResourceUsage) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  ukm::SourceId mock_source_id = ukm::NoURLSourceId();
  mock_graph.page->SetType(performance_manager::PageType::kTab);
  mock_graph.page->SetUkmSourceId(mock_source_id);

  TriggerCollectPageResourceUsage();
  WaitForMetrics();

  auto entries = test_ukm_recorder()->GetEntriesByName(
      ukm::builders::PerformanceManager_PageResourceUsage2::kEntryName);
  EXPECT_EQ(entries.size(), 1u);
}

TEST_F(PageResourceMonitorUnitTest, TestPageResourceUsageNavigation) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  ukm::SourceId mock_source_id =
      ukm::AssignNewSourceId();  // ukm::NoURLSourceId();
  ukm::SourceId mock_source_id_2 = ukm::AssignNewSourceId();

  mock_graph.page->SetUkmSourceId(mock_source_id);
  mock_graph.page->SetType(performance_manager::PageType::kTab);

  // Each SourceId should record an entry.
  std::vector<ukm::SourceId> expected_ids{mock_source_id, mock_source_id_2};

  TriggerCollectPageResourceUsage();
  WaitForMetrics();

  auto entries = test_ukm_recorder()->GetEntriesByName(
      ukm::builders::PerformanceManager_PageResourceUsage2::kEntryName);
  EXPECT_EQ(entries.size(), 1u);

  mock_graph.page->SetUkmSourceId(mock_source_id_2);

  TriggerCollectPageResourceUsage();
  WaitForMetrics();

  entries = test_ukm_recorder()->GetEntriesByName(
      ukm::builders::PerformanceManager_PageResourceUsage2::kEntryName);
  EXPECT_EQ(entries.size(), 2u);

  std::vector<ukm::SourceId> ids;
  for (const ukm::mojom::UkmEntry* entry : entries) {
    const int64_t* algorithm =
        test_ukm_recorder()->GetEntryMetric(entry, "MeasurementAlgorithm");
    ASSERT_NE(algorithm, nullptr);
    EXPECT_EQ(static_cast<PageMeasurementAlgorithm>(*algorithm),
              PageMeasurementAlgorithm::kEvenSplitAndAggregate);
    ids.emplace_back(entry->source_id);
  }

  EXPECT_THAT(ids, ::testing::UnorderedElementsAreArray(expected_ids));
}

TEST_F(PageResourceMonitorUnitTest, TestOnlyRecordTabs) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  ukm::SourceId mock_source_id = ukm::NoURLSourceId();
  mock_graph.page->SetUkmSourceId(mock_source_id);

  TriggerCollectPageResourceUsage();
  WaitForMetrics();

  auto entries2 = test_ukm_recorder()->GetEntriesByName(
      ukm::builders::PerformanceManager_PageResourceUsage2::kEntryName);
  EXPECT_EQ(entries2.size(), 0UL);
}

TEST_F(PageResourceMonitorUnitTest, TestResourceUsage) {
  MockMultiplePagesWithMultipleProcessesGraph mock_graph(graph());

  // Register fake memory results. Make sure they're divisible by 2 for easier
  // matching when divided between frames.
  MemoryMeasurementDelegate::MemorySummaryMap& memory_summaries =
      GetMemoryDelegate().memory_summaries();
  memory_summaries[mock_graph.process->GetResourceContext()] = {
      .resident_set_size_kb = 1230};
  memory_summaries[mock_graph.other_process->GetResourceContext()] = {
      .resident_set_size_kb = 4560, .private_footprint_kb = 7890};

  const ukm::SourceId mock_source_id = ukm::AssignNewSourceId();
  mock_graph.page->SetType(performance_manager::PageType::kTab);
  mock_graph.page->SetUkmSourceId(mock_source_id);

  const ukm::SourceId mock_source_id2 = ukm::AssignNewSourceId();
  mock_graph.other_page->SetType(performance_manager::PageType::kTab);
  mock_graph.other_page->SetUkmSourceId(mock_source_id2);

  TriggerCollectPageResourceUsage();
  WaitForMetrics();

  auto entries = test_ukm_recorder()->GetEntriesByName(
      ukm::builders::PerformanceManager_PageResourceUsage2::kEntryName);
  // Expect 1 entry per page.
  EXPECT_EQ(entries.size(), 2u);

  // `page` gets its memory from `frame`, which is 1/2 the memory from
  // `process`.
  // `other_page` gets its memory from `other_frame` (1/2 of `process`) +
  // `child_frame` (all of `other_process`).
  // See the diagram in
  // components/performance_manager/test_support/mock_graphs.h.
  const base::flat_map<ukm::SourceId, int64_t> expected_resident_set_size{
      {mock_source_id, 1230 / 2},
      {mock_source_id2, 1230 / 2 + 4560},
  };
  const base::flat_map<ukm::SourceId, int64_t> expected_private_footprint{
      {mock_source_id, 0},
      {mock_source_id2, 7890},
  };
  // The SimulatedCPUMeasurementDelegate returns 50% of the CPU is used.
  // `process` contains `frame` and `other_frame` -> each gets 25%
  // `other_process` contains `child_frame` -> 50%
  const base::flat_map<ukm::SourceId, int64_t> expected_cpu_usage{
      // `page` contains `frame`
      {mock_source_id, 2500},
      // `other_page` gets the sum of `other_frame` and `child_frame`
      {mock_source_id2, 7500},
  };
  const auto kExpectedAllCPUUsage = 2500 + 7500;

  // Each SourceId should record an entry.
  std::vector<ukm::SourceId> expected_ids{mock_source_id, mock_source_id2};

  std::vector<ukm::SourceId> ids;
  for (const ukm::mojom::UkmEntry* entry : entries) {
    test_ukm_recorder()->ExpectEntryMetric(
        entry, "ResidentSetSizeEstimate",
        expected_resident_set_size.at(entry->source_id));
    test_ukm_recorder()->ExpectEntryMetric(
        entry, "PrivateFootprintEstimate",
        expected_private_footprint.at(entry->source_id));
    test_ukm_recorder()->ExpectEntryMetric(
        entry, "RecentCPUUsage", expected_cpu_usage.at(entry->source_id));
    test_ukm_recorder()->ExpectEntryMetric(entry, "TotalRecentCPUUsageAllPages",
                                           kExpectedAllCPUUsage);
    const int64_t* algorithm =
        test_ukm_recorder()->GetEntryMetric(entry, "MeasurementAlgorithm");
    ASSERT_NE(algorithm, nullptr);
    EXPECT_EQ(static_cast<PageMeasurementAlgorithm>(*algorithm),
              PageMeasurementAlgorithm::kEvenSplitAndAggregate);
    ids.emplace_back(entry->source_id);
  }
  EXPECT_THAT(ids, ::testing::UnorderedElementsAreArray(expected_ids));
}

TEST_F(PageResourceMonitorUnitTest, TestResourceUsageBackgroundState) {
  MockMultiplePagesWithMultipleProcessesGraph mock_graph(graph());
  const ukm::SourceId mock_source_id = ukm::AssignNewSourceId();
  mock_graph.page->SetType(performance_manager::PageType::kTab);
  mock_graph.page->SetUkmSourceId(mock_source_id);

  const ukm::SourceId mock_source_id2 = ukm::AssignNewSourceId();
  mock_graph.other_page->SetType(performance_manager::PageType::kTab);
  mock_graph.other_page->SetUkmSourceId(mock_source_id2);

  // Start with page 1 in foreground. Pages remain in the same state for all
  // of the first measurement period.
  mock_graph.page->SetIsVisible(true);
  mock_graph.other_page->SetIsVisible(false);
  TriggerCollectPageResourceUsage();

  // Pages become audible for all of next measurement period. Change the state
  // before waiting for the metrics to finish logging so the change happens at
  // the beginning of the period.
  mock_graph.page->SetIsAudible(true);
  mock_graph.other_page->SetIsAudible(true);

  // Test the metrics from the first measurement period.
  WaitForMetricsAndTestBackgroundStates(
      {{mock_source_id, PageMeasurementBackgroundState::kForeground},
       {mock_source_id2, PageMeasurementBackgroundState::kBackground}});

  // Finish the second measurement period.
  TriggerCollectPageResourceUsage();
  WaitForMetricsAndTestBackgroundStates(
      {{mock_source_id, PageMeasurementBackgroundState::kForeground},
       {mock_source_id2,
        PageMeasurementBackgroundState::kAudibleInBackground}});

  // Partway through next measurement period:
  // - Page 1 moves to background (still audible).
  // - Page 2 stops playing audio.
  const base::TimeDelta partial_delay =
      monitor_->GetCollectionDelayForTesting() / 4;
  task_env().FastForwardBy(partial_delay);
  mock_graph.page->SetIsVisible(false);
  mock_graph.other_page->SetIsAudible(false);
  TriggerCollectPageResourceUsage();
  WaitForMetricsAndTestBackgroundStates(
      {{mock_source_id,
        PageMeasurementBackgroundState::kMixedForegroundBackground},
       {mock_source_id2,
        PageMeasurementBackgroundState::kBackgroundMixedAudible}});

  // Partway through next measurement period, page 2 moves to foreground (still
  // inaudible).
  task_env().FastForwardBy(partial_delay);
  mock_graph.other_page->SetIsVisible(true);
  TriggerCollectPageResourceUsage();
  WaitForMetricsAndTestBackgroundStates(
      {{mock_source_id, PageMeasurementBackgroundState::kAudibleInBackground},
       {mock_source_id2,
        PageMeasurementBackgroundState::kMixedForegroundBackground}});
}

}  // namespace

}  // namespace performance_manager::metrics
