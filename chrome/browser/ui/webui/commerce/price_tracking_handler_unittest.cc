// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/commerce/price_tracking_handler.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/commerce/core/mock_account_checker.h"
#include "components/commerce/core/mock_shopping_service.h"
#include "components/commerce/core/mojom/shared.mojom.h"
#include "components/commerce/core/price_tracking_utils.h"
#include "components/commerce/core/test_utils.h"
#include "components/feature_engagement/test/mock_tracker.h"
#include "components/power_bookmarks/core/power_bookmark_utils.h"
#include "components/power_bookmarks/core/proto/power_bookmark_meta.pb.h"
#include "components/power_bookmarks/core/proto/shopping_specifics.pb.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace commerce {
namespace {

class MockPage : public price_tracking::mojom::Page {
 public:
  MockPage() = default;
  ~MockPage() override = default;

  mojo::PendingRemote<price_tracking::mojom::Page> BindAndGetRemote() {
    DCHECK(!receiver_.is_bound());
    return receiver_.BindNewPipeAndPassRemote();
  }
  mojo::Receiver<price_tracking::mojom::Page> receiver_{this};

  MOCK_METHOD(void,
              PriceTrackedForBookmark,
              (shared::mojom::BookmarkProductInfoPtr bookmark_product),
              (override));
  MOCK_METHOD(void,
              PriceUntrackedForBookmark,
              (shared::mojom::BookmarkProductInfoPtr bookmark_product),
              (override));
  MOCK_METHOD(void,
              OperationFailedForBookmark,
              (shared::mojom::BookmarkProductInfoPtr bookmark_product,
               bool attempted_track),
              (override));
  MOCK_METHOD(void,
              OnProductBookmarkMoved,
              (shared::mojom::BookmarkProductInfoPtr bookmark_product),
              (override));
};

// A matcher for checking if a mojo bookmark info has the specified bookmark ID
// (uint64_t).
MATCHER_P(MojoBookmarkInfoWithId, expected_id, "") {
  return arg->bookmark_id == expected_id;
}

// A matcher for checking if a mojo bookmark info has the specified cluster ID
// (uint64_t).
MATCHER_P(MojoBookmarkInfoWithClusterId, expected_id, "") {
  return arg->info->cluster_id == expected_id;
}

void AssertEqualProductInfoLists(
    base::OnceClosure closure,
    std::vector<shared::mojom::BookmarkProductInfoPtr> expected,
    std::vector<shared::mojom::BookmarkProductInfoPtr> found) {
  ASSERT_EQ(expected.size(), found.size());
  std::unordered_map<uint64_t, shared::mojom::BookmarkProductInfoPtr*>
      found_map;
  for (auto& item : found) {
    found_map[item->bookmark_id] = &item;
  }

  for (auto& item : expected) {
    auto find_it = found_map.find(item->bookmark_id);
    ASSERT_FALSE(find_it == found_map.end());

    shared::mojom::BookmarkProductInfoPtr* found_item = find_it->second;

    ASSERT_EQ(item->bookmark_id, (*found_item)->bookmark_id);
    ASSERT_EQ(item->info->current_price, (*found_item)->info->current_price);
    ASSERT_EQ(item->info->domain, (*found_item)->info->domain);
    ASSERT_EQ(item->info->title, (*found_item)->info->title);
    ASSERT_EQ(item->info->image_url.spec(),
              (*found_item)->info->image_url.spec());
  }
  std::move(closure).Run();
}

}  // namespace

class PriceTrackingHandlerTest : public testing::Test {
 public:
  void SetUp() override {
    web_ui_ = std::make_unique<content::TestWebUI>();
    account_checker_ = std::make_unique<MockAccountChecker>();
    account_checker_->SetLocale("en-us");
    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    MockAccountChecker::RegisterCommercePrefs(pref_service_->registry());
    SetTabCompareEnterprisePolicyPref(pref_service_.get(), 0);
    SetShoppingListEnterprisePolicyPref(pref_service_.get(), true);
    account_checker_->SetPrefs(pref_service_.get());

    shopping_service_ = std::make_unique<MockShoppingService>();
    shopping_service_->SetAccountChecker(account_checker_.get());

    auto client = std::make_unique<bookmarks::TestBookmarkClient>();
    client->SetIsSyncFeatureEnabledIncludingBookmarks(true);
    bookmark_model_ =
        bookmarks::TestBookmarkClient::CreateModelWithClient(std::move(client));

    handler_ = std::make_unique<commerce::PriceTrackingHandler>(
        page_.BindAndGetRemote(),
        mojo::PendingReceiver<price_tracking::mojom::PriceTrackingHandler>(),
        nullptr, shopping_service_.get(), &tracker_, bookmark_model_.get());
  }

