// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/callback.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/commerce/core/mock_shopping_service.h"
#include "components/commerce/core/price_tracking_utils.h"
#include "components/power_bookmarks/core/power_bookmark_utils.h"
#include "components/power_bookmarks/core/proto/power_bookmark_meta.pb.h"
#include "components/power_bookmarks/core/proto/shopping_specifics.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace commerce {
namespace {

class PriceTrackingUtilsTest : public testing::Test {
 protected:
  void SetUp() override {
    bookmark_model_ = bookmarks::TestBookmarkClient::CreateModel();
    shopping_service_ = std::make_unique<MockShoppingService>();
  }

  // Create a product bookmark with the specified cluster ID and place it in the
  // "other" bookmarks folder.
  const bookmarks::BookmarkNode* AddProductBookmark(const std::u16string& title,
                                                    const GURL& url,
                                                    uint64_t cluster_id,
                                                    bool is_price_tracked) {
    const bookmarks::BookmarkNode* node =
        bookmark_model_->AddURL(bookmark_model_->other_node(), 0, title, url);
    std::unique_ptr<power_bookmarks::PowerBookmarkMeta> meta =
        std::make_unique<power_bookmarks::PowerBookmarkMeta>();
    meta->set_type(power_bookmarks::PowerBookmarkType::SHOPPING);
    power_bookmarks::ShoppingSpecifics* specifics =
        meta->mutable_shopping_specifics();
    specifics->set_product_cluster_id(cluster_id);
    specifics->set_is_price_tracked(is_price_tracked);

    power_bookmarks::SetNodePowerBookmarkMeta(bookmark_model_.get(), node,
                                              std::move(meta));
    return node;
  }

  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model_;
  std::unique_ptr<MockShoppingService> shopping_service_;

  base::test::TaskEnvironment task_environment_;
};

// Test that the utility for setting the price tracking state of a bookmark
// updates all of the bookmarks with the same cluster ID if the subscription
// backend call is successful.
TEST_F(PriceTrackingUtilsTest,
       SetPriceTrackingStateUpdatesAll_UnsubscribeSuccess) {
  const uint64_t cluster_id = 12345L;
  const bookmarks::BookmarkNode* product1 = AddProductBookmark(
      u"product 1", GURL("http://example.com/1"), cluster_id, true);
  const bookmarks::BookmarkNode* product2 = AddProductBookmark(
      u"product 2", GURL("http://example.com/2"), cluster_id, true);

  // Simulate successful calls in the subscriptions manager.
  shopping_service_->SetSubscribeCallbackValue(true);
  shopping_service_->SetUnsubscribeCallbackValue(true);

  base::RunLoop run_loop;
  SetPriceTrackingStateForBookmark(
      shopping_service_.get(), bookmark_model_.get(), product1, false,
      base::BindOnce(
          [](base::RunLoop* run_loop, bool success) {
            EXPECT_TRUE(success);
            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();

  EXPECT_FALSE(IsBookmarkPriceTracked(bookmark_model_.get(), product1));
  EXPECT_FALSE(IsBookmarkPriceTracked(bookmark_model_.get(), product2));
}

// Same as the _SubscriptionSuccess version but the subscription fails on the
// backend. In this case, the bookmarks should not be updated.
TEST_F(PriceTrackingUtilsTest,
       SetPriceTrackingStateUpdatesAll_UnsubscribeFailed) {
  const uint64_t cluster_id = 12345L;
  const bookmarks::BookmarkNode* product1 = AddProductBookmark(
      u"product 1", GURL("http://example.com/1"), cluster_id, true);
  const bookmarks::BookmarkNode* product2 = AddProductBookmark(
      u"product 2", GURL("http://example.com/2"), cluster_id, true);

  // Simulate failed calls in the subscriptions manager.
  shopping_service_->SetSubscribeCallbackValue(false);
  shopping_service_->SetUnsubscribeCallbackValue(false);

  base::RunLoop run_loop;
  SetPriceTrackingStateForBookmark(
      shopping_service_.get(), bookmark_model_.get(), product1, false,
      base::BindOnce(
          [](base::RunLoop* run_loop, bool success) {
            EXPECT_FALSE(success);
            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();

  EXPECT_TRUE(IsBookmarkPriceTracked(bookmark_model_.get(), product1));
  EXPECT_TRUE(IsBookmarkPriceTracked(bookmark_model_.get(), product2));
}

TEST_F(PriceTrackingUtilsTest, GetBookmarksWithClusterId) {
  const uint64_t cluster_id = 12345L;
  AddProductBookmark(u"product 1", GURL("http://example.com/1"), cluster_id,
                     true);
  AddProductBookmark(u"product 2", GURL("http://example.com/2"), cluster_id,
                     true);
  bookmark_model_->AddURL(bookmark_model_->other_node(), 0, u"non-product",
                          GURL("http://www.example.com"));

  ASSERT_EQ(3U, bookmark_model_->other_node()->children().size());
  ASSERT_EQ(
      2U, GetBookmarksWithClusterId(bookmark_model_.get(), cluster_id).size());
}

TEST_F(PriceTrackingUtilsTest, GetBookmarksWithClusterId_NoProducts) {
  const uint64_t cluster_id = 12345L;
  bookmark_model_->AddURL(bookmark_model_->other_node(), 0, u"non-product",
                          GURL("http://www.example.com"));

  ASSERT_EQ(1U, bookmark_model_->other_node()->children().size());
  ASSERT_EQ(
      0U, GetBookmarksWithClusterId(bookmark_model_.get(), cluster_id).size());
}

TEST_F(PriceTrackingUtilsTest, IsBookmarkPriceTracked_Tracked) {
  const bookmarks::BookmarkNode* product = AddProductBookmark(
      u"product 1", GURL("http://example.com/1"), 12345L, true);

  EXPECT_TRUE(IsBookmarkPriceTracked(bookmark_model_.get(), product));
}

TEST_F(PriceTrackingUtilsTest, IsBookmarkPriceTracked_NotTracked) {
  const bookmarks::BookmarkNode* product = AddProductBookmark(
      u"product 1", GURL("http://example.com/1"), 12345L, false);

  EXPECT_FALSE(IsBookmarkPriceTracked(bookmark_model_.get(), product));
}

TEST_F(PriceTrackingUtilsTest, IsBookmarkPriceTracked_NonProduct) {
  const bookmarks::BookmarkNode* normal_bookmark =
      bookmark_model_->AddURL(bookmark_model_->other_node(), 0, u"non-product",
                              GURL("http://www.example.com"));

  EXPECT_FALSE(IsBookmarkPriceTracked(bookmark_model_.get(), normal_bookmark));
}

}  // namespace
}  // namespace commerce