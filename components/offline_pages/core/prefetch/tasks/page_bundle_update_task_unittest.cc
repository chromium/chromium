// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/tasks/page_bundle_update_task.h"

#include "base/logging.h"
#include "base/test/mock_callback.h"
#include "components/offline_pages/core/prefetch/prefetch_item.h"
#include "components/offline_pages/core/prefetch/prefetch_types.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store_test_util.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store_utils.h"
#include "components/offline_pages/core/prefetch/tasks/prefetch_task_test_base.h"
#include "components/offline_pages/core/prefetch/test_prefetch_dispatcher.h"
#include "components/offline_pages/task/task.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::HasSubstr;

namespace offline_pages {

class PageBundleUpdateTaskTest : public PrefetchTaskTestBase {
 public:
  PageBundleUpdateTaskTest() = default;
  ~PageBundleUpdateTaskTest() override = default;

  TestPrefetchDispatcher dispatcher;

  RenderPageInfo FailedPageInfoForPrefetchItem(const PrefetchItem& item) {
    RenderPageInfo rv;

    rv.url = item.url.spec();
    rv.status = RenderStatus::FAILED;
    return rv;
  }

  RenderPageInfo PendingPageInfoForPrefetchItem(const PrefetchItem& item) {
    RenderPageInfo rv;

    rv.url = item.url.spec();
    rv.status = RenderStatus::PENDING;
    return rv;
  }

  RenderPageInfo RenderedPageInfoForPrefetchItem(const PrefetchItem& item) {
    RenderPageInfo rv;

    rv.url = item.url.spec();
    rv.status = RenderStatus::RENDERED;
    rv.body_name = std::to_string(++body_name_count_);
    rv.body_length = 1024 + body_name_count_;  // a realistic number of bytes

    rv.render_time = base::Time::Now();
    return rv;
  }

