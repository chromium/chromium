// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/tasks/metrics_finalization_task.h"

#include <memory>
#include <set>
#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "components/offline_pages/core/prefetch/mock_prefetch_item_generator.h"
#include "components/offline_pages/core/prefetch/prefetch_item.h"
#include "components/offline_pages/core/prefetch/prefetch_types.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store_test_util.h"
#include "components/offline_pages/core/prefetch/tasks/prefetch_task_test_base.h"
#include "components/offline_pages/core/test_scoped_offline_clock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

class MetricsFinalizationTaskTest : public PrefetchTaskTestBase {
 public:
  MetricsFinalizationTaskTest() = default;
  ~MetricsFinalizationTaskTest() override = default;

  void SetUp() override;
  void TearDown() override;

 protected:
  std::unique_ptr<MetricsFinalizationTask> metrics_finalization_task_;
};

void MetricsFinalizationTaskTest::SetUp() {
  PrefetchTaskTestBase::SetUp();
  metrics_finalization_task_ =
      std::make_unique<MetricsFinalizationTask>(store());
}

void MetricsFinalizationTaskTest::TearDown() {
  metrics_finalization_task_.reset();
  PrefetchTaskTestBase::TearDown();
}

TEST_F(MetricsFinalizationTaskTest, StoreFailure) {
  store_util()->SimulateInitializationError();

  // Execute the metrics task.
  RunTask(metrics_finalization_task_.get());
}

// Tests that the task works correctly with an empty database.
TEST_F(MetricsFinalizationTaskTest, EmptyRun) {
  EXPECT_EQ(0, store_util()->CountPrefetchItems());

  // Execute the metrics task.
  RunTask(metrics_finalization_task_.get());
  EXPECT_EQ(0, store_util()->CountPrefetchItems());
}

TEST_F(MetricsFinalizationTaskTest, LeavesOtherStatesAlone) {
  std::vector<PrefetchItemState> all_states_but_finished =
      GetAllStatesExcept({PrefetchItemState::FINISHED});

  for (auto& state : all_states_but_finished) {
    PrefetchItem item = item_generator()->CreateItem(state);
    EXPECT_TRUE(store_util()->InsertPrefetchItem(item))
        << "Failed inserting item with state " << static_cast<int>(state);
  }

  std::set<PrefetchItem> all_inserted_items;
  EXPECT_EQ(10U, store_util()->GetAllItems(&all_inserted_items));

  // Execute the task.
  RunTask(metrics_finalization_task_.get());

  std::set<PrefetchItem> all_items_after_task;
  EXPECT_EQ(10U, store_util()->GetAllItems(&all_items_after_task));
  EXPECT_EQ(all_inserted_items, all_items_after_task);
}

TEST_F(MetricsFinalizationTaskTest, FinalizesMultipleItems) {
  base::Time before_insert_time = base::Time::Now();
  std::set<PrefetchItem> finished_items = {
      item_generator()->CreateItem(PrefetchItemState::FINISHED),
      item_generator()->CreateItem(PrefetchItemState::FINISHED),
      item_generator()->CreateItem(PrefetchItemState::FINISHED)};
  for (auto& item : finished_items) {
    ASSERT_TRUE(store_util()->InsertPrefetchItem(item));
    // Confirms that ItemGenerator did set |freshness_time| with Time::Now().
    ASSERT_LE(before_insert_time, item.freshness_time);
  }

  PrefetchItem unfinished_item =
      item_generator()->CreateItem(PrefetchItemState::NEW_REQUEST);
  ASSERT_TRUE(store_util()->InsertPrefetchItem(unfinished_item));

  // Overrides the offline clock and set a current time in the future.
  TestScopedOfflineClock clock;
  clock.SetNow(before_insert_time + base::Hours(1));

  // Execute the metrics task.
  RunTask(metrics_finalization_task_.get());

  // The finished ones should all have become zombies and the new request should
  // be untouched.
  std::set<PrefetchItem> all_items;
  EXPECT_EQ(4U, store_util()->GetAllItems(&all_items));
  EXPECT_EQ(0U, FilterByState(all_items, PrefetchItemState::FINISHED).size());

  std::set<PrefetchItem> zombie_items =
      FilterByState(all_items, PrefetchItemState::ZOMBIE);
  EXPECT_EQ(3U, zombie_items.size());
  for (const PrefetchItem& zombie_item : zombie_items) {
    EXPECT_EQ(clock.Now(), zombie_item.freshness_time)
        << "Incorrect freshness_time (not updated?) for item "
        << zombie_item.client_id;
  }

  std::set<PrefetchItem> items_in_new_request_state =
      FilterByState(all_items, PrefetchItemState::NEW_REQUEST);
  EXPECT_EQ(1U, items_in_new_request_state.count(unfinished_item));
}

