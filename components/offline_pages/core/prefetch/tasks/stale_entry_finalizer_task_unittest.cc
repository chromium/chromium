// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/tasks/stale_entry_finalizer_task.h"

#include <memory>
#include <set>

#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_simple_task_runner.h"
#include "components/offline_pages/core/prefetch/mock_prefetch_item_generator.h"
#include "components/offline_pages/core/prefetch/prefetch_item.h"
#include "components/offline_pages/core/prefetch/prefetch_types.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store_test_util.h"
#include "components/offline_pages/core/prefetch/tasks/prefetch_task_test_base.h"
#include "components/offline_pages/core/prefetch/test_prefetch_dispatcher.h"
#include "components/offline_pages/core/test_scoped_offline_clock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

using Result = StaleEntryFinalizerTask::Result;

// Return only the items in the provided |state|.
std::set<PrefetchItem> Select(const std::set<PrefetchItem>& items,
                              PrefetchItemState state) {
  std::set<PrefetchItem> result;
  for (const PrefetchItem& item : items) {
    if (item.state == state)
      result.insert(item);
  }
  return result;
}

// Return only the items with the provided |error_code|.
std::set<PrefetchItem> Select(const std::set<PrefetchItem>& items,
                              PrefetchItemErrorCode error_code) {
  std::set<PrefetchItem> result;
  for (const PrefetchItem& item : items) {
    if (item.error_code == error_code)
      result.insert(item);
  }
  return result;
}

class StaleEntryFinalizerTaskTest : public PrefetchTaskTestBase {
 public:
  StaleEntryFinalizerTaskTest() = default;
  ~StaleEntryFinalizerTaskTest() override = default;

  void SetUp() override;
  void TearDown() override;

  PrefetchItem InsertItemWithFreshnessTime(PrefetchItemState state,
                                           int freshness_delta_in_hours);
  PrefetchItem InsertItemWithCreationTime(PrefetchItemState state,
                                          int creation_delta_in_hours);

  TestPrefetchDispatcher* dispatcher() { return &dispatcher_; }

 protected:
  TestPrefetchDispatcher dispatcher_;
  std::unique_ptr<StaleEntryFinalizerTask> stale_finalizer_task_;
  TestScopedOfflineClock simple_test_clock_;
};

void StaleEntryFinalizerTaskTest::SetUp() {
  PrefetchTaskTestBase::SetUp();
  stale_finalizer_task_ =
      std::make_unique<StaleEntryFinalizerTask>(dispatcher(), store());
  simple_test_clock_.SetNow(base::Time() + base::Days(100));
}

void StaleEntryFinalizerTaskTest::TearDown() {
  stale_finalizer_task_.reset();
  PrefetchTaskTestBase::TearDown();
}

PrefetchItem StaleEntryFinalizerTaskTest::InsertItemWithFreshnessTime(
    PrefetchItemState state,
    int freshness_delta_in_hours) {
  PrefetchItem item(item_generator()->CreateItem(state));
  item.freshness_time =
      simple_test_clock_.Now() + base::Hours(freshness_delta_in_hours);
  item.creation_time = simple_test_clock_.Now();
  EXPECT_TRUE(store_util()->InsertPrefetchItem(item))
      << "Failed inserting item with state " << static_cast<int>(state);
  return item;
}

PrefetchItem StaleEntryFinalizerTaskTest::InsertItemWithCreationTime(
    PrefetchItemState state,
    int creation_delta_in_hours) {
  PrefetchItem item(item_generator()->CreateItem(state));
  item.creation_time =
      simple_test_clock_.Now() + base::Hours(creation_delta_in_hours);
  item.freshness_time = simple_test_clock_.Now();
  EXPECT_TRUE(store_util()->InsertPrefetchItem(item))
      << "Failed inserting item with state " << static_cast<int>(state);
  return item;
}

TEST_F(StaleEntryFinalizerTaskTest, StoreFailure) {
  store_util()->SimulateInitializationError();

  // Execute the expiration task.
  RunTask(stale_finalizer_task_.get());
}

// Tests that the task works correctly with an empty database.
TEST_F(StaleEntryFinalizerTaskTest, EmptyRun) {
  EXPECT_EQ(std::set<PrefetchItem>(), store_util()->GetAllItems());

  // Execute the expiration task.
  RunTask(stale_finalizer_task_.get());
  EXPECT_EQ(Result::NO_MORE_WORK, stale_finalizer_task_->final_status());
  EXPECT_EQ(std::set<PrefetchItem>(), store_util()->GetAllItems());
}

