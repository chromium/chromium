// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/tasks/mark_operation_done_task.h"

#include <set>
#include <string>
#include <vector>

#include "components/offline_pages/core/prefetch/prefetch_item.h"
#include "components/offline_pages/core/prefetch/prefetch_types.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store_test_util.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store_utils.h"
#include "components/offline_pages/core/prefetch/tasks/prefetch_task_test_base.h"
#include "components/offline_pages/core/prefetch/test_prefetch_dispatcher.h"
#include "components/offline_pages/core/prefetch/test_prefetch_gcm_handler.h"
#include "components/offline_pages/task/task.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

const char kOperationName[] = "an_operation";
const char kOtherOperationName[] = "other_operation";

// All tests cases here only validate the request data and check for general
// http response. The tests for the Operation proto data returned in the http
// response are covered in PrefetchRequestOperationResponseTest.
class MarkOperationDoneTaskTest : public PrefetchTaskTestBase {
 public:
  MarkOperationDoneTaskTest() = default;
  ~MarkOperationDoneTaskTest() override = default;

  int64_t InsertAwaitingGCMOperation(std::string name) {
    return InsertPrefetchItemInStateWithOperation(
        name, PrefetchItemState::AWAITING_GCM);
  }

  int64_t InsertPrefetchItemInStateWithOperation(std::string operation_name,
                                                 PrefetchItemState state) {
    PrefetchItem item = item_generator()->CreateItem(state);
    item.operation_name = operation_name;
    EXPECT_TRUE(store_util()->InsertPrefetchItem(item));
    return item.offline_id;
  }

  void ExpectStoreChangeCount(MarkOperationDoneTask* task,
                              int64_t change_count) {
    EXPECT_EQ(MarkOperationDoneTask::StoreResult::UPDATED,
              task->store_result());
    EXPECT_EQ(change_count, task->change_count());
    EXPECT_EQ(change_count > 0 ? 1 : 0, dispatcher()->task_schedule_count);
  }

  TestPrefetchDispatcher* dispatcher() { return &dispatcher_; }

 private:
  TestPrefetchDispatcher dispatcher_;
};

TEST_F(MarkOperationDoneTaskTest, StoreFailure) {
  store_util()->SimulateInitializationError();

  RunTask(std::make_unique<MarkOperationDoneTask>(dispatcher(), store(),
                                                  kOperationName));
}

TEST_F(MarkOperationDoneTaskTest, NoOpTask) {
  MarkOperationDoneTask task(dispatcher(), store(), kOperationName);
  RunTask(&task);
  ExpectStoreChangeCount(&task, 0);
}

TEST_F(MarkOperationDoneTaskTest, SingleMatchingURL) {
  int64_t id = InsertAwaitingGCMOperation(kOperationName);
  MarkOperationDoneTask task(dispatcher(), store(), kOperationName);
  RunTask(&task);
  ExpectStoreChangeCount(&task, 1);

  EXPECT_EQ(1, store_util()->CountPrefetchItems());
  ASSERT_TRUE(store_util()->GetPrefetchItem(id));
  EXPECT_EQ(PrefetchItemState::RECEIVED_GCM,
            store_util()->GetPrefetchItem(id)->state);
}

TEST_F(MarkOperationDoneTaskTest, NoSuchURLs) {
  // Insert a record with an operation name.
  int64_t id1 = InsertAwaitingGCMOperation(kOperationName);

  // Start a task for an unrelated operation name.
  MarkOperationDoneTask task(dispatcher(), store(), kOtherOperationName);
  RunTask(&task);
  ExpectStoreChangeCount(&task, 0);

  ASSERT_TRUE(store_util()->GetPrefetchItem(id1));
  EXPECT_EQ(PrefetchItemState::AWAITING_GCM,
            store_util()->GetPrefetchItem(id1)->state);
}

TEST_F(MarkOperationDoneTaskTest, ManyURLs) {
  // Create 5 records with an operation name.
  std::vector<int64_t> ids;
  for (int i = 0; i < 5; i++) {
    ids.push_back(InsertAwaitingGCMOperation(kOperationName));
  }

  // Insert a record with a different operation name.
  int64_t id_other = InsertAwaitingGCMOperation(kOtherOperationName);

  ASSERT_EQ(6, store_util()->CountPrefetchItems());

  // Start a task for the first operation name.
  MarkOperationDoneTask task(dispatcher(), store(), kOperationName);
  RunTask(&task);
  ExpectStoreChangeCount(&task, ids.size());

  // The items should be in the new state.
  for (int64_t id : ids) {
    auto item = store_util()->GetPrefetchItem(id);
    ASSERT_TRUE(item);
    EXPECT_EQ(PrefetchItemState::RECEIVED_GCM, item->state);
  }

  // The other item should not be changed.
  ASSERT_TRUE(store_util()->GetPrefetchItem(id_other));
  EXPECT_EQ(PrefetchItemState::AWAITING_GCM,
            store_util()->GetPrefetchItem(id_other)->state);
}

TEST_F(MarkOperationDoneTaskTest, URLsInWrongState) {
  // Insert items in all states but AWAITING_GCM.
  std::set<PrefetchItem> inserted_items;
  for (PrefetchItemState state :
       GetAllStatesExcept({PrefetchItemState::AWAITING_GCM})) {
    PrefetchItem item = item_generator()->CreateItem(state);
    item.operation_name = kOperationName;
    EXPECT_TRUE(store_util()->InsertPrefetchItem(item));
    inserted_items.insert(item);
  }

  // Start a task for the operation name.
  MarkOperationDoneTask task(dispatcher(), store(), kOperationName);
  RunTask(&task);
  ExpectStoreChangeCount(&task, 0);

  // No item should have been changed.
  std::set<PrefetchItem> items_after_run;
  EXPECT_EQ(inserted_items.size(), store_util()->GetAllItems(&items_after_run));
  EXPECT_EQ(inserted_items, items_after_run);
}

}  // namespace offline_pages
