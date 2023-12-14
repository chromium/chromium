// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <math.h>
#include <map>
#include <memory>
#include <vector>

#include "base/cancelable_callback.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/clock.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/commerce/core/bookmark_update_manager.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/mock_shopping_service.h"
#include "components/commerce/core/pref_names.h"
#include "components/commerce/core/test_utils.h"
#include "components/power_bookmarks/core/power_bookmark_utils.h"
#include "components/power_bookmarks/core/proto/power_bookmark_meta.pb.h"
#include "components/power_bookmarks/core/proto/shopping_specifics.pb.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace commerce {

class BookmarkUpdateManagerTest : public testing::Test {
 public:
  BookmarkUpdateManagerTest()
      : shopping_service_(std::make_unique<MockShoppingService>()),
        bookmark_model_(bookmarks::TestBookmarkClient::CreateModel()),
        pref_service_(std::make_unique<TestingPrefServiceSimple>()) {
    update_manager_ = std::make_unique<BookmarkUpdateManager>(
        shopping_service_.get(), bookmark_model_.get(), pref_service_.get());
  }
  BookmarkUpdateManagerTest(const BookmarkUpdateManagerTest&) = delete;
  BookmarkUpdateManagerTest operator=(const BookmarkUpdateManagerTest&) =
      delete;
  ~BookmarkUpdateManagerTest() override = default;

  void TestBody() override {}

  void SetUp() override {
    // The update manager should not have an update scheduled by default.
    EXPECT_FALSE(IsUpdateScheduled());

    RegisterPrefs(pref_service_->registry());
    pref_service_->SetTime(kShoppingListBookmarkLastUpdateTime, base::Time());
  }

  void TearDown() override { update_manager_->CancelUpdates(); }

  // Generates a large number of product bookmarks and returns them in a list.
  std::vector<const bookmarks::BookmarkNode*> AddProductBookmarks(
      size_t count) {
    std::vector<const bookmarks::BookmarkNode*> bookmarks;
    for (size_t i = 0; i < count; i++) {
      bookmarks.push_back(AddProductBookmark(
          bookmark_model_.get(), u"Title",
          GURL("http://example.com/" + base::NumberToString(i)), i));
    }
    return bookmarks;
  }

  // Get a list of IDs from the provided list of bookmarks (in the same order).
  std::vector<int64_t> GetIdsFromBookmarks(
      const std::vector<const bookmarks::BookmarkNode*>& bookmarks) {
    std::vector<int64_t> ids;
    for (size_t i = 0; i < bookmarks.size(); i++) {
      ids.push_back(bookmarks[i]->id());
    }
    return ids;
  }

  // Creates and returns a map of bookmark ID to fake product info that can be
  // used with the mock shopping service.
  std::map<int64_t, ProductInfo> BuildOnDemandMapForIds(
      const std::vector<int64_t>& ids) {
    std::map<int64_t, ProductInfo> update_map;

    for (int64_t id : ids) {
      ProductInfo info;
      info.title = "Updated title";
      info.product_cluster_id = id;
      update_map.emplace(id, std::move(info));
    }

    return update_map;
  }

  bool IsUpdateScheduled() {
    return update_manager_->scheduled_task_ != nullptr;
  }

