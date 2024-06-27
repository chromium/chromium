// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/price_tracking_utils.h"

#include <memory>
#include <optional>

#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_uuids.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/mock_shopping_service.h"
#include "components/commerce/core/pref_names.h"
#include "components/commerce/core/subscriptions/commerce_subscription.h"
#include "components/commerce/core/test_utils.h"
#include "components/power_bookmarks/core/power_bookmark_utils.h"
#include "components/power_bookmarks/core/proto/power_bookmark_meta.pb.h"
#include "components/power_bookmarks/core/proto/shopping_specifics.pb.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace commerce {
namespace {

class PriceTrackingUtilsTest : public testing::Test {
 protected:
  void SetUp() override {
    auto client = std::make_unique<bookmarks::TestBookmarkClient>();
    client->SetIsSyncFeatureEnabledIncludingBookmarks(true);
    bookmark_model_ =
        bookmarks::TestBookmarkClient::CreateModelWithClient(std::move(client));
    shopping_service_ = std::make_unique<MockShoppingService>();
    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    RegisterPrefs(pref_service_->registry());
  }

  base::test::ScopedFeatureList test_features_;
  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model_;
  std::unique_ptr<MockShoppingService> shopping_service_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  base::test::TaskEnvironment task_environment_;
};

// Test that the utility for setting the price tracking state of a bookmark
// updates all of the bookmarks with the same cluster ID if the subscription
// backend call is successful.
TEST_F(PriceTrackingUtilsTest,
       SetPriceTrackingStateUpdatesAll_UnsubscribeSuccess) {
  const uint64_t cluster_id = 12345L;
  const int64_t last_change_time = 100L;
  const bookmarks::BookmarkNode* product1 =
      AddProductBookmark(bookmark_model_.get(), u"product 1",
                         GURL("http://example.com/1"), cluster_id, true);
  const bookmarks::BookmarkNode* product2 =
      AddProductBookmark(bookmark_model_.get(), u"product 2",
                         GURL("http://example.com/2"), cluster_id, true, 0L,
                         "usd", std::make_optional<int64_t>(last_change_time));
  ASSERT_EQ(std::nullopt, GetBookmarkLastSubscriptionChangeTime(
                              bookmark_model_.get(), product1));
  ASSERT_EQ(last_change_time, GetBookmarkLastSubscriptionChangeTime(
                                  bookmark_model_.get(), product2)
                                  .value());

  // Simulate successful calls in the subscriptions manager.
  shopping_service_->SetSubscribeCallbackValue(true);
  shopping_service_->SetUnsubscribeCallbackValue(true);

  EXPECT_CALL(
      *shopping_service_,
      Unsubscribe(VectorHasSubscriptionWithId(base::NumberToString(cluster_id)),
                  testing::_))
      .Times(1);

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

  ASSERT_GT(
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds(),
      GetBookmarkLastSubscriptionChangeTime(bookmark_model_.get(), product1)
          .value());
  ASSERT_NE(last_change_time, GetBookmarkLastSubscriptionChangeTime(
                                  bookmark_model_.get(), product2)
                                  .value());
}

// Ensure bookmarks created by price tracking are kept when the product is
// unsubscribed if the "track by default" feature is enabled.
TEST_F(PriceTrackingUtilsTest,
       SetPriceTrackingState_Unsubscribe_TrackByDefault) {
  const bookmarks::BookmarkNode* product =
      AddProductBookmark(bookmark_model_.get(), u"product 1",
                         GURL("http://example.com/1"), 12345L, true);

  EXPECT_EQ(1U, bookmark_model_->other_node()->children().size());

  // Simulate successful calls in the subscriptions manager.
  shopping_service_->SetSubscribeCallbackValue(true);
  shopping_service_->SetUnsubscribeCallbackValue(true);

  base::RunLoop run_loop;
  SetPriceTrackingStateForBookmark(
      shopping_service_.get(), bookmark_model_.get(), product, true,
      base::BindOnce([](bool success) {}).Then(run_loop.QuitClosure()), true);
  run_loop.Run();

  EXPECT_EQ(1U, bookmark_model_->other_node()->children().size());

  base::RunLoop run_loop2;
  SetPriceTrackingStateForBookmark(
      shopping_service_.get(), bookmark_model_.get(), product, false,
      base::BindOnce([](bool success) {}).Then(run_loop2.QuitClosure()));
  run_loop2.Run();

  // The bookmark should not have been deleted.
  EXPECT_EQ(1U, bookmark_model_->other_node()->children().size());
}

// Test that a bookmark is updated in-place if revisiting the page and it is
// detected to be a trackable product.
TEST_F(PriceTrackingUtilsTest, SetPriceTrackingState_SubscribeOldBookmark) {
  const uint64_t cluster_id = 12345L;

  // This bookmark is intentionally a non-product bookmark to start with.
  const bookmarks::BookmarkNode* existing_bookmark = bookmark_model_->AddURL(
      bookmark_model_->other_node(), 0, u"Title", GURL("https://example.com"));

  // Since bookmarking, the shopping service detected that the bookmark is
  // actually a product.
  std::optional<ProductInfo> info;
  info.emplace();
  info->product_cluster_id = cluster_id;
  shopping_service_->SetResponseForGetProductInfoForUrl(std::move(info));

  // Simulate successful calls in the subscriptions manager.
  shopping_service_->SetSubscribeCallbackValue(true);
  shopping_service_->SetUnsubscribeCallbackValue(true);

  EXPECT_CALL(
      *shopping_service_,
      Subscribe(VectorHasSubscriptionWithId(base::NumberToString(cluster_id)),
                testing::_))
      .Times(1);

  base::RunLoop run_loop;
  SetPriceTrackingStateForBookmark(shopping_service_.get(),
                                   bookmark_model_.get(), existing_bookmark,
                                   true, base::BindOnce([](bool success) {
                                           EXPECT_TRUE(success);
                                         }).Then(run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_EQ(GetBookmarksWithClusterId(bookmark_model_.get(), cluster_id)[0],
            existing_bookmark);
}

TEST_F(PriceTrackingUtilsTest, SetPriceTrackingState_ErrorCases_NoService) {
  // This bookmark is intentionally a non-product bookmark to start with.
  const bookmarks::BookmarkNode* existing_bookmark = bookmark_model_->AddURL(
      bookmark_model_->other_node(), 0, u"Title", GURL("https://example.com"));

  EXPECT_CALL(*shopping_service_, Subscribe(testing::_, testing::_)).Times(0);

  base::RunLoop run_loop;
  SetPriceTrackingStateForBookmark(nullptr, bookmark_model_.get(),
                                   existing_bookmark, true,
                                   base::BindOnce([](bool success) {
                                     EXPECT_FALSE(success);
                                   }).Then(run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(PriceTrackingUtilsTest, SetPriceTrackingState_ErrorCases_NoBookmark) {
  const uint64_t cluster_id = 12345L;

  // Since bookmarking, the shopping service detected that the bookmark is
  // actually a product.
  std::optional<ProductInfo> info;
  info.emplace();
  info->product_cluster_id = cluster_id;
  shopping_service_->SetResponseForGetProductInfoForUrl(std::move(info));

  // Simulate successful calls in the subscriptions manager.
  shopping_service_->SetSubscribeCallbackValue(true);
  shopping_service_->SetUnsubscribeCallbackValue(true);

  EXPECT_CALL(*shopping_service_, Subscribe(testing::_, testing::_)).Times(0);

  base::RunLoop run_loop;
  SetPriceTrackingStateForBookmark(nullptr, bookmark_model_.get(), nullptr,
                                   true, base::BindOnce([](bool success) {
                                           EXPECT_FALSE(success);
                                         }).Then(run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(PriceTrackingUtilsTest, SetPriceTrackingState_ErrorCases_NoInfo) {
  // This bookmark is intentionally a non-product bookmark to start with.
  const bookmarks::BookmarkNode* existing_bookmark = bookmark_model_->AddURL(
      bookmark_model_->other_node(), 0, u"Title", GURL("https://example.com"));

  shopping_service_->SetResponseForGetProductInfoForUrl(std::nullopt);

  // Simulate successful calls in the subscriptions manager.
  shopping_service_->SetSubscribeCallbackValue(true);
  shopping_service_->SetUnsubscribeCallbackValue(true);

  EXPECT_CALL(*shopping_service_, Subscribe(testing::_, testing::_)).Times(0);

  base::RunLoop run_loop;
  SetPriceTrackingStateForBookmark(shopping_service_.get(),
                                   bookmark_model_.get(), existing_bookmark,
                                   true, base::BindOnce([](bool success) {
                                           EXPECT_FALSE(success);
                                         }).Then(run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(PriceTrackingUtilsTest, SetPriceTrackingForClusterId) {
  const uint64_t cluster_id = 12345L;

  const bookmarks::BookmarkNode* product =
      AddProductBookmark(bookmark_model_.get(), u"product 1",
                         GURL("http://example.com/1"), cluster_id, true);

  // Simulate successful calls in the subscriptions manager.
  shopping_service_->SetSubscribeCallbackValue(true);
  shopping_service_->SetUnsubscribeCallbackValue(true);

  EXPECT_CALL(
      *shopping_service_,
      Subscribe(VectorHasSubscriptionWithId(base::NumberToString(cluster_id)),
                testing::_))
      .Times(1);

  base::RunLoop run_loop;
  SetPriceTrackingStateForClusterId(
      shopping_service_.get(), bookmark_model_.get(), cluster_id, true,
      base::BindOnce(
          [](base::RunLoop* run_loop, bool success) {
            EXPECT_TRUE(success);
            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();

  EXPECT_EQ(GetBookmarksWithClusterId(bookmark_model_.get(), cluster_id)[0],
            product);
}

TEST_F(PriceTrackingUtilsTest, GetBookmarksWithClusterId) {
  const uint64_t cluster_id = 12345L;
  AddProductBookmark(bookmark_model_.get(), u"product 1",
                     GURL("http://example.com/1"), cluster_id, true);
  AddProductBookmark(bookmark_model_.get(), u"product 2",
                     GURL("http://example.com/2"), cluster_id, true);
  bookmark_model_->AddURL(bookmark_model_->other_node(), 0, u"non-product",
                          GURL("http://www.example.com"));

  ASSERT_EQ(3U, bookmark_model_->other_node()->children().size());
  ASSERT_EQ(
      2U, GetBookmarksWithClusterId(bookmark_model_.get(), cluster_id).size());
}

TEST_F(PriceTrackingUtilsTest, GetBookmarksWithClusterId_CountRestricted) {
  const uint64_t cluster_id = 12345L;
  AddProductBookmark(bookmark_model_.get(), u"product 1",
                     GURL("http://example.com/1"), cluster_id, true);
  AddProductBookmark(bookmark_model_.get(), u"product 2",
                     GURL("http://example.com/2"), cluster_id, true);
  bookmark_model_->AddURL(bookmark_model_->other_node(), 0, u"non-product",
                          GURL("http://www.example.com"));

  ASSERT_EQ(3U, bookmark_model_->other_node()->children().size());
  ASSERT_EQ(
      1U,
      GetBookmarksWithClusterId(bookmark_model_.get(), cluster_id, 1).size());
}

TEST_F(PriceTrackingUtilsTest, GetAllPriceTrackedBookmarks) {
  const uint64_t cluster_id = 12345L;
  const bookmarks::BookmarkNode* tracked_product =
      AddProductBookmark(bookmark_model_.get(), u"product 1",
                         GURL("http://example.com/1"), cluster_id, true);
  bookmark_model_->AddURL(bookmark_model_->other_node(), 0, u"non-product",
                          GURL("http://www.example.com"));

  ASSERT_EQ(2U, bookmark_model_->other_node()->children().size());

  shopping_service_->SetGetAllSubscriptionsCallbackValue(
      {BuildUserSubscriptionForClusterId(12345L)});

  base::RunLoop run_loop;
  GetAllPriceTrackedBookmarks(
      shopping_service_.get(), bookmark_model_.get(),
      base::BindOnce(
          [](base::RunLoop* looper,
             const bookmarks::BookmarkNode* known_tracked_bookmark,
             std::vector<const bookmarks::BookmarkNode*> bookmarks) {
            ASSERT_EQ(1U, bookmarks.size());
            ASSERT_EQ(bookmarks[0]->id(), known_tracked_bookmark->id());
            looper->Quit();
          },
          &run_loop, tracked_product));
  run_loop.Run();
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
  const bookmarks::BookmarkNode* product =
      AddProductBookmark(bookmark_model_.get(), u"product 1",
                         GURL("http://example.com/1"), 12345L, true);
  shopping_service_->SetIsSubscribedCallbackValue(true);

  EXPECT_CALL(*shopping_service_,
              IsSubscribed(SubscriptionWithId("12345"), testing::_))
      .Times(1);

  base::RunLoop run_loop;
  IsBookmarkPriceTracked(shopping_service_.get(), bookmark_model_.get(),
                         product,
                         base::BindOnce(
                             [](base::RunLoop* looper, bool is_tracked) {
                               ASSERT_TRUE(is_tracked);
                               looper->Quit();
                             },
                             &run_loop));
  run_loop.Run();
}

TEST_F(PriceTrackingUtilsTest, IsBookmarkPriceTracked_NonProduct) {
  const bookmarks::BookmarkNode* normal_bookmark =
      bookmark_model_->AddURL(bookmark_model_->other_node(), 0, u"non-product",
                              GURL("http://www.example.com"));

  EXPECT_CALL(*shopping_service_, IsSubscribed(testing::_, testing::_))
      .Times(0);

  base::RunLoop run_loop;
  IsBookmarkPriceTracked(shopping_service_.get(), bookmark_model_.get(),
                         normal_bookmark,
                         base::BindOnce(
                             [](base::RunLoop* looper, bool is_tracked) {
                               ASSERT_FALSE(is_tracked);
                               looper->Quit();
                             },
                             &run_loop));
  run_loop.Run();
}

TEST_F(PriceTrackingUtilsTest, PopulateOrUpdateBookmark) {
  const std::string new_title = "New Title";
  const std::string new_image_url = "https://example.com/product_image.png";
  const std::string new_country_code = "us";
  const long new_price = 500000L;
  const long old_price = 700000L;
  const std::string new_currency_code = "USD";
  const uint64_t new_offer_id = 10000L;
  const uint64_t cluster_id = 12345L;

  // Fill up bookmark meta with some nonsense data.
  power_bookmarks::PowerBookmarkMeta meta;
  meta.mutable_lead_image()->set_url("http://example.com/image.png");
  power_bookmarks::ShoppingSpecifics* specifics =
      meta.mutable_shopping_specifics();
  specifics->set_title("Old Title");
  specifics->set_country_code("abc");
  specifics->set_is_price_tracked(true);
  specifics->set_product_cluster_id(cluster_id);
  specifics->set_offer_id(67890L);
  power_bookmarks::ProductPrice* price = specifics->mutable_current_price();
  price->set_amount_micros(1000000L);
  price->set_currency_code("XYZ");

  // Provide new information via shopping service (ProductInfo).
  ProductInfo new_info;
  new_info.title = new_title;
  new_info.image_url = GURL(new_image_url);
  new_info.amount_micros = new_price;
  new_info.currency_code = new_currency_code;
  new_info.country_code = new_country_code;
  new_info.offer_id = new_offer_id;
  new_info.product_cluster_id = cluster_id;  // This shouldn't change.
  new_info.previous_amount_micros.emplace(old_price);

  EXPECT_TRUE(PopulateOrUpdateBookmarkMetaIfNeeded(&meta, new_info));

  specifics = meta.mutable_shopping_specifics();

  EXPECT_TRUE(specifics->is_price_tracked());
  EXPECT_EQ(new_title, specifics->title());
  EXPECT_EQ(new_image_url, meta.lead_image().url());
  EXPECT_EQ(new_country_code, specifics->country_code());
  EXPECT_EQ(new_price, specifics->current_price().amount_micros());
  EXPECT_EQ(new_currency_code, specifics->current_price().currency_code());
  EXPECT_EQ(new_offer_id, specifics->offer_id());
  EXPECT_EQ(cluster_id, specifics->product_cluster_id());
  EXPECT_EQ(old_price, specifics->previous_price().amount_micros());
}

TEST_F(PriceTrackingUtilsTest, PopulateOrUpdateBookmark_NoNewData) {
  const std::string title = "New Title";
  const std::string image_url = "https://example.com/product_image.png";
  const std::string country_code = "us";
  const long price_micros = 500000L;
  const std::string currency_code = "USD";
  const uint64_t offer_id = 67890L;
  const uint64_t cluster_id = 12345L;

  power_bookmarks::PowerBookmarkMeta meta;
  meta.mutable_lead_image()->set_url(image_url);
  power_bookmarks::ShoppingSpecifics* specifics =
      meta.mutable_shopping_specifics();
  specifics->set_title(title);
  specifics->set_country_code(country_code);
  specifics->set_is_price_tracked(true);
  specifics->set_product_cluster_id(cluster_id);
  specifics->set_offer_id(offer_id);
  power_bookmarks::ProductPrice* price = specifics->mutable_current_price();
  price->set_amount_micros(price_micros);
  price->set_currency_code(currency_code);

  // Provide the same information via shopping service (ProductInfo).
  ProductInfo info;
  info.title = title;
  info.image_url = GURL(image_url);
  info.amount_micros = price_micros;
  info.currency_code = currency_code;
  info.country_code = country_code;
  info.offer_id = offer_id;
  info.product_cluster_id = cluster_id;

  EXPECT_FALSE(PopulateOrUpdateBookmarkMetaIfNeeded(&meta, info));

  specifics = meta.mutable_shopping_specifics();

  EXPECT_TRUE(specifics->is_price_tracked());
  EXPECT_EQ(title, specifics->title());
  EXPECT_EQ(image_url, meta.lead_image().url());
  EXPECT_EQ(country_code, specifics->country_code());
  EXPECT_EQ(price_micros, specifics->current_price().amount_micros());
  EXPECT_EQ(currency_code, specifics->current_price().currency_code());
  EXPECT_EQ(offer_id, specifics->offer_id());
  EXPECT_EQ(cluster_id, specifics->product_cluster_id());
  EXPECT_FALSE(specifics->has_previous_price());
}

TEST_F(PriceTrackingUtilsTest,
       PopulateOrUpdateBookmark_EmptyClusterIdReplaced) {
  ProductInfo new_info;
  new_info.product_cluster_id = 12345L;

  power_bookmarks::PowerBookmarkMeta meta;

  EXPECT_TRUE(PopulateOrUpdateBookmarkMetaIfNeeded(&meta, new_info));

  EXPECT_EQ(new_info.product_cluster_id,
            meta.shopping_specifics().product_cluster_id());
}

TEST_F(PriceTrackingUtilsTest, PopulateOrUpdateBookmark_ClusterIdUnchanged) {
  const uint64_t cluster_id = 12345L;

  ProductInfo new_info;
  new_info.product_cluster_id = 99999L;

  power_bookmarks::PowerBookmarkMeta meta;
  meta.mutable_shopping_specifics()->set_product_cluster_id(cluster_id);

  EXPECT_FALSE(PopulateOrUpdateBookmarkMetaIfNeeded(&meta, new_info));

  EXPECT_EQ(cluster_id, meta.shopping_specifics().product_cluster_id());
}

TEST_F(PriceTrackingUtilsTest, PopulateOrUpdateBookmark_ImageRemoved) {
  ProductInfo new_info;
  new_info.image_url = GURL("");

  power_bookmarks::PowerBookmarkMeta meta;
  meta.mutable_lead_image()->set_url("http://example.com/image.png");

  EXPECT_TRUE(PopulateOrUpdateBookmarkMetaIfNeeded(&meta, new_info));

  EXPECT_TRUE(meta.lead_image().url().empty());
}

TEST_F(PriceTrackingUtilsTest, PopulateOrUpdateBookmark_TitleUpdated) {
  ProductInfo new_info;
  const uint64_t cluster_id = 12345L;
  const std::string new_title = "New Title";
  new_info.title = new_title;
  new_info.product_cluster_id = cluster_id;

  power_bookmarks::PowerBookmarkMeta meta;
  meta.mutable_shopping_specifics()->set_title("Nonempty Title");
  meta.mutable_shopping_specifics()->set_product_cluster_id(cluster_id);

  EXPECT_TRUE(PopulateOrUpdateBookmarkMetaIfNeeded(&meta, new_info));

  EXPECT_EQ(new_title, meta.shopping_specifics().title());
}

TEST_F(PriceTrackingUtilsTest, PopulateOrUpdateBookmark_NonemptyTitleKept) {
  ProductInfo new_info;
  const uint64_t cluster_id = 12345L;
  const std::string title = "Nonempty Title";
  new_info.title = "";
  new_info.product_cluster_id = cluster_id;

  power_bookmarks::PowerBookmarkMeta meta;
  meta.mutable_shopping_specifics()->set_title(title);
  meta.mutable_shopping_specifics()->set_product_cluster_id(cluster_id);

  EXPECT_FALSE(PopulateOrUpdateBookmarkMetaIfNeeded(&meta, new_info));

  EXPECT_EQ(title, meta.shopping_specifics().title());
}

// Make sure the previous price is cleared if we're no longer receiving it from
// the backend.
TEST_F(PriceTrackingUtilsTest, PopulateOrUpdateBookmark_PreviousPriceCleared) {
  ProductInfo new_info;

  power_bookmarks::PowerBookmarkMeta meta;
  meta.mutable_shopping_specifics()
      ->mutable_previous_price()
      ->set_currency_code("us");
  meta.mutable_shopping_specifics()
      ->mutable_previous_price()
      ->set_amount_micros(1234L);

  EXPECT_TRUE(meta.shopping_specifics().has_previous_price());

  EXPECT_TRUE(PopulateOrUpdateBookmarkMetaIfNeeded(&meta, new_info));

  EXPECT_FALSE(meta.shopping_specifics().has_previous_price());
}

TEST_F(PriceTrackingUtilsTest, MaybeEnableEmailNotifications) {
  // Verify the initial pref values.
  ASSERT_EQ(false, pref_service_->GetBoolean(kPriceEmailNotificationsEnabled));

  MaybeEnableEmailNotifications(pref_service_.get());

  // Verify the updated pref values.
  ASSERT_EQ(true, pref_service_->GetBoolean(kPriceEmailNotificationsEnabled));

  // Mock that user has customized the email pref setting, in which case we
  // shouldn't auto enable it.
  pref_service_->SetBoolean(kPriceEmailNotificationsEnabled, false);

  MaybeEnableEmailNotifications(pref_service_.get());

  ASSERT_EQ(false, pref_service_->GetBoolean(kPriceEmailNotificationsEnabled));
}

TEST_F(PriceTrackingUtilsTest, TestSubscriptionForClusterIdCreation) {
  uint64_t cluster_id = 12345L;
  std::string cluster_id_string = "12345";

  CommerceSubscription sub = BuildUserSubscriptionForClusterId(cluster_id);

  ASSERT_EQ(sub.id, cluster_id_string);
  ASSERT_EQ(sub.id_type, IdentifierType::kProductClusterId);
  ASSERT_EQ(sub.management_type, ManagementType::kUserManaged);
  ASSERT_EQ(sub.type, SubscriptionType::kPriceTrack);
}

TEST_F(PriceTrackingUtilsTest, TestGetBookmarkParentName) {
  const GURL url = GURL("https://www.foo.com");

  ASSERT_FALSE(
      commerce::GetBookmarkParentName(bookmark_model_.get(), url).has_value());

  bookmark_model_->AddURL(bookmark_model_->mobile_node(), 0, u"test", url,
                          nullptr, std::nullopt, std::nullopt, true);

  ASSERT_EQ(
      bookmark_model_->mobile_node()->GetTitle(),
      commerce::GetBookmarkParentName(bookmark_model_.get(), url).value());
}

// Ensure the utility to get the shopping collection knows when to create or
// simply lookup the folder. The folder's UUID should be deterministic.
TEST_F(PriceTrackingUtilsTest, GetShoppingCollection) {
  const base::Uuid collection_uuid =
      base::Uuid::ParseLowercase(bookmarks::kShoppingCollectionUuid);

  const bookmarks::BookmarkNode* collection =
      GetShoppingCollectionBookmarkFolder(bookmark_model_.get());

  ASSERT_EQ(collection, nullptr);

  collection = GetShoppingCollectionBookmarkFolder(bookmark_model_.get(), true);

  ASSERT_NE(collection, nullptr);
  ASSERT_EQ(collection->uuid(), collection_uuid);

  // Deleting the collection should behave like any other bookmark node
  // deletion.
  bookmark_model_->Remove(
      collection, bookmarks::metrics::BookmarkEditSource::kUser, FROM_HERE);

  collection = GetShoppingCollectionBookmarkFolder(bookmark_model_.get());

  ASSERT_EQ(collection, nullptr);

  // Creating the collection a second time should result in the same UUID.
  collection = GetShoppingCollectionBookmarkFolder(bookmark_model_.get(), true);

  ASSERT_NE(collection, nullptr);
  ASSERT_EQ(collection->uuid(), collection_uuid);
}

TEST_F(PriceTrackingUtilsTest, GetShoppingCollection_InvalidParams) {
  const bookmarks::BookmarkNode* collection =
      GetShoppingCollectionBookmarkFolder(nullptr);

  ASSERT_EQ(collection, nullptr);
}

TEST_F(PriceTrackingUtilsTest, GetProductClusterIdFromBookmark) {
  const GURL product_url = GURL("https://example.com/product");
  const GURL non_product_url = GURL("https://example.com/");
  const uint64_t cluster_id = 12345L;
  AddProductBookmark(bookmark_model_.get(), u"product", product_url, cluster_id,
                     true);

  ASSERT_EQ(GetProductClusterIdFromBookmark(product_url, bookmark_model_.get())
                .value(),
            cluster_id);
  ASSERT_FALSE(
      GetProductClusterIdFromBookmark(non_product_url, bookmark_model_.get())
          .has_value());
}

TEST_F(PriceTrackingUtilsTest, RemoveDanglingSubscriptions) {
  std::vector<CommerceSubscription> subs;
  const uint64_t dangling_id = 12345L;
  subs.push_back(BuildUserSubscriptionForClusterId(dangling_id));
  uint64_t valid_id = 67890L;
  subs.push_back(BuildUserSubscriptionForClusterId(valid_id));
  shopping_service_->SetGetAllSubscriptionsCallbackValue(std::move(subs));
  shopping_service_->SetUnsubscribeCallbackValue(true);

  // Ensure there's a bookmark for the "valid_id" but not the "dangling_id".
  AddProductBookmark(bookmark_model_.get(), u"product",
                     GURL("https://example.com/"), valid_id, true);

  EXPECT_CALL(*shopping_service_,
              Unsubscribe(VectorHasSubscriptionWithId(
                              base::NumberToString(dangling_id)),
                          testing::_))
      .Times(1);
  EXPECT_CALL(
      *shopping_service_,
      Unsubscribe(VectorHasSubscriptionWithId(base::NumberToString(valid_id)),
                  testing::_))
      .Times(0);

  base::RunLoop run_loop;
  RemoveDanglingSubscriptions(shopping_service_.get(), bookmark_model_.get(),
                              base::BindOnce(
                                  [](base::RunLoop* run_loop, size_t count) {
                                    ASSERT_EQ(count, 1u);
                                    run_loop->Quit();
                                  },
                                  &run_loop));
  run_loop.Run();
}

// Ensure the callback runs even if there are no dangling
TEST_F(PriceTrackingUtilsTest, RemoveDanglingSubscriptions_NoDanglingSubs) {
  std::vector<CommerceSubscription> subs;
  uint64_t valid_id = 67890L;
  subs.push_back(BuildUserSubscriptionForClusterId(valid_id));
  shopping_service_->SetGetAllSubscriptionsCallbackValue(std::move(subs));
  shopping_service_->SetUnsubscribeCallbackValue(true);

  // Ensure there's a bookmark for the above subscription.
  AddProductBookmark(bookmark_model_.get(), u"product",
                     GURL("https://example.com/"), valid_id, true);

  EXPECT_CALL(*shopping_service_, Unsubscribe(testing::_, testing::_)).Times(0);

  base::RunLoop run_loop;
  RemoveDanglingSubscriptions(shopping_service_.get(), bookmark_model_.get(),
                              base::BindOnce(
                                  [](base::RunLoop* run_loop, size_t count) {
                                    ASSERT_EQ(count, 0u);
                                    run_loop->Quit();
                                  },
                                  &run_loop));
  run_loop.Run();
}

}  // namespace
}  // namespace commerce
