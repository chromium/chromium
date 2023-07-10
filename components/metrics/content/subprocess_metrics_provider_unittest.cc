// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/content/subprocess_metrics_provider.h"

#include <memory>
#include <string>
#include <vector>

#include "base/metrics/histogram.h"
#include "base/metrics/histogram_flattener.h"
#include "base/metrics/histogram_snapshot_manager.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "base/metrics/persistent_memory_allocator.h"
#include "base/metrics/statistics_recorder.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "components/metrics/metrics_features.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;

namespace metrics {
namespace {

const uint32_t TEST_MEMORY_SIZE = 64 << 10;  // 64 KiB

class HistogramFlattenerDeltaRecorder : public base::HistogramFlattener {
 public:
  HistogramFlattenerDeltaRecorder() = default;

  HistogramFlattenerDeltaRecorder(const HistogramFlattenerDeltaRecorder&) =
      delete;
  HistogramFlattenerDeltaRecorder& operator=(
      const HistogramFlattenerDeltaRecorder&) = delete;

  ~HistogramFlattenerDeltaRecorder() override = default;

  void RecordDelta(const base::HistogramBase& histogram,
                   const base::HistogramSamples& snapshot) override {
    // Only record histograms that start with '_' (e.g., _foo, _bar, _baz) to
    // not record histograms from outside what is being tested.
    if (histogram.histogram_name()[0] == '_') {
      recorded_delta_histogram_names_.push_back(histogram.histogram_name());
    }
  }

  std::vector<std::string> GetRecordedDeltaHistogramNames() {
    return recorded_delta_histogram_names_;
  }

 private:
  std::vector<std::string> recorded_delta_histogram_names_;
};

// Same as PersistentHistogramAllocator, but will call a callback on being
// destroyed.
class TestPersistentHistogramAllocator
    : public base::PersistentHistogramAllocator {
 public:
  using base::PersistentHistogramAllocator::PersistentHistogramAllocator;

  TestPersistentHistogramAllocator(const TestPersistentHistogramAllocator&) =
      delete;
  TestPersistentHistogramAllocator& operator=(
      const TestPersistentHistogramAllocator&) = delete;

  ~TestPersistentHistogramAllocator() override {
    if (!destroyed_callback_.is_null()) {
      std::move(destroyed_callback_).Run();
    }
  }

  void SetDestroyedCallback(base::OnceClosure destroyed_callback) {
    destroyed_callback_ = std::move(destroyed_callback);
  }

 private:
  base::OnceClosure destroyed_callback_;
};

}  // namespace

class SubprocessMetricsProviderTest : public testing::Test {
 public:
  SubprocessMetricsProviderTest(const SubprocessMetricsProviderTest&) = delete;
  SubprocessMetricsProviderTest& operator=(
      const SubprocessMetricsProviderTest&) = delete;

 protected:
  SubprocessMetricsProviderTest()
      : task_environment_(
            // Use ThreadPoolExecutionMode::QUEUED so that tests can decide
            // exactly when tasks posted to ThreadPool start running.
            base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED) {
    SubprocessMetricsProvider::CreateInstance();

    // Need to recreate a task runner, otherwise it may be a stale one from a
    // previous test (it's possible the global instance is re-used across
    // tests).
    RecreateTaskRunnerForTesting();

    // MergeHistogramDeltas needs to be called because it uses a histogram
    // macro which caches a pointer to a histogram. If not done before setting
    // a persistent global allocator, then it would point into memory that
    // will go away.
    SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

    // Create a dedicated StatisticsRecorder for this test.
    test_recorder_ = base::StatisticsRecorder::CreateTemporaryForTesting();

    // Create a global allocator using a block of memory from the heap.
    base::GlobalHistogramAllocator::CreateWithLocalMemory(TEST_MEMORY_SIZE, 0,
                                                          "");
  }

  ~SubprocessMetricsProviderTest() override {
    base::GlobalHistogramAllocator::ReleaseForTesting();
  }

  std::unique_ptr<TestPersistentHistogramAllocator> CreateDuplicateAllocator(
      base::PersistentHistogramAllocator* allocator) {
    // Just wrap around the data segment in-use by the passed allocator.
    return std::make_unique<TestPersistentHistogramAllocator>(
        std::make_unique<base::PersistentMemoryAllocator>(
            const_cast<void*>(allocator->data()), allocator->length(), 0, 0,
            std::string(), false));
  }

  std::vector<std::string> GetSnapshotHistogramNames() {
    // Flatten what is known to see what has changed since the last time.
    HistogramFlattenerDeltaRecorder flattener;
    base::HistogramSnapshotManager snapshot_manager(&flattener);
    // "true" to the begin() includes histograms held in persistent storage.
    base::StatisticsRecorder::PrepareDeltas(true, base::Histogram::kNoFlags,
                                            base::Histogram::kNoFlags,
                                            &snapshot_manager);
    return flattener.GetRecordedDeltaHistogramNames();
  }