 protected:
  std::unique_ptr<content::TestWebUI> web_ui_;
  std::unique_ptr<MockShoppingService> shopping_service_;
  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model_;
  feature_engagement::test::MockTracker tracker_;
  std::unique_ptr<MockAccountChecker> account_checker_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;

  MockPage page_;
  std::unique_ptr<commerce::PriceTrackingHandler> handler_;

  base::test::ScopedFeatureList features_;
  base::test::TaskEnvironment task_environment_;
};

TEST_F(PriceTrackingHandlerTest, ConvertToMojoTypes) {
  const bookmarks::BookmarkNode* product = AddProductBookmark(
      bookmark_model_.get(), u"product 1", GURL("http://example.com/1"), 123L,
      true, 1230000, "usd");

  const std::string image_url = "https://example.com/image.png";
  std::unique_ptr<power_bookmarks::PowerBookmarkMeta> meta =
      power_bookmarks::GetNodePowerBookmarkMeta(bookmark_model_.get(), product);
  meta->mutable_lead_image()->set_url(image_url);
  meta->mutable_shopping_specifics()
      ->mutable_previous_price()
      ->set_amount_micros(4560000);
  meta->mutable_shopping_specifics()
      ->mutable_previous_price()
      ->set_currency_code("usd");
  power_bookmarks::SetNodePowerBookmarkMeta(bookmark_model_.get(), product,
                                            std::move(meta));

  std::vector<const bookmarks::BookmarkNode*> bookmark_list;
  bookmark_list.push_back(product);

  std::vector<shared::mojom::BookmarkProductInfoPtr> mojo_list =
      PriceTrackingHandler::BookmarkListToMojoList(*bookmark_model_,
                                                   bookmark_list, "en-us");

  EXPECT_EQ(mojo_list[0]->bookmark_id, product->id());
  EXPECT_EQ(mojo_list[0]->info->current_price, "$1.23");
  EXPECT_EQ(mojo_list[0]->info->previous_price, "$4.56");
  EXPECT_EQ(mojo_list[0]->info->domain, "example.com");
  EXPECT_EQ(mojo_list[0]->info->title, "product 1");
  EXPECT_EQ(mojo_list[0]->info->image_url.spec(), image_url);
}

// If the new price is greater than the old price, we shouldn't include the
// |previous_price| field in the mojo data type.
TEST_F(PriceTrackingHandlerTest, ConvertToMojoTypes_PriceIncrease) {
  const bookmarks::BookmarkNode* product = AddProductBookmark(
      bookmark_model_.get(), u"product 1", GURL("http://example.com/1"), 123L,
      true, 1230000, "usd");

  const std::string image_url = "https://example.com/image.png";
  std::unique_ptr<power_bookmarks::PowerBookmarkMeta> meta =
      power_bookmarks::GetNodePowerBookmarkMeta(bookmark_model_.get(), product);
  meta->mutable_lead_image()->set_url(image_url);
  meta->mutable_shopping_specifics()
      ->mutable_previous_price()
      ->set_amount_micros(1000000);
  meta->mutable_shopping_specifics()
      ->mutable_previous_price()
      ->set_currency_code("usd");
  power_bookmarks::SetNodePowerBookmarkMeta(bookmark_model_.get(), product,
                                            std::move(meta));

  std::vector<const bookmarks::BookmarkNode*> bookmark_list;
  bookmark_list.push_back(product);

  std::vector<shared::mojom::BookmarkProductInfoPtr> mojo_list =
      PriceTrackingHandler::BookmarkListToMojoList(*bookmark_model_,
                                                   bookmark_list, "en-us");

  EXPECT_EQ(mojo_list[0]->bookmark_id, product->id());
  EXPECT_EQ(mojo_list[0]->info->current_price, "$1.23");
  EXPECT_TRUE(mojo_list[0]->info->previous_price.empty());
  EXPECT_EQ(mojo_list[0]->info->domain, "example.com");
  EXPECT_EQ(mojo_list[0]->info->title, "product 1");
  EXPECT_EQ(mojo_list[0]->info->image_url.spec(), image_url);
}

TEST_F(PriceTrackingHandlerTest, TestTrackProductSuccess) {
  uint64_t cluster_id = 123u;
  const bookmarks::BookmarkNode* product = AddProductBookmark(
      bookmark_model_.get(), u"product 1", GURL("http://example.com/1"),
      cluster_id, false, 1230000, "usd");

  EXPECT_CALL(*shopping_service_,
              Subscribe(VectorHasSubscriptionWithId("123"), testing::_))
      .Times(1);
  EXPECT_CALL(page_,
              PriceTrackedForBookmark(MojoBookmarkInfoWithId(product->id())))
      .Times(1);
  EXPECT_CALL(page_, OperationFailedForBookmark(testing::_, testing::_))
      .Times(0);

  handler_->TrackPriceForBookmark(product->id());

  // Assume the subscription callback fires with a success.
  handler_->OnSubscribe(BuildUserSubscriptionForClusterId(cluster_id), true);

  task_environment_.RunUntilIdle();
}