 private:
  int body_name_count_ = 0;
};

TEST_F(PageBundleUpdateTaskTest, StoreFailure) {
  store_util()->SimulateInitializationError();
  PageBundleUpdateTask task(store(), &dispatcher, "operation", {});
  RunTask(&task);
}

TEST_F(PageBundleUpdateTaskTest, EmptyTask) {
  PageBundleUpdateTask task(store(), &dispatcher, "operation", {});
  RunTask(&task);
  EXPECT_EQ(0, dispatcher.processing_schedule_count);
}

TEST_F(PageBundleUpdateTaskTest, UpdatesItemsFromSentGeneratePageBundle) {
  // Tests that SENT_GENERATE_PAGE_BUNDLE can correctly transition pages to
  // RECEIVED_BUNDLE.
  PrefetchItem item1 = item_generator()->CreateItem(
      PrefetchItemState::SENT_GENERATE_PAGE_BUNDLE);
  ASSERT_EQ("", item1.operation_name);
  PrefetchItem item2 =
      item_generator()->CreateItem(PrefetchItemState::AWAITING_GCM);
  ASSERT_NE("", item2.operation_name);
  ASSERT_TRUE(store_util()->InsertPrefetchItem(item1));
  ASSERT_TRUE(store_util()->InsertPrefetchItem(item2));
  ASSERT_EQ(2, store_util()->CountPrefetchItems());

  // We assume that the bundle contains items for two entries, but one of those
  // entries is not in the correct state to be updated (different operation
  // name).
  PageBundleUpdateTask task(store(), &dispatcher, "operation",
                            {
                                RenderedPageInfoForPrefetchItem(item1),
                                RenderedPageInfoForPrefetchItem(item2),
                            });
  RunTask(&task);

  // The matching entry has been updated.
  const PrefetchItem stored_item =
      *store_util()->GetPrefetchItem(item1.offline_id).get();
  EXPECT_EQ(PrefetchItemState::RECEIVED_BUNDLE, stored_item.state);
  EXPECT_NE("", stored_item.archive_body_name);
  EXPECT_LT(0, stored_item.archive_body_length);
  EXPECT_EQ("operation", stored_item.operation_name);
  // The non-matching entry has not changed.
  EXPECT_EQ(item2, *(store_util()->GetPrefetchItem(item2.offline_id)));
  // The dispatcher knows to reschedule the action tasks.
  EXPECT_LE(1, dispatcher.processing_schedule_count);
}

TEST_F(PageBundleUpdateTaskTest, SentGetOperationToReceivedBundle) {
  PrefetchItem item1 =
      item_generator()->CreateItem(PrefetchItemState::SENT_GET_OPERATION);
  ASSERT_EQ("", item1.archive_body_name);
  ASSERT_EQ(-1, item1.archive_body_length);

  ASSERT_TRUE(store_util()->InsertPrefetchItem(item1));

  PageBundleUpdateTask task(store(), &dispatcher, item1.operation_name,
                            {
                                RenderedPageInfoForPrefetchItem(item1),
                            });
  RunTask(&task);

  // The matching entry has been updated.
  const PrefetchItem stored_item =
      *store_util()->GetPrefetchItem(item1.offline_id).get();
  EXPECT_EQ(stored_item.state, PrefetchItemState::RECEIVED_BUNDLE);
  EXPECT_NE("", stored_item.archive_body_name);
  EXPECT_LT(0, stored_item.archive_body_length);
  EXPECT_EQ(item1.operation_name, stored_item.operation_name);
  EXPECT_LE(1, dispatcher.processing_schedule_count);
}

TEST_F(PageBundleUpdateTaskTest, UrlDoesNotMatch) {
  // Tests that no change happens if the URLs passed into the task don't match
  // the ones in the store.
  PrefetchItem item = item_generator()->CreateItem(
      PrefetchItemState::SENT_GENERATE_PAGE_BUNDLE);
  ASSERT_TRUE(store_util()->InsertPrefetchItem(item));

  // Dummy item not inserted into store.
  PrefetchItem item2 = item_generator()->CreateItem(
      PrefetchItemState::SENT_GENERATE_PAGE_BUNDLE);

  ASSERT_NE(item.url, item2.url);

  PageBundleUpdateTask task(store(), &dispatcher, "operation",
                            {
                                RenderedPageInfoForPrefetchItem(item2),
                            });
  RunTask(&task);
  EXPECT_EQ(item, *(store_util()->GetPrefetchItem(item.offline_id)));
  EXPECT_EQ(0, dispatcher.processing_schedule_count);
}

TEST_F(PageBundleUpdateTaskTest, PendingRenderAwaitsGCM) {
  // Tests that the transition to AWAITING_GCM happens correctly from
  // SENT_GENERATE_PAGE_BUNDLE.
  PrefetchItem item = item_generator()->CreateItem(
      PrefetchItemState::SENT_GENERATE_PAGE_BUNDLE);
  ASSERT_EQ("", item.operation_name);
  ASSERT_TRUE(store_util()->InsertPrefetchItem(item));

  PageBundleUpdateTask task(store(), &dispatcher, "operation",
                            {
                                PendingPageInfoForPrefetchItem(item),
                            });
  RunTask(&task);

  EXPECT_EQ(PrefetchItemState::AWAITING_GCM,
            store_util()->GetPrefetchItem(item.offline_id)->state);
  EXPECT_EQ("operation",
            store_util()->GetPrefetchItem(item.offline_id)->operation_name);
  EXPECT_EQ(0, dispatcher.processing_schedule_count);
}

TEST_F(PageBundleUpdateTaskTest, FailedRenderToFinished) {
  // Tests one item with a archiving failed result, and one with a limit
  // exceeded result.
  PrefetchItem item1 = item_generator()->CreateItem(
      PrefetchItemState::SENT_GENERATE_PAGE_BUNDLE);
  item1.operation_name = "operation";
  ASSERT_EQ(PrefetchItemErrorCode::SUCCESS, item1.error_code);
  ASSERT_TRUE(store_util()->InsertPrefetchItem(item1));
  RenderPageInfo item1_render_info = FailedPageInfoForPrefetchItem(item1);

  PrefetchItem item2 = item_generator()->CreateItem(
      PrefetchItemState::SENT_GENERATE_PAGE_BUNDLE);
  item2.operation_name = "operation";
  ASSERT_EQ(PrefetchItemErrorCode::SUCCESS, item2.error_code);
  ASSERT_TRUE(store_util()->InsertPrefetchItem(item2));
  RenderPageInfo item2_render_info = FailedPageInfoForPrefetchItem(item2);
  item2_render_info.status = RenderStatus::EXCEEDED_LIMIT;
  PageBundleUpdateTask task(store(), &dispatcher, "operation",
                            {item2_render_info, item1_render_info});
  RunTask(&task);

  EXPECT_EQ(PrefetchItemState::FINISHED,
            store_util()->GetPrefetchItem(item1.offline_id)->state);
  EXPECT_EQ(PrefetchItemErrorCode::ARCHIVING_FAILED,
            store_util()->GetPrefetchItem(item1.offline_id)->error_code);
  EXPECT_EQ("operation",
            store_util()->GetPrefetchItem(item1.offline_id)->operation_name);

  EXPECT_EQ(PrefetchItemState::FINISHED,
            store_util()->GetPrefetchItem(item2.offline_id)->state);
  EXPECT_EQ(PrefetchItemErrorCode::ARCHIVING_LIMIT_EXCEEDED,
            store_util()->GetPrefetchItem(item2.offline_id)->error_code);
  EXPECT_EQ("operation",
            store_util()->GetPrefetchItem(item2.offline_id)->operation_name);
  EXPECT_EQ(0, dispatcher.processing_schedule_count);
}

TEST_F(PageBundleUpdateTaskTest, MixOfResults) {
  // Tests that 3 items all with different render results get updated
  // independently and properly.
  PrefetchItem item1 = item_generator()->CreateItem(
      PrefetchItemState::SENT_GENERATE_PAGE_BUNDLE);
  item1.operation_name = "operation";
  ASSERT_TRUE(store_util()->InsertPrefetchItem(item1));
  RenderPageInfo item1_render_info = PendingPageInfoForPrefetchItem(item1);

  PrefetchItem item2 = item_generator()->CreateItem(
      PrefetchItemState::SENT_GENERATE_PAGE_BUNDLE);
  item2.operation_name = "operation";
  ASSERT_TRUE(store_util()->InsertPrefetchItem(item2));
  RenderPageInfo item2_render_info = FailedPageInfoForPrefetchItem(item2);

  PrefetchItem item3 = item_generator()->CreateItem(
      PrefetchItemState::SENT_GENERATE_PAGE_BUNDLE);
  item3.operation_name = "operation";
  ASSERT_TRUE(store_util()->InsertPrefetchItem(item3));
  RenderPageInfo item3_render_info = RenderedPageInfoForPrefetchItem(item3);

  PageBundleUpdateTask task(
      store(), &dispatcher, "operation",
      {item3_render_info, item2_render_info, item1_render_info});
  RunTask(&task);

  EXPECT_EQ(PrefetchItemState::AWAITING_GCM,
            store_util()->GetPrefetchItem(item1.offline_id)->state);
  EXPECT_EQ(PrefetchItemState::FINISHED,
            store_util()->GetPrefetchItem(item2.offline_id)->state);
  EXPECT_EQ(PrefetchItemState::RECEIVED_BUNDLE,
            store_util()->GetPrefetchItem(item3.offline_id)->state);
  EXPECT_LE(1, dispatcher.processing_schedule_count);
}

}  // namespace offline_pages