  void RegisterSubprocessAllocator(
      int id,
      std::unique_ptr<base::PersistentHistogramAllocator> allocator) {
    SubprocessMetricsProvider::GetInstance()->RegisterSubprocessAllocator(
        id, std::move(allocator));
  }

  void DeregisterSubprocessAllocator(int id) {
    SubprocessMetricsProvider::GetInstance()->DeregisterSubprocessAllocator(id);
  }

  void RecreateTaskRunnerForTesting() {
    SubprocessMetricsProvider::GetInstance()->RecreateTaskRunnerForTesting();
  }

  content::BrowserTaskEnvironment* task_environment() {
    return &task_environment_;
  }

 private:
  // A thread-bundle makes the tests appear on the UI thread, something that is
  // checked in methods called from the SubprocessMetricsProvider class under
  // test. This must be constructed before the |provider_| field.
  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<base::StatisticsRecorder> test_recorder_;
};

TEST_F(SubprocessMetricsProviderTest, SnapshotMetrics) {
  base::HistogramBase* foo = base::Histogram::FactoryGet("_foo", 1, 100, 10, 0);
  base::HistogramBase* bar = base::Histogram::FactoryGet("_bar", 1, 100, 10, 0);
  base::HistogramBase* baz = base::Histogram::FactoryGet("_baz", 1, 100, 10, 0);
  foo->Add(42);
  bar->Add(84);

  // Detach the global allocator but keep it around until this method exits
  // so that the memory holding histogram data doesn't get released. Register
  // a new allocator that duplicates the global one.
  std::unique_ptr<base::GlobalHistogramAllocator> global_allocator(
      base::GlobalHistogramAllocator::ReleaseForTesting());
  auto duplicate_allocator = CreateDuplicateAllocator(global_allocator.get());
  bool duplicate_allocator_destroyed = false;
  duplicate_allocator->SetDestroyedCallback(base::BindLambdaForTesting(
      [&] { duplicate_allocator_destroyed = true; }));
  RegisterSubprocessAllocator(123, std::move(duplicate_allocator));

  // Recording should find the two histograms created in persistent memory.
  SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  EXPECT_THAT(GetSnapshotHistogramNames(),
              UnorderedElementsAre("_foo", "_bar"));

  // A second run should have nothing to produce.
  SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  EXPECT_THAT(GetSnapshotHistogramNames(), IsEmpty());

  // Create a new histogram and update existing ones. Should now report 3 items.
  baz->Add(1969);
  foo->Add(10);
  bar->Add(20);
  SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  EXPECT_THAT(GetSnapshotHistogramNames(),
              UnorderedElementsAre("_foo", "_bar", "_baz"));

  // Ensure that deregistering does a final merge of the data.
  foo->Add(10);
  bar->Add(20);
  DeregisterSubprocessAllocator(123);
  // Do not call MergeHistogramDeltas() here, because the call to
  // DeregisterSubprocessAllocator() should have already merged the histograms.
  EXPECT_THAT(GetSnapshotHistogramNames(),
              UnorderedElementsAre("_foo", "_bar"));
  // The allocator should have been released after deregistering.
  EXPECT_TRUE(duplicate_allocator_destroyed);

  // Further snapshots should be empty even if things have changed.
  foo->Add(10);
  bar->Add(20);
  SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  EXPECT_THAT(GetSnapshotHistogramNames(), IsEmpty());
}

TEST_F(SubprocessMetricsProviderTest, SnapshotMetricsAsync) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      features::kSubprocessMetricsAsync,
      {{features::kPeriodicMergeAsync.name, "true"},
       {features::kDeregisterAsync.name, "true"}});
  // Force creation of task runner after enabling |kSubprocessMetricsAsync| with
  // |kDeregisterAsync| set to true.
  RecreateTaskRunnerForTesting();

  base::HistogramBase* foo = base::Histogram::FactoryGet("_foo", 1, 100, 10, 0);
  base::HistogramBase* bar = base::Histogram::FactoryGet("_bar", 1, 100, 10, 0);
  base::HistogramBase* baz = base::Histogram::FactoryGet("_baz", 1, 100, 10, 0);
  foo->Add(42);
  bar->Add(84);

  // Detach the global allocator but keep it around until this method exits
  // so that the memory holding histogram data doesn't get released. Register
  // a new allocator that duplicates the global one.
  std::unique_ptr<base::GlobalHistogramAllocator> global_allocator(
      base::GlobalHistogramAllocator::ReleaseForTesting());
  RegisterSubprocessAllocator(123,
                              CreateDuplicateAllocator(global_allocator.get()));

  // Recording should find the two histograms created in persistent memory.
  SubprocessMetricsProvider::MergeHistogramDeltasForTesting(
      /*async=*/true, /*done_callback=*/task_environment()->QuitClosure());
  EXPECT_THAT(GetSnapshotHistogramNames(), IsEmpty());
  task_environment()->RunUntilQuit();
  EXPECT_THAT(GetSnapshotHistogramNames(),
              UnorderedElementsAre("_foo", "_bar"));

  // A second run should have nothing to produce.
  SubprocessMetricsProvider::MergeHistogramDeltasForTesting(
      /*async=*/true, /*done_callback=*/task_environment()->QuitClosure());
  EXPECT_THAT(GetSnapshotHistogramNames(), IsEmpty());
  task_environment()->RunUntilQuit();
  EXPECT_THAT(GetSnapshotHistogramNames(), IsEmpty());

  // Create a new histogram and update existing ones. Should now report 3 items.
  baz->Add(1969);
  foo->Add(10);
  bar->Add(20);
  SubprocessMetricsProvider::MergeHistogramDeltasForTesting(
      /*async=*/true, /*done_callback=*/task_environment()->QuitClosure());
  EXPECT_THAT(GetSnapshotHistogramNames(), IsEmpty());
  task_environment()->RunUntilQuit();
  EXPECT_THAT(GetSnapshotHistogramNames(),
              UnorderedElementsAre("_foo", "_bar", "_baz"));

  // Ensure that deregistering does a final merge of the data.
  foo->Add(10);
  bar->Add(20);
  DeregisterSubprocessAllocator(123);
  // Do not call MergeHistogramDeltas() here, because the call to
  // DeregisterSubprocessAllocator() should have already scheduled a task to
  // merge the histograms.
  EXPECT_THAT(GetSnapshotHistogramNames(), IsEmpty());
  task_environment()->RunUntilIdle();
  EXPECT_THAT(GetSnapshotHistogramNames(),
              UnorderedElementsAre("_foo", "_bar"));

  // Further snapshots should be empty even if things have changed.
  foo->Add(10);
  bar->Add(20);
  SubprocessMetricsProvider::MergeHistogramDeltasForTesting(
      /*async=*/true, /*done_callback=*/task_environment()->QuitClosure());
  EXPECT_THAT(GetSnapshotHistogramNames(), IsEmpty());
  task_environment()->RunUntilQuit();
  EXPECT_THAT(GetSnapshotHistogramNames(), IsEmpty());
}

