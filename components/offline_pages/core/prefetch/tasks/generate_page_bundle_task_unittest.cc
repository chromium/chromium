// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/tasks/generate_page_bundle_task.h"

#include <utility>

#include "base/logging.h"
#include "base/test/mock_callback.h"
#include "base/time/time.h"
#include "components/offline_pages/core/prefetch/prefetch_item.h"
#include "components/offline_pages/core/prefetch/prefetch_types.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store_test_util.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store_utils.h"
#include "components/offline_pages/core/prefetch/tasks/prefetch_task_test_base.h"
#include "components/offline_pages/core/prefetch/test_prefetch_dispatcher.h"
#include "components/offline_pages/core/test_scoped_offline_clock.h"
#include "components/offline_pages/task/task.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Contains;
using testing::HasSubstr;
using testing::Not;

namespace offline_pages {

// All tests cases here only validate the request data and check for general
// http response. The tests for the Operation proto data returned in the http
// response are covered in PrefetchRequestOperationResponseTest.
class GeneratePageBundleTaskTest : public PrefetchTaskTestBase {
 public:
  GeneratePageBundleTaskTest() = default;
  ~GeneratePageBundleTaskTest() override = default;

  std::string gcm_token() { return "dummy_gcm_token"; }

  TestPrefetchDispatcher* dispatcher() { return &dispatcher_; }

 private:
  TestPrefetchDispatcher dispatcher_;
};

TEST_F(GeneratePageBundleTaskTest, StoreFailure) {
  store_util()->SimulateInitializationError();

  base::MockCallback<PrefetchRequestFinishedCallback> callback;
  RunTask(std::make_unique<GeneratePageBundleTask>(
      dispatcher(), store(), gcm_token(), prefetch_request_factory(),
      callback.Get()));
  EXPECT_EQ(0, dispatcher()->generate_page_bundle_requested);
}

TEST_F(GeneratePageBundleTaskTest, EmptyTask) {
  base::MockCallback<PrefetchRequestFinishedCallback> callback;
  RunTask(std::make_unique<GeneratePageBundleTask>(
      dispatcher(), store(), gcm_token(), prefetch_request_factory(),
      callback.Get()));

  EXPECT_FALSE(prefetch_request_factory()->HasOutstandingRequests());
  auto requested_urls = prefetch_request_factory()->GetAllUrlsRequested();
  EXPECT_TRUE(requested_urls->empty());
  EXPECT_EQ(0, dispatcher()->generate_page_bundle_requested);
}

TEST_F(GeneratePageBundleTaskTest, TaskMakesNetworkRequest) {
  base::MockCallback<PrefetchRequestFinishedCallback> request_callback;

  TestScopedOfflineClock clock;

  // This item will be sent with the bundle request.
  PrefetchItem item1 =
      item_generator()->CreateItem(PrefetchItemState::NEW_REQUEST);
  item1.freshness_time = clock.Now();
  item1.creation_time = item1.freshness_time;
  EXPECT_TRUE(store_util()->InsertPrefetchItem(item1));

  clock.Advance(base::TimeDelta::FromSeconds(1));

  // This item will also be sent with the bundle request but being the freshest
  // it will come first in the list.
  PrefetchItem item2 =
      item_generator()->CreateItem(PrefetchItemState::NEW_REQUEST);
  item1.freshness_time = clock.Now();
  item1.creation_time = item1.freshness_time;
  item2.generate_bundle_attempts = 1;
  EXPECT_TRUE(store_util()->InsertPrefetchItem(item2));
  EXPECT_NE(item1.offline_id, item2.offline_id);

  // This item should be unaffected by the task.
  PrefetchItem item3 =
      item_generator()->CreateItem(PrefetchItemState::FINISHED);
  EXPECT_TRUE(store_util()->InsertPrefetchItem(item3));
  EXPECT_NE(item3.offline_id, item1.offline_id);
  EXPECT_NE(item3.offline_id, item2.offline_id);

  EXPECT_EQ(3, store_util()->CountPrefetchItems());

  clock.Advance(base::TimeDelta::FromHours(1));

  GeneratePageBundleTask task(dispatcher(), store(), gcm_token(),
                              prefetch_request_factory(),
                              request_callback.Get());
  RunTask(&task);

  // Note: even though the requested URLs checked further below are in undefined
  // order (due to use of std::set) their order of requesting is known: latest
  // creation dates should come first. But as these ids are stored in a
  // std::vector we can rely on the order being correct.
  EXPECT_EQ(1, dispatcher()->generate_page_bundle_requested);
  EXPECT_EQ(2u, dispatcher()->ids_from_generate_page_bundle_requested->size());
  EXPECT_EQ(std::make_pair(item1.offline_id, item1.client_id),
            dispatcher()->ids_from_generate_page_bundle_requested->at(1));
  EXPECT_EQ(std::make_pair(item2.offline_id, item2.client_id),
            dispatcher()->ids_from_generate_page_bundle_requested->at(0));

  std::unique_ptr<std::set<std::string>> requested_urls =
      prefetch_request_factory()->GetAllUrlsRequested();
  EXPECT_EQ(2u, requested_urls->size());
  EXPECT_THAT(*requested_urls, Contains(item1.url.spec()));
  EXPECT_THAT(*requested_urls, Contains(item2.url.spec()));
  EXPECT_THAT(*requested_urls, Not(Contains(item3.url.spec())));

  std::string upload_data =
      network::GetUploadData(GetPendingRequest(0 /*index*/)->request);
  EXPECT_THAT(upload_data, HasSubstr(MockPrefetchItemGenerator::kUrlPrefix));

  EXPECT_EQ(3, store_util()->CountPrefetchItems());

  std::unique_ptr<PrefetchItem> updated_item1 =
      store_util()->GetPrefetchItem(item1.offline_id);
  std::unique_ptr<PrefetchItem> updated_item2 =
      store_util()->GetPrefetchItem(item2.offline_id);
  ASSERT_TRUE(updated_item1);
  ASSERT_TRUE(updated_item2);

  EXPECT_EQ(PrefetchItemState::SENT_GENERATE_PAGE_BUNDLE, updated_item1->state);
  EXPECT_EQ(1, updated_item1->generate_bundle_attempts);
  // Item #1 should have had it's freshness date updated during the task
  // execution.
  EXPECT_EQ(clock.Now(), updated_item1->freshness_time);

  EXPECT_EQ(PrefetchItemState::SENT_GENERATE_PAGE_BUNDLE, updated_item2->state);
  EXPECT_EQ(2, updated_item2->generate_bundle_attempts);
  // As item #2 has already an attempt to GPB it should not have its
  // freshness_time updated.
  EXPECT_EQ(item2.freshness_time, updated_item2->freshness_time);
}

}  // namespace offline_pages