TEST_F(MetricsFinalizationTaskTest, MetricsAreReported) {
  PrefetchItem successful_item =
      item_generator()->CreateItem(PrefetchItemState::FINISHED);
  successful_item.generate_bundle_attempts = 1;
  successful_item.get_operation_attempts = 1;
  successful_item.download_initiation_attempts = 1;
  ASSERT_TRUE(store_util()->InsertPrefetchItem(successful_item));

  PrefetchItem failed_item =
      item_generator()->CreateItem(PrefetchItemState::RECEIVED_GCM);
  failed_item.state = PrefetchItemState::FINISHED;
  failed_item.error_code = PrefetchItemErrorCode::ARCHIVING_FAILED;
  ASSERT_TRUE(store_util()->InsertPrefetchItem(failed_item));

  PrefetchItem unfinished_item =
      item_generator()->CreateItem(PrefetchItemState::NEW_REQUEST);
  ASSERT_TRUE(store_util()->InsertPrefetchItem(unfinished_item));

  // Execute the metrics task.
  base::HistogramTester histogram_tester;
  RunTask(metrics_finalization_task_.get());

  std::set<PrefetchItem> all_items;
  EXPECT_EQ(3U, store_util()->GetAllItems(&all_items));
  EXPECT_EQ(2U, FilterByState(all_items, PrefetchItemState::ZOMBIE).size());
  EXPECT_EQ(1U,
            FilterByState(all_items, PrefetchItemState::NEW_REQUEST).size());

  // One successful and one failed samples.
  histogram_tester.ExpectUniqueSample(
      "OfflinePages.Prefetching.ItemLifetime.Successful", 0, 1);
  histogram_tester.ExpectUniqueSample(
      "OfflinePages.Prefetching.ItemLifetime.Failed", 0, 1);

  // One sample for each_error code value.
  histogram_tester.ExpectTotalCount(
      "OfflinePages.Prefetching.FinishedItemErrorCode", 2);
  histogram_tester.ExpectBucketCount(
      "OfflinePages.Prefetching.FinishedItemErrorCode",
      static_cast<int>(PrefetchItemErrorCode::SUCCESS), 1);
  histogram_tester.ExpectBucketCount(
      "OfflinePages.Prefetching.FinishedItemErrorCode",
      static_cast<int>(PrefetchItemErrorCode::ARCHIVING_FAILED), 1);

  // Attempt values match what was set above (non set values default to 0).
  histogram_tester.ExpectTotalCount(
      "OfflinePages.Prefetching.ActionAttempts.GeneratePageBundle", 2);
  histogram_tester.ExpectBucketCount(
      "OfflinePages.Prefetching.ActionAttempts.GeneratePageBundle", 0, 1);
  histogram_tester.ExpectBucketCount(
      "OfflinePages.Prefetching.ActionAttempts.GeneratePageBundle", 1, 1);
  histogram_tester.ExpectTotalCount(
      "OfflinePages.Prefetching.ActionAttempts.GetOperation", 2);
  histogram_tester.ExpectBucketCount(
      "OfflinePages.Prefetching.ActionAttempts.GetOperation", 0, 1);
  histogram_tester.ExpectBucketCount(
      "OfflinePages.Prefetching.ActionAttempts.GetOperation", 1, 1);
  histogram_tester.ExpectTotalCount(
      "OfflinePages.Prefetching.ActionAttempts.DownloadInitiation", 2);
  histogram_tester.ExpectBucketCount(
      "OfflinePages.Prefetching.ActionAttempts.DownloadInitiation", 0, 1);
  histogram_tester.ExpectBucketCount(
      "OfflinePages.Prefetching.ActionAttempts.DownloadInitiation", 1, 1);
}

// Verifies that items from all states are counted properly.
TEST_F(MetricsFinalizationTaskTest,
       CountsItemsInEachStateMetricReportedCorectly) {
  // Insert a different number of items for each state.
  for (size_t i = 0; i < kOrderedPrefetchItemStates.size(); ++i) {
    PrefetchItemState state = kOrderedPrefetchItemStates[i];
    for (size_t j = 0; j < i + 1; ++j) {
      PrefetchItem item = item_generator()->CreateItem(state);
      EXPECT_TRUE(store_util()->InsertPrefetchItem(item))
          << "Failed inserting item with state " << static_cast<int>(state);
    }
  }

  // Execute the task.
  base::HistogramTester histogram_tester;
  RunTask(metrics_finalization_task_.get());

  histogram_tester.ExpectTotalCount("OfflinePages.Prefetching.StateCounts", 66);

  // Check that histogram was recorded correctly for items in each state.
  for (size_t i = 0; i < kOrderedPrefetchItemStates.size(); ++i) {
    histogram_tester.ExpectBucketCount(
        "OfflinePages.Prefetching.StateCounts",
        static_cast<int>(kOrderedPrefetchItemStates[i]), i + 1);
  }
}

}  // namespace offline_pages