// Verifies that expired and non-expired items from all expirable states are
// properly handled.
TEST_F(StaleEntryFinalizerTaskTest, HandlesFreshnessTimesCorrectly) {
  // Insert fresh and stale items for all expirable states from all buckets.
  PrefetchItem b1_item1_fresh =
      InsertItemWithFreshnessTime(PrefetchItemState::NEW_REQUEST, -23);
  PrefetchItem b1_item2_stale =
      InsertItemWithFreshnessTime(PrefetchItemState::NEW_REQUEST, -25);

  PrefetchItem b2_item1_fresh =
      InsertItemWithFreshnessTime(PrefetchItemState::AWAITING_GCM, -23);
  PrefetchItem b2_item2_stale =
      InsertItemWithFreshnessTime(PrefetchItemState::AWAITING_GCM, -25);
  PrefetchItem b2_item3_fresh =
      InsertItemWithFreshnessTime(PrefetchItemState::RECEIVED_GCM, -23);
  PrefetchItem b2_item4_stale =
      InsertItemWithFreshnessTime(PrefetchItemState::RECEIVED_GCM, -25);
  PrefetchItem b2_item5_fresh =
      InsertItemWithFreshnessTime(PrefetchItemState::RECEIVED_BUNDLE, -23);
  PrefetchItem b2_item6_stale =
      InsertItemWithFreshnessTime(PrefetchItemState::RECEIVED_BUNDLE, -25);

  PrefetchItem b3_item1_fresh =
      InsertItemWithFreshnessTime(PrefetchItemState::DOWNLOADING, -47);
  PrefetchItem b3_item2_stale =
      InsertItemWithFreshnessTime(PrefetchItemState::DOWNLOADING, -49);
  PrefetchItem b3_item3_fresh =
      InsertItemWithFreshnessTime(PrefetchItemState::IMPORTING, -47);
  PrefetchItem b3_item4_stale =
      InsertItemWithFreshnessTime(PrefetchItemState::IMPORTING, -49);

  // Check inserted initial items.
  std::set<PrefetchItem> initial_items = {
      b1_item1_fresh, b1_item2_stale, b2_item1_fresh, b2_item2_stale,
      b2_item3_fresh, b2_item4_stale, b2_item5_fresh, b2_item6_stale,
      b3_item1_fresh, b3_item2_stale, b3_item3_fresh, b3_item4_stale};
  EXPECT_EQ(initial_items, store_util()->GetAllItems());

  // Execute the expiration task.
  RunTask(stale_finalizer_task_.get());
  EXPECT_EQ(Result::MORE_WORK_NEEDED, stale_finalizer_task_->final_status());

  // Create the expected finished version of each stale item.
  PrefetchItem b1_item2_finished(b1_item2_stale);
  b1_item2_finished.state = PrefetchItemState::FINISHED;
  b1_item2_finished.error_code = PrefetchItemErrorCode::STALE_AT_NEW_REQUEST;
  PrefetchItem b2_item2_finished(b2_item2_stale);
  b2_item2_finished.state = PrefetchItemState::FINISHED;
  b2_item2_finished.error_code = PrefetchItemErrorCode::STALE_AT_AWAITING_GCM;
  PrefetchItem b2_item4_finished(b2_item4_stale);
  b2_item4_finished.state = PrefetchItemState::FINISHED;
  b2_item4_finished.error_code = PrefetchItemErrorCode::STALE_AT_RECEIVED_GCM;
  PrefetchItem b2_item6_finished(b2_item6_stale);
  b2_item6_finished.state = PrefetchItemState::FINISHED;
  b2_item6_finished.error_code =
      PrefetchItemErrorCode::STALE_AT_RECEIVED_BUNDLE;
  PrefetchItem b3_item2_finished(b3_item2_stale);
  b3_item2_finished.state = PrefetchItemState::FINISHED;
  b3_item2_finished.error_code = PrefetchItemErrorCode::STALE_AT_DOWNLOADING;
  PrefetchItem b3_item4_finished(b3_item4_stale);
  b3_item4_finished.state = PrefetchItemState::FINISHED;
  b3_item4_finished.error_code = PrefetchItemErrorCode::STALE_AT_IMPORTING;

  // Creates the expected set of final items and compares with what's in store.
  std::set<PrefetchItem> expected_final_items = {
      b1_item1_fresh, b1_item2_finished, b2_item1_fresh, b2_item2_finished,
      b2_item3_fresh, b2_item4_finished, b2_item5_fresh, b2_item6_finished,
      b3_item1_fresh, b3_item2_finished, b3_item3_fresh, b3_item4_finished};
  EXPECT_EQ(expected_final_items, store_util()->GetAllItems());
}

