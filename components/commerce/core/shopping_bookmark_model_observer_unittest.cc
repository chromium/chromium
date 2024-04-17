// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/common/bookmark_metrics.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/mock_shopping_service.h"
#include "components/commerce/core/price_tracking_utils.h"
#include "components/commerce/core/shopping_bookmark_model_observer.h"
#include "components/commerce/core/subscriptions/mock_subscriptions_manager.h"
#include "components/commerce/core/test_utils.h"
#include "components/power_bookmarks/core/power_bookmark_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace commerce {
namespace {

class ShoppingBookmarkModelObserverTest : public testing::Test {
 protected:
  void SetUp() override {
    auto client = std::make_unique<bookmarks::TestBookmarkClient>();
    client->SetIsSyncFeatureEnabledIncludingBookmarks(true);
    bookmark_model_ =
        bookmarks::TestBookmarkClient::CreateModelWithClient(std::move(client));
    shopping_service_ = std::make_unique<MockShoppingService>();
    subscriptions_manager_ = std::make_unique<MockSubscriptionsManager>();

    observer_ = std::make_unique<ShoppingBookmarkModelObserver>(
        bookmark_model_.get(), shopping_service_.get(),
        subscriptions_manager_.get());
  }

  void TearDown() override {
    // Ensure the observer is destroyed prior to any of its dependencies.
    observer_.reset();
  }