  const base::CancelableOnceClosure* GetScheduledTask() {
    return update_manager_->scheduled_task_.get();
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList test_features_;
  std::unique_ptr<MockShoppingService> shopping_service_;
  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;

  std::unique_ptr<BookmarkUpdateManager> update_manager_;
};

// Test that an update is scheduled
TEST_F(BookmarkUpdateManagerTest, UpdateScheduled) {
  test_features_.InitWithFeatures(
      {kShoppingList, kCommerceAllowOnDemandBookmarkUpdates}, {});

  pref_service_->SetTime(kShoppingListBookmarkLastUpdateTime,
                         base::Time::Now());

  update_manager_->ScheduleUpdate();

  EXPECT_TRUE(IsUpdateScheduled());
}

// Test that the kill switch blocks updates.
TEST_F(BookmarkUpdateManagerTest, NoUpdateScheduled_KillSwitch) {
  test_features_.InitWithFeatures({kShoppingList},
                                  {kCommerceAllowOnDemandBookmarkUpdates});

  pref_service_->SetTime(kShoppingListBookmarkLastUpdateTime,
                         base::Time::Now());

  update_manager_->ScheduleUpdate();

  EXPECT_FALSE(IsUpdateScheduled());
}

// Ensure that calling ScheduleUpdate multiple times does not affect the
// previously scheduled update.
TEST_F(BookmarkUpdateManagerTest, UpdateNotDoubleScheduled) {
  test_features_.InitWithFeatures(
      {kShoppingList, kCommerceAllowOnDemandBookmarkUpdates}, {});

  pref_service_->SetTime(kShoppingListBookmarkLastUpdateTime,
                         base::Time::Now());

  update_manager_->ScheduleUpdate();

  const base::CancelableOnceClosure* original = GetScheduledTask();

  update_manager_->ScheduleUpdate();

  const base::CancelableOnceClosure* task = GetScheduledTask();

  EXPECT_TRUE(IsUpdateScheduled());
  EXPECT_EQ(original, task);
}

TEST_F(BookmarkUpdateManagerTest, RunScheduledTask) {
  test_features_.InitWithFeatures(
      {kShoppingList, kCommerceAllowOnDemandBookmarkUpdates}, {});

  shopping_service_->SetIsShoppingListEligible(true);

  const int64_t cluster_id = 123L;
  const bookmarks::BookmarkNode* bookmark = AddProductBookmark(
      bookmark_model_.get(), u"Title", GURL("http://example.com"), cluster_id);

  ProductInfo new_info;
  const std::string updated_title = "Updated Title";
  new_info.title = updated_title;
  new_info.product_cluster_id = cluster_id;

  std::map<int64_t, ProductInfo> info_map;
  info_map[bookmark->id()] = new_info;
  shopping_service_->SetResponsesForGetUpdatedProductInfoForBookmarks(
      std::move(info_map));

  shopping_service_->SetGetAllShoppingBookmarksValue({bookmark});

  update_manager_->ScheduleUpdate();
  task_environment_.FastForwardBy(base::Days(1));
  base::RunLoop().RunUntilIdle();

  auto meta = power_bookmarks::GetNodePowerBookmarkMeta(bookmark_model_.get(),
                                                        bookmark);

  EXPECT_EQ(meta->shopping_specifics().title(), updated_title);

  // Ensure the preference for last updated time was also set.
  base::TimeDelta time_since_last =
      base::Time::Now() -
      pref_service_->GetTime(kShoppingListBookmarkLastUpdateTime);
  EXPECT_TRUE(time_since_last < base::Minutes(1));
}

TEST_F(BookmarkUpdateManagerTest, RunScheduledTask_BlockedByFeatureCheck) {
  test_features_.InitWithFeatures(
      {kShoppingList, kCommerceAllowOnDemandBookmarkUpdates}, {});
  shopping_service_->SetIsShoppingListEligible(false);

  const std::string title = "Title";
  const int64_t cluster_id = 123L;
  const bookmarks::BookmarkNode* bookmark = AddProductBookmark(
      bookmark_model_.get(), u"Title", GURL("http://example.com"), cluster_id);

  ProductInfo new_info;
  new_info.title = "Updated Title";
  new_info.product_cluster_id = cluster_id;

  std::map<int64_t, ProductInfo> info_map;
  info_map[bookmark->id()] = new_info;
  shopping_service_->SetResponsesForGetUpdatedProductInfoForBookmarks(
      std::move(info_map));

  shopping_service_->SetGetAllShoppingBookmarksValue({bookmark});

  update_manager_->ScheduleUpdate();
  task_environment_.FastForwardBy(base::Hours(6));
  base::RunLoop().RunUntilIdle();

  auto meta = power_bookmarks::GetNodePowerBookmarkMeta(bookmark_model_.get(),
                                                        bookmark);

  // The bookmark should not have been updated.
  EXPECT_EQ(meta->shopping_specifics().title(), title);

  // Even though the update was blocked, we're still scheduling the noop task.
  // Make sure the previous time was recorded (recorded time is not default).
  EXPECT_TRUE(pref_service_->GetTime(kShoppingListBookmarkLastUpdateTime) !=
              base::Time());
}

// Ensure that updates are handled in batches that the backend can handle. For
// example: if the backend can only handle 30 updates per call, make sure two
// batches are sent.
TEST_F(BookmarkUpdateManagerTest, RunBatchedUpdate) {
  test_features_.InitWithFeatures(
      {kShoppingList, kCommerceAllowOnDemandBookmarkUpdates,
       kCommerceAllowOnDemandBookmarkBatchUpdates},
      {});

  shopping_service_->SetIsShoppingListEligible(true);
  ON_CALL(*shopping_service_, GetMaxProductBookmarkUpdatesPerBatch)
      .WillByDefault(testing::Return(30));

  const size_t bookmark_count = 50;
  ASSERT_LT(shopping_service_->GetMaxProductBookmarkUpdatesPerBatch(),
            bookmark_count);

  const size_t expected_update_calls =
      ceil(static_cast<float>(bookmark_count) /
           shopping_service_->GetMaxProductBookmarkUpdatesPerBatch());
  std::vector<const bookmarks::BookmarkNode*> bookmarks =
      AddProductBookmarks(bookmark_count);
  std::vector<int64_t> ids = GetIdsFromBookmarks(bookmarks);
  shopping_service_->SetGetAllShoppingBookmarksValue(bookmarks);
  std::map<int64_t, ProductInfo> info_map = BuildOnDemandMapForIds(ids);

  shopping_service_->SetResponsesForGetUpdatedProductInfoForBookmarks(info_map);

  // With 50 items and a max batch size of 30, we expect two calls to the
  // shopping service api.
  EXPECT_CALL(*shopping_service_,
              GetUpdatedProductInfoForBookmarks(testing::_, testing::_))
      .Times(expected_update_calls);

  update_manager_->ScheduleUpdate();
  base::RunLoop().RunUntilIdle();

  // Ensure the preference for last updated time was also set.
  base::TimeDelta time_since_last =
      base::Time::Now() -
      pref_service_->GetTime(kShoppingListBookmarkLastUpdateTime);
  EXPECT_TRUE(time_since_last < base::Minutes(1));
}

// Same as the above test, but ensures that if a user has more than the max
// allowed updatable bookmarks, we stop updating.
TEST_F(BookmarkUpdateManagerTest, RunBatchedUpdate_OverMaxAllowed) {
  test_features_.InitWithFeatures(
      {kShoppingList, kCommerceAllowOnDemandBookmarkUpdates,
       kCommerceAllowOnDemandBookmarkBatchUpdates},
      {});

  shopping_service_->SetIsShoppingListEligible(true);
  ON_CALL(*shopping_service_, GetMaxProductBookmarkUpdatesPerBatch)
      .WillByDefault(testing::Return(10));

  const size_t bookmark_count =
      kShoppingListBookmarkpdateBatchMaxParam.Get() + 10;
  const size_t expected_update_calls =
      ceil(static_cast<float>(kShoppingListBookmarkpdateBatchMaxParam.Get()) /
           shopping_service_->GetMaxProductBookmarkUpdatesPerBatch());
  const size_t ungated_update_calls =
      ceil(static_cast<float>(bookmark_count) /
           shopping_service_->GetMaxProductBookmarkUpdatesPerBatch());
  ASSERT_GT(ungated_update_calls, expected_update_calls);

  std::vector<const bookmarks::BookmarkNode*> bookmarks =
      AddProductBookmarks(bookmark_count);
  std::vector<int64_t> ids = GetIdsFromBookmarks(bookmarks);
  shopping_service_->SetGetAllShoppingBookmarksValue(bookmarks);
  std::map<int64_t, ProductInfo> info_map = BuildOnDemandMapForIds(ids);

  shopping_service_->SetResponsesForGetUpdatedProductInfoForBookmarks(info_map);

  EXPECT_CALL(*shopping_service_,
              GetUpdatedProductInfoForBookmarks(testing::_, testing::_))
      .Times(expected_update_calls);

  update_manager_->ScheduleUpdate();
  base::RunLoop().RunUntilIdle();

  // Ensure the preference for last updated time was also set.
  base::TimeDelta time_since_last =
      base::Time::Now() -
      pref_service_->GetTime(kShoppingListBookmarkLastUpdateTime);
  EXPECT_TRUE(time_since_last < base::Minutes(1));
}

TEST_F(BookmarkUpdateManagerTest, RunBatchedUpdate_BatchingDisabled) {
  test_features_.InitWithFeatures(
      {kShoppingList, kCommerceAllowOnDemandBookmarkUpdates},
      {kCommerceAllowOnDemandBookmarkBatchUpdates});

  shopping_service_->SetIsShoppingListEligible(true);
  ON_CALL(*shopping_service_, GetMaxProductBookmarkUpdatesPerBatch)
      .WillByDefault(testing::Return(10));

  const size_t bookmark_count = 50;
  ASSERT_LT(shopping_service_->GetMaxProductBookmarkUpdatesPerBatch(),
            bookmark_count);

  std::vector<const bookmarks::BookmarkNode*> bookmarks =
      AddProductBookmarks(bookmark_count);
  std::vector<int64_t> ids = GetIdsFromBookmarks(bookmarks);
  shopping_service_->SetGetAllShoppingBookmarksValue(bookmarks);
  std::map<int64_t, ProductInfo> info_map = BuildOnDemandMapForIds(ids);

  shopping_service_->SetResponsesForGetUpdatedProductInfoForBookmarks(info_map);

  // Even though the user has more than one batch of bookmarks to request
  // updates for, we should only do one since the flag is disabled.
  EXPECT_CALL(*shopping_service_,
              GetUpdatedProductInfoForBookmarks(testing::_, testing::_))
      .Times(1);

  update_manager_->ScheduleUpdate();
  base::RunLoop().RunUntilIdle();

  // Ensure the preference for last updated time was also set.
  base::TimeDelta time_since_last =
      base::Time::Now() -
      pref_service_->GetTime(kShoppingListBookmarkLastUpdateTime);
  EXPECT_TRUE(time_since_last < base::Minutes(1));
}

}  // namespace commerce