// Checks that items from all states are handled properly by the task when all
// their freshness dates are really old.
TEST_F(StaleEntryFinalizerTaskTest, HandlesStalesInAllStatesCorrectly) {
  // Insert "stale" items for every state.
  // We want a longer time than the pipeline normally takes, but shorter
  // than the point at which we report items as too old.
  const int many_hours = -6 * 24;
  for (PrefetchItemState state : kOrderedPrefetchItemStates)
    InsertItemWithFreshnessTime(state, many_hours);
  EXPECT_EQ(11, store_util()->CountPrefetchItems());

  // Execute the expiration task.
  RunTask(stale_finalizer_task_.get());
  EXPECT_EQ(Result::MORE_WORK_NEEDED, stale_finalizer_task_->final_status());

  // Checks item counts for states expected to still exist.
  std::set<PrefetchItem> post_items = store_util()->GetAllItems();
  EXPECT_EQ(11U, post_items.size());
  EXPECT_EQ(
      1U,
      Select(post_items, PrefetchItemState::SENT_GENERATE_PAGE_BUNDLE).size());
  EXPECT_EQ(1U,
            Select(post_items, PrefetchItemState::SENT_GET_OPERATION).size());
  EXPECT_EQ(1U, Select(post_items, PrefetchItemState::DOWNLOADED).size());
  EXPECT_EQ(7U, Select(post_items, PrefetchItemState::FINISHED).size());
  EXPECT_EQ(1U, Select(post_items, PrefetchItemState::ZOMBIE).size());
}

// Items in states AWAITING_GCM and ZOMBIE should cause the task to finish with
// a NO_MORE_WORK result.
TEST_F(StaleEntryFinalizerTaskTest, NoWorkInQueue) {
  InsertItemWithFreshnessTime(PrefetchItemState::AWAITING_GCM, 0);
  InsertItemWithFreshnessTime(PrefetchItemState::ZOMBIE, 0);

  RunTask(stale_finalizer_task_.get());
  EXPECT_EQ(Result::NO_MORE_WORK, stale_finalizer_task_->final_status());
  EXPECT_EQ(0, dispatcher()->task_schedule_count);
}

// Items in any state but AWAITING_GCM and ZOMBIE should cause the task to
// finish with a MORE_WORK_NEEDED result.
TEST_F(StaleEntryFinalizerTaskTest, WorkInQueue) {
  std::vector<PrefetchItemState> work_states = GetAllStatesExcept(
      {PrefetchItemState::AWAITING_GCM, PrefetchItemState::ZOMBIE});

  for (auto& state : work_states) {
    store_util()->DeleteStore();
    store_util()->BuildStoreInMemory();
    dispatcher()->task_schedule_count = 0;

    PrefetchItem item = item_generator()->CreateItem(state);
    ASSERT_TRUE(store_util()->InsertPrefetchItem(item))
        << "Failed inserting item with state " << static_cast<int>(state);

    StaleEntryFinalizerTask task(dispatcher(), store());
    RunTask(&task);
    EXPECT_EQ(Result::MORE_WORK_NEEDED, task.final_status());
    EXPECT_EQ(1, dispatcher()->task_schedule_count);
  }
}