  base::test::ScopedFeatureList test_features_;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<ShoppingBookmarkModelObserver> observer_;
  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model_;
  std::unique_ptr<MockShoppingService> shopping_service_;
  std::unique_ptr<MockSubscriptionsManager> subscriptions_manager_;
};

// Ensure a subscription is removed if the owning bookmark is deleted and the
// relationship is 1:1.
TEST_F(ShoppingBookmarkModelObserverTest, TestUnsubscribeOnBookmarkDeletion) {
  uint64_t cluster_id = 12345L;

  const bookmarks::BookmarkNode* node = AddProductBookmark(
      bookmark_model_.get(), u"title", GURL("https://example.com"), cluster_id);
  shopping_service_->SetIsSubscribedCallbackValue(true);

  EXPECT_CALL(
      *shopping_service_,
      Unsubscribe(VectorHasSubscriptionWithId(base::NumberToString(cluster_id)),
                  testing::_))
      .Times(1);
  bookmark_model_->Remove(node, bookmarks::metrics::BookmarkEditSource::kOther,
                          FROM_HERE);
  base::RunLoop().RunUntilIdle();
}

// If there are multiple bookmarks with the same product ID, don't remove the
// subscription.
TEST_F(ShoppingBookmarkModelObserverTest,
       TestUnsubscribeOnBookmarkDeletion_MultipleBookmarks) {
  uint64_t cluster_id = 12345L;

  const bookmarks::BookmarkNode* node = AddProductBookmark(
      bookmark_model_.get(), u"title", GURL("https://example.com"), cluster_id);
  AddProductBookmark(bookmark_model_.get(), u"title 2",
                     GURL("https://example.com/2"), cluster_id);
  shopping_service_->SetIsSubscribedCallbackValue(true);

  EXPECT_CALL(*shopping_service_, Unsubscribe(testing::_, testing::_)).Times(0);
  bookmark_model_->Remove(node, bookmarks::metrics::BookmarkEditSource::kOther,
                          FROM_HERE);
  base::RunLoop().RunUntilIdle();
}

// Make sure that when a folder is deleted, the products that it contains are
// unsubscribed.
TEST_F(ShoppingBookmarkModelObserverTest,
       TestUnsubscribeOnBookmarkFolderDeletion) {
  uint64_t cluster_id = 12345L;

  const bookmarks::BookmarkNode* folder = bookmark_model_->AddFolder(
      bookmark_model_->other_node(),
      bookmark_model_->other_node()->children().size(), u"folder");

  bookmark_model_->Move(
      AddProductBookmark(bookmark_model_.get(), u"title 1",
                         GURL("https://example.com/1"), cluster_id),
      folder, folder->children().size());
  shopping_service_->SetIsSubscribedCallbackValue(true);

  EXPECT_CALL(*shopping_service_, Unsubscribe(testing::_, testing::_)).Times(1);
  bookmark_model_->Remove(
      folder, bookmarks::metrics::BookmarkEditSource::kOther, FROM_HERE);
  base::RunLoop().RunUntilIdle();
}

// Make sure that when a folder is deleted and contains duplicate products in
// the subtree, it is correctly unsubscribed.
TEST_F(ShoppingBookmarkModelObserverTest,
       TestUnsubscribeOnBookmarkFolderDeletion_SameProduct_SameFolder) {
  uint64_t cluster_id = 12345L;

  const bookmarks::BookmarkNode* folder = bookmark_model_->AddFolder(
      bookmark_model_->other_node(),
      bookmark_model_->other_node()->children().size(), u"folder");

  bookmark_model_->Move(
      AddProductBookmark(bookmark_model_.get(), u"title 1",
                         GURL("https://example.com/1"), cluster_id),
      folder, folder->children().size());
  bookmark_model_->Move(
      AddProductBookmark(bookmark_model_.get(), u"title 2",
                         GURL("https://example.com/2"), cluster_id),
      folder, folder->children().size());
  shopping_service_->SetIsSubscribedCallbackValue(true);

  EXPECT_CALL(*shopping_service_, Unsubscribe(testing::_, testing::_)).Times(1);
  bookmark_model_->Remove(
      folder, bookmarks::metrics::BookmarkEditSource::kOther, FROM_HERE);
  base::RunLoop().RunUntilIdle();
}

// If there are duplicate products but they exist in different subtrees, make
// sure the product remains subscribed.
TEST_F(ShoppingBookmarkModelObserverTest,
       TestUnsubscribeOnBookmarkFolderDeletion_SameProduct_DifferentFolder) {
  uint64_t cluster_id = 12345L;

  const bookmarks::BookmarkNode* folder1 = bookmark_model_->AddFolder(
      bookmark_model_->other_node(),
      bookmark_model_->other_node()->children().size(), u"folder 1");
  const bookmarks::BookmarkNode* folder2 = bookmark_model_->AddFolder(
      bookmark_model_->other_node(),
      bookmark_model_->other_node()->children().size(), u"folder 2");

  bookmark_model_->Move(
      AddProductBookmark(bookmark_model_.get(), u"title 1",
                         GURL("https://example.com/1"), cluster_id),
      folder1, folder1->children().size());
  bookmark_model_->Move(
      AddProductBookmark(bookmark_model_.get(), u"title 2",
                         GURL("https://example.com/2"), cluster_id),
      folder2, folder2->children().size());
  shopping_service_->SetIsSubscribedCallbackValue(true);

  EXPECT_CALL(*shopping_service_, Unsubscribe(testing::_, testing::_)).Times(0);
  bookmark_model_->Remove(
      folder1, bookmarks::metrics::BookmarkEditSource::kOther, FROM_HERE);
  base::RunLoop().RunUntilIdle();
}

// If the URL of a bookmark changes, we don't know if it still points to a valid
// product. The subscription and meta should be removed.
TEST_F(ShoppingBookmarkModelObserverTest,
       TestMetaRemovalAndUnsubscribeOnURLChange) {
  uint64_t cluster_id = 12345L;

  const bookmarks::BookmarkNode* node = AddProductBookmark(
      bookmark_model_.get(), u"title", GURL("https://example.com"), cluster_id);
  shopping_service_->SetIsSubscribedCallbackValue(true);

  EXPECT_CALL(
      *shopping_service_,
      Unsubscribe(VectorHasSubscriptionWithId(base::NumberToString(cluster_id)),
                  testing::_))
      .Times(1);
  bookmark_model_->SetURL(node, GURL("https://example.com/different"),
                          bookmarks::metrics::BookmarkEditSource::kUser);
  base::RunLoop().RunUntilIdle();

  // The meta for the bookmark should have also been removed.
  ASSERT_FALSE(
      power_bookmarks::GetNodePowerBookmarkMeta(bookmark_model_.get(), node)
          ->has_shopping_specifics());
}

// Same test as above, but we shouldn't unsubscribe if there were multiple
// product bookmarks with the same cluster ID.
TEST_F(ShoppingBookmarkModelObserverTest,
       TestMetaRemovalAndUnsubscribeOnURLChange_MultipleBookmarks) {
  uint64_t cluster_id = 12345L;

  const bookmarks::BookmarkNode* node = AddProductBookmark(
      bookmark_model_.get(), u"title", GURL("https://example.com"), cluster_id);
  AddProductBookmark(bookmark_model_.get(), u"title 2",
                     GURL("https://example.com/2"), cluster_id);
  shopping_service_->SetIsSubscribedCallbackValue(true);

  EXPECT_CALL(*shopping_service_, Unsubscribe(testing::_, testing::_)).Times(0);
  bookmark_model_->SetURL(node, GURL("https://example.com/different"),
                          bookmarks::metrics::BookmarkEditSource::kUser);
  base::RunLoop().RunUntilIdle();

  // The meta for the bookmark should have also been removed.
  ASSERT_FALSE(
      power_bookmarks::GetNodePowerBookmarkMeta(bookmark_model_.get(), node)
          ->has_shopping_specifics());
}

// Ensure a subscription is automatically tracked if that flag is enabled.
TEST_F(ShoppingBookmarkModelObserverTest, TestAutomaticTrackingOnAdd) {
  test_features_.InitAndEnableFeature(kTrackByDefaultOnMobile);

  uint64_t cluster_id = 12345L;
  ProductInfo info;
  info.product_cluster_id.emplace(cluster_id);

  shopping_service_->SetResponseForGetProductInfoForUrl(info);

  EXPECT_CALL(
      *shopping_service_,
      Subscribe(VectorHasSubscriptionWithId(base::NumberToString(cluster_id)),
                testing::_))
      .Times(1);

  AddProductBookmark(bookmark_model_.get(), u"title",
                     GURL("https://example.com"), cluster_id);

  base::RunLoop().RunUntilIdle();
}

// Ensure a subscription is automatically tracked if that flag is enabled.
TEST_F(ShoppingBookmarkModelObserverTest, TestShoppingCollectionChangeMetrics) {
  base::UserActionTester user_action_tester;

  ASSERT_EQ(user_action_tester.GetActionCount(
                "Commerce.PriceTracking.ShoppingCollection.Created"),
            0);
  ASSERT_EQ(user_action_tester.GetActionCount(
                "Commerce.PriceTracking.ShoppingCollection.Deleted"),
            0);
  ASSERT_EQ(user_action_tester.GetActionCount(
                "Commerce.PriceTracking.ShoppingCollection.ParentChanged"),
            0);
  ASSERT_EQ(user_action_tester.GetActionCount(
                "Commerce.PriceTracking.ShoppingCollection.NameChanged"),
            0);

  const bookmarks::BookmarkNode* collection =
      GetShoppingCollectionBookmarkFolder(bookmark_model_.get(),
                                          /* create_if_needed = */ true);

  ASSERT_EQ(user_action_tester.GetActionCount(
                "Commerce.PriceTracking.ShoppingCollection.Created"),
            1);

  bookmark_model_->SetTitle(collection, u"new name",
                            bookmarks::metrics::BookmarkEditSource::kUser);

  ASSERT_EQ(user_action_tester.GetActionCount(
                "Commerce.PriceTracking.ShoppingCollection.NameChanged"),
            1);

  const bookmarks::BookmarkNode* subfolder = bookmark_model_->AddFolder(
      bookmark_model_->other_node(),
      bookmark_model_->other_node()->children().size() - 1, u"subfolder");

  bookmark_model_->Move(collection, subfolder, 0);

  ASSERT_EQ(user_action_tester.GetActionCount(
                "Commerce.PriceTracking.ShoppingCollection.ParentChanged"),
            1);

  bookmark_model_->Remove(
      collection, bookmarks::metrics::BookmarkEditSource::kUser, FROM_HERE);

  ASSERT_EQ(user_action_tester.GetActionCount(
                "Commerce.PriceTracking.ShoppingCollection.Deleted"),
            1);
}

}  // namespace
}  // namespace commerce