TEST_F(PriceTrackingHandlerTest, TestUntrackProductSuccess) {
  uint64_t cluster_id = 123u;
  const bookmarks::BookmarkNode* product = AddProductBookmark(
      bookmark_model_.get(), u"product 1", GURL("http://example.com/1"),
      cluster_id, true, 1230000, "usd");

  EXPECT_CALL(*shopping_service_,
              Unsubscribe(VectorHasSubscriptionWithId("123"), testing::_))
      .Times(1);
  EXPECT_CALL(page_,
              PriceUntrackedForBookmark(MojoBookmarkInfoWithId(product->id())))
      .Times(1);
  EXPECT_CALL(page_, OperationFailedForBookmark(testing::_, testing::_))
      .Times(0);

  handler_->UntrackPriceForBookmark(product->id());

  // Assume the subscription callback fires with a success.
  handler_->OnUnsubscribe(BuildUserSubscriptionForClusterId(cluster_id), true);

  task_environment_.RunUntilIdle();
}

TEST_F(PriceTrackingHandlerTest, TestTrackProductFailure) {
  uint64_t cluster_id = 123u;
  const bookmarks::BookmarkNode* product = AddProductBookmark(
      bookmark_model_.get(), u"product 1", GURL("http://example.com/1"),
      cluster_id, false, 1230000, "usd");

  // Simulate failed calls in the subscriptions manager.
  shopping_service_->SetSubscribeCallbackValue(false);
  shopping_service_->SetUnsubscribeCallbackValue(false);

  // "untrack" should be called once to undo the "track" change in the UI.
  EXPECT_CALL(page_,
              PriceUntrackedForBookmark(MojoBookmarkInfoWithId(product->id())))
      .Times(1);
  EXPECT_CALL(page_, PriceTrackedForBookmark(testing::_)).Times(0);
  EXPECT_CALL(page_, OperationFailedForBookmark(
                         MojoBookmarkInfoWithId(product->id()), true))
      .Times(1);

  handler_->TrackPriceForBookmark(product->id());

  // Assume the subscription callback fires with a failure.
  handler_->OnUnsubscribe(BuildUserSubscriptionForClusterId(cluster_id), false);

  task_environment_.RunUntilIdle();
}

TEST_F(PriceTrackingHandlerTest, TestUntrackProductFailure) {
  uint64_t cluster_id = 123u;
  const bookmarks::BookmarkNode* product = AddProductBookmark(
      bookmark_model_.get(), u"product 1", GURL("http://example.com/1"),
      cluster_id, true, 1230000, "usd");

  // Simulate failed calls in the subscriptions manager.
  shopping_service_->SetSubscribeCallbackValue(false);
  shopping_service_->SetUnsubscribeCallbackValue(false);

  // "track" should be called once to undo the "untrack" change in the UI.
  EXPECT_CALL(page_, PriceTrackedForBookmark(testing::_)).Times(1);
  EXPECT_CALL(page_,
              PriceUntrackedForBookmark(MojoBookmarkInfoWithId(product->id())))
      .Times(0);
  EXPECT_CALL(page_, OperationFailedForBookmark(
                         MojoBookmarkInfoWithId(product->id()), false))
      .Times(1);

  handler_->UntrackPriceForBookmark(product->id());

  // Assume the subscription callback fires with a failure.
  handler_->OnUnsubscribe(BuildUserSubscriptionForClusterId(cluster_id), false);

  task_environment_.RunUntilIdle();
}

TEST_F(PriceTrackingHandlerTest, PageUpdateForPriceTrackChange) {
  const bookmarks::BookmarkNode* product = AddProductBookmark(
      bookmark_model_.get(), u"product 1", GURL("http://example.com/1"), 123L,
      true, 1230000, "usd");

  EXPECT_CALL(page_,
              PriceUntrackedForBookmark(MojoBookmarkInfoWithId(product->id())));

  // Assume the plumbing for subscriptions works and fake an unsubscribe event.
  handler_->OnUnsubscribe(BuildUserSubscriptionForClusterId(123L), true);

  task_environment_.RunUntilIdle();
}

TEST_F(PriceTrackingHandlerTest, TestUnsubscribeCausedByBookmarkDeletion) {
  uint64_t cluster_id = 123u;
  EXPECT_CALL(page_, PriceUntrackedForBookmark(
                         MojoBookmarkInfoWithClusterId(cluster_id)))
      .Times(1);

  handler_->OnUnsubscribe(BuildUserSubscriptionForClusterId(cluster_id), true);

  task_environment_.RunUntilIdle();
}