// Verifies that expired and non-expired items from all expirable states are
// properly handled.
TEST_F(StaleEntryFinalizerTaskTest, HandlesClockSetBackwardsCorrectly) {
  // Insert fresh and stale items for all expirable states from all buckets.
  PrefetchItem b1_item1_recent =
      InsertItemWithFreshnessTime(PrefetchItemState::NEW_REQUEST, 23);
  PrefetchItem b1_item2_future =
      InsertItemWithFreshnessTime(PrefetchItemState::NEW_REQUEST, 25);

  PrefetchItem b2_item1_recent =
      InsertItemWithFreshnessTime(PrefetchItemState::AWAITING_GCM, 23);
  PrefetchItem b2_item2_future =
      InsertItemWithFreshnessTime(PrefetchItemState::AWAITING_GCM, 25);
  PrefetchItem b2_item3_recent =
      InsertItemWithFreshnessTime(PrefetchItemState::RECEIVED_GCM, 23);
  PrefetchItem b2_item4_future =
      InsertItemWithFreshnessTime(PrefetchItemState::RECEIVED_GCM, 25);
  PrefetchItem b2_item5_recent =
      InsertItemWithFreshnessTime(PrefetchItemState::RECEIVED_BUNDLE, 23);
  PrefetchItem b2_item6_future =
      InsertItemWithFreshnessTime(PrefetchItemState::RECEIVED_BUNDLE, 25);

  PrefetchItem b3_item1_recent =
      InsertItemWithFreshnessTime(PrefetchItemState::DOWNLOADING, 23);
  PrefetchItem b3_item2_future =
      InsertItemWithFreshnessTime(PrefetchItemState::DOWNLOADING, 25);
  PrefetchItem b3_item3_recent =
      InsertItemWithFreshnessTime(PrefetchItemState::IMPORTING, 23);
  PrefetchItem b3_item4_future =
      InsertItemWithFreshnessTime(PrefetchItemState::IMPORTING, 25);

  PrefetchItem b4_item1_future = InsertItemWithFreshnessTime(
      PrefetchItemState::SENT_GENERATE_PAGE_BUNDLE, 25);

  // Check inserted initial items.
  std::set<PrefetchItem> initial_items = {
      b1_item1_recent, b1_item2_future, b2_item1_recent, b2_item2_future,
      b2_item3_recent, b2_item4_future, b2_item5_recent, b2_item6_future,
      b3_item1_recent, b3_item2_future, b3_item3_recent, b3_item4_future,
      b4_item1_future};
  EXPECT_EQ(initial_items, store_util()->GetAllItems());

  // Execute the expiration task.
  RunTask(stale_finalizer_task_.get());
  EXPECT_EQ(Result::MORE_WORK_NEEDED, stale_finalizer_task_->final_status());

  // Create the expected finished version of each stale item.
  PrefetchItem b1_item2_finished(b1_item2_future);
  b1_item2_finished.state = PrefetchItemState::FINISHED;
  b1_item2_finished.error_code =
      PrefetchItemErrorCode::MAXIMUM_CLOCK_BACKWARD_SKEW_EXCEEDED;
  PrefetchItem b2_item2_finished(b2_item2_future);
  b2_item2_finished.state = PrefetchItemState::FINISHED;
  b2_item2_finished.error_code =
      PrefetchItemErrorCode::MAXIMUM_CLOCK_BACKWARD_SKEW_EXCEEDED;
  PrefetchItem b2_item4_finished(b2_item4_future);
  b2_item4_finished.state = PrefetchItemState::FINISHED;
  b2_item4_finished.error_code =
      PrefetchItemErrorCode::MAXIMUM_CLOCK_BACKWARD_SKEW_EXCEEDED;
  PrefetchItem b2_item6_finished(b2_item6_future);
  b2_item6_finished.state = PrefetchItemState::FINISHED;
  b2_item6_finished.error_code =
      PrefetchItemErrorCode::MAXIMUM_CLOCK_BACKWARD_SKEW_EXCEEDED;
  PrefetchItem b3_item2_finished(b3_item2_future);
  b3_item2_finished.state = PrefetchItemState::FINISHED;
  b3_item2_finished.error_code =
      PrefetchItemErrorCode::MAXIMUM_CLOCK_BACKWARD_SKEW_EXCEEDED;
  PrefetchItem b3_item4_finished(b3_item4_future);
  b3_item4_finished.state = PrefetchItemState::FINISHED;
  b3_item4_finished.error_code =
      PrefetchItemErrorCode::MAXIMUM_CLOCK_BACKWARD_SKEW_EXCEEDED;
  PrefetchItem b4_item_finished(b4_item1_future);
  b4_item1_future.state = PrefetchItemState::SENT_GENERATE_PAGE_BUNDLE;
  b4_item1_future.error_code = PrefetchItemErrorCode::SUCCESS;

  // Creates the expected set of final items and compares with what's in
  // store.
  std::set<PrefetchItem> expected_final_items = {
      b1_item1_recent, b1_item2_finished, b2_item1_recent, b2_item2_finished,
      b2_item3_recent, b2_item4_finished, b2_item5_recent, b2_item6_finished,
      b3_item1_recent, b3_item2_finished, b3_item3_recent, b3_item4_finished,
      b4_item1_future};
  EXPECT_EQ(expected_final_items, store_util()->GetAllItems());
}

