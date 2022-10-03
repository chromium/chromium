// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>

#include "base/bind.h"
#include "base/cancelable_callback.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
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
  }

  void TearDown() override { update_manager_->CancelUpdates(); }

  bool IsUpdateScheduled() {
    return update_manager_->scheduled_task_ != nullptr;
  }

  const base::CancelableOnceClosure* GetScheduledTask() {
    return update_manager_->scheduled_task_.get();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
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

  // Set this up so the task runs immediately (last update was a year ago).
  pref_service_->SetTime(kShoppingListBookmarkLastUpdateTime,
                         base::Time::Now() - base::Days(365));

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

  update_manager_->ScheduleUpdate();
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

}  // namespace commerce
