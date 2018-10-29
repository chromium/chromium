// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/tasks/stale_entry_finalizer_task.h"

#include <memory>
#include <set>

#include "base/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/offline_pages/core/prefetch/mock_prefetch_item_generator.h"
#include "components/offline_pages/core/prefetch/prefetch_item.h"
#include "components/offline_pages/core/prefetch/prefetch_types.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store_test_util.h"
#include "components/offline_pages/core/prefetch/tasks/prefetch_task_test_base.h"
#include "components/offline_pages/core/prefetch/test_prefetch_dispatcher.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

using Result = StaleEntryFinalizerTask::Result;

std::set<PrefetchItem> Filter(const std::set<PrefetchItem>& items,
                              PrefetchItemState state) {
  std::set<PrefetchItem> result;
  for (const PrefetchItem& item : items) {
    if (item.state == state)
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

  PrefetchItem CreateAndInsertItem(PrefetchItemState state,
                                   int time_delta_in_hours);

  TestPrefetchDispatcher* dispatcher() { return &dispatcher_; }

 protected:
  TestPrefetchDispatcher dispatcher_;
  std::unique_ptr<StaleEntryFinalizerTask> stale_finalizer_task_;
  base::Time fake_now_;
};

void StaleEntryFinalizerTaskTest::SetUp() {
  PrefetchTaskTestBase::SetUp();
  stale_finalizer_task_ =
      std::make_unique<StaleEntryFinalizerTask>(dispatcher(), store());
  fake_now_ = base::Time() + base::TimeDelta::FromDays(100);
  stale_finalizer_task_->SetNowGetterForTesting(base::BindRepeating(
      [](base::Time t) -> base::Time { return t; }, fake_now_));
}

void StaleEntryFinalizerTaskTest::TearDown() {
  stale_finalizer_task_.reset();
  PrefetchTaskTestBase::TearDown();
}

PrefetchItem StaleEntryFinalizerTaskTest::CreateAndInsertItem(
    PrefetchItemState state,
    int time_delta_in_hours) {
  PrefetchItem item(item_generator()->CreateItem(state));
  item.freshness_time =
      fake_now_ + base::TimeDelta::FromHours(time_delta_in_hours);
  item.creation_time = item.freshness_time;
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
      CreateAndInsertItem(PrefetchItemState::NEW_REQUEST, -23);
  PrefetchItem b1_item2_stale =
      CreateAndInsertItem(PrefetchItemState::NEW_REQUEST, -25);

  PrefetchItem b2_item1_fresh =
      CreateAndInsertItem(PrefetchItemState::AWAITING_GCM, -23);
  PrefetchItem b2_item2_stale =
      CreateAndInsertItem(PrefetchItemState::AWAITING_GCM, -25);
  PrefetchItem b2_item3_fresh =
      CreateAndInsertItem(PrefetchItemState::RECEIVED_GCM, -23);
  PrefetchItem b2_item4_stale =
      CreateAndInsertItem(PrefetchItemState::RECEIVED_GCM, -25);
  PrefetchItem b2_item5_fresh =
      CreateAndInsertItem(PrefetchItemState::RECEIVED_BUNDLE, -23);
  PrefetchItem b2_item6_stale =
      CreateAndInsertItem(PrefetchItemState::RECEIVED_BUNDLE, -25);

  PrefetchItem b3_item1_fresh =
      CreateAndInsertItem(PrefetchItemState::DOWNLOADING, -47);
  PrefetchItem b3_item2_stale =
      CreateAndInsertItem(PrefetchItemState::DOWNLOADING, -49);
  PrefetchItem b3_item3_fresh =
      CreateAndInsertItem(PrefetchItemState::IMPORTING, -47);
  PrefetchItem b3_item4_stale =
      CreateAndInsertItem(PrefetchItemState::IMPORTING, -49);

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
    CreateAndInsertItem(state, many_hours);
  EXPECT_EQ(11, store_util()->CountPrefetchItems());

  // Execute the expiration task.
  RunTask(stale_finalizer_task_.get());
  EXPECT_EQ(Result::MORE_WORK_NEEDED, stale_finalizer_task_->final_status());

  // Checks item counts for states expected to still exist.
  std::set<PrefetchItem> post_items = store_util()->GetAllItems();
  EXPECT_EQ(11U, post_items.size());
  EXPECT_EQ(
      1U,
      Filter(post_items, PrefetchItemState::SENT_GENERATE_PAGE_BUNDLE).size());
  EXPECT_EQ(1U,
            Filter(post_items, PrefetchItemState::SENT_GET_OPERATION).size());
  EXPECT_EQ(1U, Filter(post_items, PrefetchItemState::DOWNLOADED).size());
  EXPECT_EQ(7U, Filter(post_items, PrefetchItemState::FINISHED).size());
  EXPECT_EQ(1U, Filter(post_items, PrefetchItemState::ZOMBIE).size());
}

// Items in states AWAITING_GCM and ZOMBIE should cause the task to finish with
// a NO_MORE_WORK result.
TEST_F(StaleEntryFinalizerTaskTest, NoWorkInQueue) {
  CreateAndInsertItem(PrefetchItemState::AWAITING_GCM, 0);
  CreateAndInsertItem(PrefetchItemState::ZOMBIE, 0);

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
      CreateAndInsertItem(PrefetchItemState::NEW_REQUEST, 23);
  PrefetchItem b1_item2_future =
      CreateAndInsertItem(PrefetchItemState::NEW_REQUEST, 25);

  PrefetchItem b2_item1_recent =
      CreateAndInsertItem(PrefetchItemState::AWAITING_GCM, 23);
  PrefetchItem b2_item2_future =
      CreateAndInsertItem(PrefetchItemState::AWAITING_GCM, 25);
  PrefetchItem b2_item3_recent =
      CreateAndInsertItem(PrefetchItemState::RECEIVED_GCM, 23);
  PrefetchItem b2_item4_future =
      CreateAndInsertItem(PrefetchItemState::RECEIVED_GCM, 25);
  PrefetchItem b2_item5_recent =
      CreateAndInsertItem(PrefetchItemState::RECEIVED_BUNDLE, 23);
  PrefetchItem b2_item6_future =
      CreateAndInsertItem(PrefetchItemState::RECEIVED_BUNDLE, 25);

  PrefetchItem b3_item1_recent =
      CreateAndInsertItem(PrefetchItemState::DOWNLOADING, 23);
  PrefetchItem b3_item2_future =
      CreateAndInsertItem(PrefetchItemState::DOWNLOADING, 25);
  PrefetchItem b3_item3_recent =
      CreateAndInsertItem(PrefetchItemState::IMPORTING, 23);
  PrefetchItem b3_item4_future =
      CreateAndInsertItem(PrefetchItemState::IMPORTING, 25);

  PrefetchItem b4_item1_future =
      CreateAndInsertItem(PrefetchItemState::SENT_GENERATE_PAGE_BUNDLE, 25);

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
    CreateAndInsertItem(state, many_hours);
  EXPECT_EQ(11, store_util()->CountPrefetchItems());

  // Execute the expiration task.
  RunTask(stale_finalizer_task_.get());
  EXPECT_EQ(Result::MORE_WORK_NEEDED, stale_finalizer_task_->final_status());

  // Checks item counts for states expected to still exist.
  std::set<PrefetchItem> post_items = store_util()->GetAllItems();
  EXPECT_EQ(11U, post_items.size());
  EXPECT_EQ(
      1U,
      Filter(post_items, PrefetchItemState::SENT_GENERATE_PAGE_BUNDLE).size());
  EXPECT_EQ(1U,
            Filter(post_items, PrefetchItemState::SENT_GET_OPERATION).size());
  EXPECT_EQ(1U, Filter(post_items, PrefetchItemState::DOWNLOADED).size());
  EXPECT_EQ(7U, Filter(post_items, PrefetchItemState::FINISHED).size());
  EXPECT_EQ(1U, Filter(post_items, PrefetchItemState::ZOMBIE).size());
}

// Verifies that only stale, live items are transitioned to 'FINISHED'.
TEST_F(StaleEntryFinalizerTaskTest, HandlesStuckItemsCorrectly) {
  base::HistogramTester histogram_tester;
  // Insert fresh and stale items for all expirable states from all buckets.
  PrefetchItem item1_recent =
      CreateAndInsertItem(PrefetchItemState::SENT_GENERATE_PAGE_BUNDLE, 1);
  PrefetchItem item2_stuck =
      CreateAndInsertItem(PrefetchItemState::SENT_GENERATE_PAGE_BUNDLE, -170);
  PrefetchItem item3_finished =
      CreateAndInsertItem(PrefetchItemState::FINISHED, -170);
  PrefetchItem item4_zombie =
      CreateAndInsertItem(PrefetchItemState::ZOMBIE, -170);

  // Check inserted initial items.
  std::set<PrefetchItem> initial_items = {item1_recent, item2_stuck,
                                          item3_finished, item4_zombie};
  EXPECT_EQ(initial_items, store_util()->GetAllItems());

  // Execute the expiration task.
  RunTask(stale_finalizer_task_.get());
  EXPECT_EQ(Result::MORE_WORK_NEEDED, stale_finalizer_task_->final_status());

  // Only the stuck item is changed.
  PrefetchItem want_stuck_item = item2_stuck;
  want_stuck_item.state = PrefetchItemState::FINISHED;
  want_stuck_item.error_code = PrefetchItemErrorCode::STUCK;
  std::set<PrefetchItem> final_items{item1_recent, want_stuck_item,
                                     item3_finished, item4_zombie};
  EXPECT_EQ(final_items, store_util()->GetAllItems());
  // Check that the proper UMA was reported for the stale item, but not the
  // fresh item, so there should be exactly one sample.
  histogram_tester.ExpectUniqueSample(
      "OfflinePages.Prefetching.StuckItemState",
      static_cast<int>(PrefetchItemState::SENT_GENERATE_PAGE_BUNDLE), 1);
}

}  // namespace offline_pages