class SubprocessMetricsProviderWithParamTest
    : public SubprocessMetricsProviderTest,
      public testing::WithParamInterface<std::pair<bool, bool>> {};

INSTANTIATE_TEST_SUITE_P(All,
                         SubprocessMetricsProviderWithParamTest,
                         testing::Values(std::make_pair(true, false),
                                         std::make_pair(false, true),
                                         std::make_pair(true, true)));

// Verifies that it is fine to deregister an allocator even if background tasks
// that access it are still pending/running.
TEST_P(SubprocessMetricsProviderWithParamTest, AllocatorRefCounted) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      features::kSubprocessMetricsAsync,
      {{features::kPeriodicMergeAsync.name,
        GetParam().first ? "true" : "false"},
       {features::kDeregisterAsync.name,
        GetParam().second ? "true" : "false"}});
  // Force creation of task runner if |kDeregisterAsync| was set to true.
  RecreateTaskRunnerForTesting();

  base::HistogramBase* foo = base::Histogram::FactoryGet("_foo", 1, 100, 10, 0);
  base::HistogramBase* bar = base::Histogram::FactoryGet("_bar", 1, 100, 10, 0);
  foo->Add(42);
  bar->Add(84);

  // Detach the global allocator but keep it around until this method exits
  // so that the memory holding histogram data doesn't get released. Register
  // a new allocator that duplicates the global one.
  std::unique_ptr<base::GlobalHistogramAllocator> global_allocator(
      base::GlobalHistogramAllocator::ReleaseForTesting());
  auto duplicate_allocator = CreateDuplicateAllocator(global_allocator.get());
  bool duplicate_allocator_destroyed = false;
  duplicate_allocator->SetDestroyedCallback(base::BindLambdaForTesting(
      [&] { duplicate_allocator_destroyed = true; }));
  RegisterSubprocessAllocator(123, std::move(duplicate_allocator));

  // Merge histogram deltas. Depending on the test's param (GetParam().first),
  // this may be done asynchronously.
  SubprocessMetricsProvider::MergeHistogramDeltasForTesting(
      /*async=*/true, /*done_callback=*/base::DoNothing());
  // Deregister the allocator. Depending on the test's param
  // (GetParam().second), the final merge of the histograms may be done
  // asynchronously.
  DeregisterSubprocessAllocator(123);

  // Regardless of the test's params (GetParam()), the call to
  // DeregisterSubprocessAllocator() above will have removed the allocator from
  // the internal map. However, the allocator should not have been freed yet as
  // there are still background tasks pending/running that have a reference to
  // it (i.e., one possibly posted by MergeHistogramDeltas(), and one possibly
  // by DeregisterSubprocessAllocator()).
  ASSERT_FALSE(duplicate_allocator_destroyed);

  // Run tasks.
  task_environment()->RunUntilIdle();

  // After all the tasks have finished, the allocator should have been released.
  EXPECT_TRUE(duplicate_allocator_destroyed);

  // Verify that the histograms were merged.
  EXPECT_THAT(GetSnapshotHistogramNames(),
              UnorderedElementsAre("_foo", "_bar"));
}

}  // namespace metrics