// Checks that items from all states are handled properly by the task when all
// their freshness dates are really old.
TEST_F(StaleEntryFinalizerTaskTest,
       HandleClockChangeBackwardsInAllStatesCorrectly) {
  // Insert "future" items for every state.
  const int many_hours = 7 * 24;
  for (PrefetchItemState state : kOrderedPrefetchItemStates)
    InsertItemWithFreshnessTime(state, many_hours);
  EXPECT_EQ(11, store_util()->CountPrefetchItems());

  // Execute the expiration task.
  RunTask(stale_finalizer_task_.get());
  EXPECT_EQ(Result::MORE_WORK_NEEDED, stale_finalizer_task_->final_status());

  // Checks item counts for states expected to still exist. The zombie item is
  // expected to be deleted.
  std::set<PrefetchItem> post_items = store_util()->GetAllItems();
  EXPECT_EQ(10U, post_items.size());
  EXPECT_EQ(
      1U,
      Select(post_items, PrefetchItemState::SENT_GENERATE_PAGE_BUNDLE).size());
  EXPECT_EQ(1U,
            Select(post_items, PrefetchItemState::SENT_GET_OPERATION).size());
  EXPECT_EQ(1U, Select(post_items, PrefetchItemState::DOWNLOADED).size());
  EXPECT_EQ(7U, Select(post_items, PrefetchItemState::FINISHED).size());
}

TEST_F(StaleEntryFinalizerTaskTest, HandlesStuckItemsCorrectly) {
  base::HistogramTester histogram_tester;
  // Insert stuck and non stuck items for all expirable states from all buckets.
  // Note that stuck items are determined based on creation time instead of
  // freshness.
  for (PrefetchItemState state : kOrderedPrefetchItemStates) {
    InsertItemWithCreationTime(state, -1);
    InsertItemWithCreationTime(state, -170);  // 170h is a bit more than a week.
  }
  EXPECT_EQ(22, store_util()->CountPrefetchItems());

  // Execute the expiration task.
  RunTask(stale_finalizer_task_.get());
  EXPECT_EQ(Result::MORE_WORK_NEEDED, stale_finalizer_task_->final_status());

  std::set<PrefetchItem> final_items = store_util()->GetAllItems();

  EXPECT_EQ(22U, final_items.size());
  // Zombie items should still be there.
  EXPECT_EQ(2U, Select(final_items, PrefetchItemState::ZOMBIE).size());
  // Stuck entries should have been finalized with appropriate error codes. The
  // initially inserter finished items should still be there, with a "success"
  // error code.
  std::set<PrefetchItem> final_finished_items =
      Select(final_items, PrefetchItemState::FINISHED);
  EXPECT_EQ(11U, final_finished_items.size());
  EXPECT_EQ(9U,
            Select(final_finished_items, PrefetchItemErrorCode::STUCK).size());
  EXPECT_EQ(
      2U, Select(final_finished_items, PrefetchItemErrorCode::SUCCESS).size());

  // All other non-stuck items should remain as they were.
  std::vector<PrefetchItemState> stuck_states = GetAllStatesExcept(
      {PrefetchItemState::FINISHED, PrefetchItemState::ZOMBIE});
  for (PrefetchItemState state : stuck_states)
    EXPECT_EQ(1U, Select(final_items, state).size());

  // Check metrics were reported for all stuck entries but not for finished nor
  // zombie items.
  histogram_tester.ExpectTotalCount("OfflinePages.Prefetching.StuckItemState",
                                    9);
  for (PrefetchItemState state : stuck_states) {
    histogram_tester.ExpectBucketCount(
        "OfflinePages.Prefetching.StuckItemState", state, 1);
  }
}

TEST_F(StaleEntryFinalizerTaskTest, HandlesZombieFreshnessTimesCorrectly) {
  PrefetchItem zombie_item1_fresh =
      InsertItemWithFreshnessTime(PrefetchItemState::ZOMBIE, -160);
  PrefetchItem zombie_item2_expired =
      InsertItemWithFreshnessTime(PrefetchItemState::ZOMBIE, -170);
  PrefetchItem zombie_item3_future_fresh =
      InsertItemWithFreshnessTime(PrefetchItemState::ZOMBIE, 23);
  PrefetchItem zombie_item3_future_expired =
      InsertItemWithFreshnessTime(PrefetchItemState::ZOMBIE, 25);

  // Check inserted initial items.
  std::set<PrefetchItem> initial_items = {
      zombie_item1_fresh, zombie_item2_expired, zombie_item3_future_fresh,
      zombie_item3_future_expired};
  EXPECT_EQ(initial_items, store_util()->GetAllItems());

  // Execute the expiration task.
  RunTask(stale_finalizer_task_.get());
  EXPECT_EQ(Result::NO_MORE_WORK, stale_finalizer_task_->final_status());

  // Only the unexpired zombie should remain.
  std::set<PrefetchItem> expected_final_items = {zombie_item1_fresh,
                                                 zombie_item3_future_fresh};
  EXPECT_EQ(expected_final_items, store_util()->GetAllItems());
}

}  // namespace offline_pages