TEST_F(PriceTrackingHandlerTest, TestBookmarkNodeMoved) {
  uint64_t cluster_id = 12345u;

  const bookmarks::BookmarkNode* node_with_product = AddProductBookmark(
      bookmark_model_.get(), u"title", GURL("https://example.com"), cluster_id);
  shopping_service_->SetIsSubscribedCallbackValue(true);
  const bookmarks::BookmarkNode* node_without_product =
      bookmark_model_->AddNewURL(bookmark_model_->other_node(), 0, u"title",
                                 GURL("https://test.com"));

  EXPECT_CALL(page_, OnProductBookmarkMoved(
                         MojoBookmarkInfoWithId(node_with_product->id())))
      .Times(1);
  bookmark_model_->Move(node_with_product, bookmark_model_->bookmark_bar_node(),
                        0);
  bookmark_model_->Move(node_without_product,
                        bookmark_model_->bookmark_bar_node(), 1);
  base::RunLoop().RunUntilIdle();
}

TEST_F(PriceTrackingHandlerTest, TestGetProductInfo_FeatureEnabled) {
  EXPECT_CALL(tracker_, NotifyEvent("price_tracking_side_panel_shown"));

  shopping_service_->SetIsReady(true);
  shopping_service_->SetIsShoppingListEligible(true);

  const bookmarks::BookmarkNode* product = AddProductBookmark(
      bookmark_model_.get(), u"product 1", GURL("http://example.com/1"), 123L,
      true, 1230000, "usd");
  AddProductBookmark(bookmark_model_.get(), u"product 2",
                     GURL("http://example.com/2"), 456L, false, 4560000, "usd");
  shopping_service_->SetGetAllSubscriptionsCallbackValue(
      {BuildUserSubscriptionForClusterId(123L)});

  std::vector<const bookmarks::BookmarkNode*> bookmark_list;
  bookmark_list.push_back(product);
  shopping_service_->SetGetAllPriceTrackedBookmarksCallbackValue(bookmark_list);
  shopping_service_->SetGetAllShoppingBookmarksValue(bookmark_list);

  std::vector<shared::mojom::BookmarkProductInfoPtr> mojo_list =
      PriceTrackingHandler::BookmarkListToMojoList(*bookmark_model_,
                                                   bookmark_list, "en-us");

  handler_->GetAllPriceTrackedBookmarkProductInfo(base::BindOnce(
      &AssertEqualProductInfoLists, base::DoNothing(), std::move(mojo_list)));

  task_environment_.RunUntilIdle();
}

TEST_F(PriceTrackingHandlerTest, TestGetProductInfo_FeatureDisabled) {
  shopping_service_->SetIsShoppingListEligible(false);
  EXPECT_CALL(tracker_, NotifyEvent("price_tracking_side_panel_shown"))
      .Times(0);

  const bookmarks::BookmarkNode* product = AddProductBookmark(
      bookmark_model_.get(), u"product 1", GURL("http://example.com/1"), 123L,
      true, 1230000, "usd");

  std::vector<const bookmarks::BookmarkNode*> bookmark_list;
  bookmark_list.push_back(product);
  std::vector<shared::mojom::BookmarkProductInfoPtr> empty_list;

  handler_->GetAllPriceTrackedBookmarkProductInfo(base::BindOnce(
      &AssertEqualProductInfoLists, base::DoNothing(), std::move(empty_list)));
}

TEST_F(PriceTrackingHandlerTest, TestGetAllShoppingInfo_FeatureEnabled) {
  base::RunLoop run_loop;

  const bookmarks::BookmarkNode* product = AddProductBookmark(
      bookmark_model_.get(), u"product 1", GURL("http://example.com/1"), 123L,
      true, 1230000, "usd");
  const bookmarks::BookmarkNode* product2 = AddProductBookmark(
      bookmark_model_.get(), u"product 2", GURL("http://example.com/2"), 456L,
      false, 4560000, "usd");

  std::vector<const bookmarks::BookmarkNode*> bookmark_list;
  bookmark_list.push_back(product);
  bookmark_list.push_back(product2);
  shopping_service_->SetGetAllPriceTrackedBookmarksCallbackValue(bookmark_list);
  shopping_service_->SetGetAllShoppingBookmarksValue(bookmark_list);

  std::vector<shared::mojom::BookmarkProductInfoPtr> mojo_list =
      PriceTrackingHandler::BookmarkListToMojoList(*bookmark_model_,
                                                   bookmark_list, "en-us");

  handler_->GetAllShoppingBookmarkProductInfo(
      base::BindOnce(&AssertEqualProductInfoLists, run_loop.QuitClosure(),
                     std::move(mojo_list)));
}

}  // namespace commerce
