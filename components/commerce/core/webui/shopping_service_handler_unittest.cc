// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/webui/shopping_service_handler.h"

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/uuid.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/mock_account_checker.h"
#include "components/commerce/core/mock_shopping_service.h"
#include "components/commerce/core/pref_names.h"
#include "components/commerce/core/price_tracking_utils.h"
#include "components/commerce/core/product_specifications/product_specifications_service.h"
#include "components/commerce/core/product_specifications/product_specifications_set.h"
#include "components/commerce/core/subscriptions/commerce_subscription.h"
#include "components/commerce/core/test_utils.h"
#include "components/feature_engagement/test/mock_tracker.h"
#include "components/power_bookmarks/core/power_bookmark_utils.h"
#include "components/power_bookmarks/core/proto/power_bookmark_meta.pb.h"
#include "components/power_bookmarks/core/proto/shopping_specifics.pb.h"
#include "components/prefs/testing_pref_service.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/webui/resources/cr_components/commerce/shopping_service.mojom.h"
#include "url/gurl.h"

namespace commerce {
namespace {

class MockPage : public shopping_service::mojom::Page {
 public:
  MockPage() = default;
  ~MockPage() override = default;

  mojo::PendingRemote<shopping_service::mojom::Page> BindAndGetRemote() {
    DCHECK(!receiver_.is_bound());
    return receiver_.BindNewPipeAndPassRemote();
  }
  mojo::Receiver<shopping_service::mojom::Page> receiver_{this};

  MOCK_METHOD(void,
              PriceTrackedForBookmark,
              (shopping_service::mojom::BookmarkProductInfoPtr product),
              (override));
  MOCK_METHOD(void,
              PriceUntrackedForBookmark,
              (shopping_service::mojom::BookmarkProductInfoPtr product),
              (override));
  MOCK_METHOD(void,
              OperationFailedForBookmark,
              (shopping_service::mojom::BookmarkProductInfoPtr product,
               bool is_tracked),
              (override));
  MOCK_METHOD(void,
              OnProductBookmarkMoved,
              (shopping_service::mojom::BookmarkProductInfoPtr product),
              (override));
  MOCK_METHOD(void,
              OnProductSpecificationsSetAdded,
              (shopping_service::mojom::ProductSpecificationsSetPtr set),
              (override));
  MOCK_METHOD(void,
              OnProductSpecificationsSetUpdated,
              (shopping_service::mojom::ProductSpecificationsSetPtr set),
              (override));
  MOCK_METHOD(void,
              OnProductSpecificationsSetRemoved,
              (const base::Uuid& uuid),
              (override));
};

class MockDelegate : public ShoppingServiceHandler::Delegate {
 public:
  MockDelegate() {
    SetCurrentTabUrl(GURL("http://example.com"));
    SetCurrentTabUkmSourceId(123);
  }
  ~MockDelegate() override = default;

  MOCK_METHOD(std::optional<GURL>, GetCurrentTabUrl, (), (override));
  MOCK_METHOD(void, ShowInsightsSidePanelUI, (), (override));
  MOCK_METHOD(void, OpenUrlInNewTab, (const GURL& url), (override));
  MOCK_METHOD(void, ShowFeedback, (), (override));
  MOCK_METHOD(const bookmarks::BookmarkNode*,
              GetOrAddBookmarkForCurrentUrl,
              (),
              (override));
  MOCK_METHOD(void, ShowBookmarkEditorForCurrentUrl, (), (override));
  MOCK_METHOD(ukm::SourceId, GetCurrentTabUkmSourceId, (), (override));

  void SetCurrentTabUrl(const GURL& url) {
    ON_CALL(*this, GetCurrentTabUrl)
        .WillByDefault(testing::Return(std::make_optional<GURL>(url)));
  }

  void SetCurrentTabUkmSourceId(ukm::SourceId id) {
    ON_CALL(*this, GetCurrentTabUkmSourceId).WillByDefault(testing::Return(id));
  }
};

class MockProductSpecificationsService : public ProductSpecificationsService {
 public:
  MockProductSpecificationsService() : ProductSpecificationsService(nullptr) {}
  ~MockProductSpecificationsService() override = default;
  MOCK_METHOD(const std::vector<ProductSpecificationsSet>,
              GetAllProductSpecifications,
              (),
              (override));
  MOCK_METHOD(const std::optional<ProductSpecificationsSet>,
              GetSetByUuid,
              (const base::Uuid& uuid),
              (override));
  MOCK_METHOD(const std::optional<ProductSpecificationsSet>,
              AddProductSpecificationsSet,
              (const std::string& name, const std::vector<GURL>& urls),
              (override));
  MOCK_METHOD(void,
              DeleteProductSpecificationsSet,
              (const std::string& uuid),
              (override));
};

void GetEvaluationProductInfos(
    base::OnceClosure closure,
    std::vector<shopping_service::mojom::BookmarkProductInfoPtr> expected,
    std::vector<shopping_service::mojom::BookmarkProductInfoPtr> found) {
  ASSERT_EQ(expected.size(), found.size());
  std::unordered_map<uint64_t, shopping_service::mojom::BookmarkProductInfoPtr*>
      found_map;
  for (auto& item : found) {
    found_map[item->bookmark_id] = &item;
  }

  for (auto& item : expected) {
    auto find_it = found_map.find(item->bookmark_id);
    ASSERT_FALSE(find_it == found_map.end());

    shopping_service::mojom::BookmarkProductInfoPtr* found_item =
        find_it->second;

    ASSERT_EQ(item->bookmark_id, (*found_item)->bookmark_id);
    ASSERT_EQ(item->info->current_price, (*found_item)->info->current_price);
    ASSERT_EQ(item->info->domain, (*found_item)->info->domain);
    ASSERT_EQ(item->info->title, (*found_item)->info->title);
    ASSERT_EQ(item->info->image_url.spec(),
              (*found_item)->info->image_url.spec());
  }
  std::move(closure).Run();
}

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

class ShoppingServiceHandlerTest : public testing::Test {
 public:
  ShoppingServiceHandlerTest() {
    features_.InitAndEnableFeature(kShoppingList);
  }

 protected:
  void SetUp() override {
    auto client = std::make_unique<bookmarks::TestBookmarkClient>();
    client->SetIsSyncFeatureEnabledIncludingBookmarks(true);
    product_spec_service_ =
        std::make_unique<MockProductSpecificationsService>();
    bookmark_model_ =
        bookmarks::TestBookmarkClient::CreateModelWithClient(std::move(client));
    account_checker_ = std::make_unique<MockAccountChecker>();
    account_checker_->SetLocale("en-us");
    shopping_service_ = std::make_unique<MockShoppingService>();
    shopping_service_->SetAccountChecker(account_checker_.get());
    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    RegisterPrefs(pref_service_->registry());
    SetShoppingListEnterprisePolicyPref(pref_service_.get(), true);

    ON_CALL(*shopping_service_, GetProductSpecificationsService)
        .WillByDefault(testing::Return(product_spec_service_.get()));

    auto delegate = std::make_unique<MockDelegate>();
    delegate_ = delegate.get();
    handler_ = std::make_unique<commerce::ShoppingServiceHandler>(
        page_.BindAndGetRemote(),
        mojo::PendingReceiver<
            shopping_service::mojom::ShoppingServiceHandler>(),
        bookmark_model_.get(), shopping_service_.get(), pref_service_.get(),
        &tracker_, std::move(delegate));
  }

  MockPage page_;
  std::unique_ptr<MockProductSpecificationsService> product_spec_service_;
  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model_;
  std::unique_ptr<MockAccountChecker> account_checker_;
  std::unique_ptr<MockShoppingService> shopping_service_;
  std::unique_ptr<commerce::ShoppingServiceHandler> handler_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  raw_ptr<MockDelegate> delegate_;
  feature_engagement::test::MockTracker tracker_;
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList features_;
};

TEST_F(ShoppingServiceHandlerTest, ConvertToMojoTypes) {
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

  std::vector<shopping_service::mojom::BookmarkProductInfoPtr> mojo_list =
      ShoppingServiceHandler::BookmarkListToMojoList(*bookmark_model_,
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
TEST_F(ShoppingServiceHandlerTest, ConvertToMojoTypes_PriceIncrease) {
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

  std::vector<shopping_service::mojom::BookmarkProductInfoPtr> mojo_list =
      ShoppingServiceHandler::BookmarkListToMojoList(*bookmark_model_,
                                                  bookmark_list, "en-us");

  EXPECT_EQ(mojo_list[0]->bookmark_id, product->id());
  EXPECT_EQ(mojo_list[0]->info->current_price, "$1.23");
  EXPECT_TRUE(mojo_list[0]->info->previous_price.empty());
  EXPECT_EQ(mojo_list[0]->info->domain, "example.com");
  EXPECT_EQ(mojo_list[0]->info->title, "product 1");
  EXPECT_EQ(mojo_list[0]->info->image_url.spec(), image_url);
}

TEST_F(ShoppingServiceHandlerTest, TestTrackProductSuccess) {
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

TEST_F(ShoppingServiceHandlerTest, TestUntrackProductSuccess) {
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

TEST_F(ShoppingServiceHandlerTest, TestTrackProductFailure) {
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

TEST_F(ShoppingServiceHandlerTest, TestUntrackProductFailure) {
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

TEST_F(ShoppingServiceHandlerTest, PageUpdateForPriceTrackChange) {
  const bookmarks::BookmarkNode* product = AddProductBookmark(
      bookmark_model_.get(), u"product 1", GURL("http://example.com/1"), 123L,
      true, 1230000, "usd");

  EXPECT_CALL(page_,
              PriceUntrackedForBookmark(MojoBookmarkInfoWithId(product->id())));

  // Assume the plumbing for subscriptions works and fake an unsubscribe event.
  handler_->OnUnsubscribe(BuildUserSubscriptionForClusterId(123L), true);

  task_environment_.RunUntilIdle();
}

TEST_F(ShoppingServiceHandlerTest, TestUnsubscribeCausedByBookmarkDeletion) {
  uint64_t cluster_id = 123u;
  EXPECT_CALL(page_, PriceUntrackedForBookmark(
                         MojoBookmarkInfoWithClusterId(cluster_id)))
      .Times(1);

  handler_->OnUnsubscribe(BuildUserSubscriptionForClusterId(cluster_id), true);

  task_environment_.RunUntilIdle();
}

TEST_F(ShoppingServiceHandlerTest, TestGetProductInfo_FeatureEnabled) {
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

  std::vector<shopping_service::mojom::BookmarkProductInfoPtr> mojo_list =
      ShoppingServiceHandler::BookmarkListToMojoList(*bookmark_model_,
                                                  bookmark_list, "en-us");

  handler_->GetAllPriceTrackedBookmarkProductInfo(base::BindOnce(
      &GetEvaluationProductInfos, base::DoNothing(), std::move(mojo_list)));

  task_environment_.RunUntilIdle();
}

TEST_F(ShoppingServiceHandlerTest, TestGetAllShoppingInfo_FeatureEnabled) {
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

  std::vector<shopping_service::mojom::BookmarkProductInfoPtr> mojo_list =
      ShoppingServiceHandler::BookmarkListToMojoList(*bookmark_model_,
                                                  bookmark_list, "en-us");

  handler_->GetAllShoppingBookmarkProductInfo(
      base::BindOnce(&GetEvaluationProductInfos, run_loop.QuitClosure(),
                     std::move(mojo_list)));
}

TEST_F(ShoppingServiceHandlerTest,
       TestGetProductInfoForCurrentUrl_FeatureEligible) {
  base::RunLoop run_loop;

  shopping_service_->SetIsPriceInsightsEligible(true);

  std::optional<commerce::ProductInfo> info;
  info.emplace();
  info->title = "example_title";
  info->product_cluster_title = "example_cluster_title";
  info->product_cluster_id = std::optional<uint64_t>(123u);
  shopping_service_->SetResponseForGetProductInfoForUrl(info);

  handler_->GetProductInfoForCurrentUrl(base::BindOnce(
      [](base::RunLoop* run_loop,
         shopping_service::mojom::ProductInfoPtr product_info) {
        ASSERT_EQ("example_title", product_info->title);
        ASSERT_EQ("example_cluster_title", product_info->cluster_title);
        ASSERT_EQ(123u, product_info->cluster_id);
        run_loop->Quit();
      },
      &run_loop));

  run_loop.Run();
}

TEST_F(ShoppingServiceHandlerTest, TestGetProductInfoForUrl) {
  base::RunLoop run_loop;

  shopping_service_->SetIsPriceInsightsEligible(true);

  std::optional<commerce::ProductInfo> info;
  info.emplace();
  info->title = "example_title";
  info->product_cluster_title = "example_cluster_title";
  info->product_cluster_id = std::optional<uint64_t>(123u);
  shopping_service_->SetResponseForGetProductInfoForUrl(info);

  handler_->GetProductInfoForUrl(
      GURL("http://example.com/"),
      base::BindOnce(
          [](base::RunLoop* run_loop, const GURL& url,
             shopping_service::mojom::ProductInfoPtr product_info) {
            ASSERT_EQ("example_title", product_info->title);
            ASSERT_EQ("example_cluster_title", product_info->cluster_title);
            ASSERT_EQ(123u, product_info->cluster_id);
            ASSERT_EQ("http://example.com/", url.spec());
            run_loop->Quit();
          },
          &run_loop));

  run_loop.Run();
}

TEST_F(ShoppingServiceHandlerTest,
       TestGetProductInfoForCurrentUrl_FeatureIneligible) {
  base::RunLoop run_loop;

  std::optional<commerce::ProductInfo> info;
  info.emplace();
  info->title = "example_title";
  shopping_service_->SetResponseForGetProductInfoForUrl(info);
  shopping_service_->SetIsPriceInsightsEligible(false);

  handler_->GetProductInfoForCurrentUrl(base::BindOnce(
      [](base::RunLoop* run_loop,
         shopping_service::mojom::ProductInfoPtr product_info) {
        ASSERT_EQ("", product_info->title);
        ASSERT_EQ("", product_info->cluster_title);
        run_loop->Quit();
      },
      &run_loop));

  run_loop.Run();
}

TEST_F(ShoppingServiceHandlerTest, TestGetPriceInsightsInfoForCurrentUrl) {
  base::RunLoop run_loop;

  std::optional<commerce::PriceInsightsInfo> info;
  info.emplace();
  info->product_cluster_id = 123u;
  info->currency_code = "usd";
  info->typical_low_price_micros = 1230000;
  info->typical_high_price_micros = 2340000;
  info->catalog_attributes = "Unlocked, 4GB";
  info->jackpot_url = GURL("http://example.com/jackpot");
  info->price_bucket = PriceBucket::kHighPrice;
  info->has_multiple_catalogs = true;
  info->catalog_history_prices.emplace_back("2021-01-01", 3330000);
  info->catalog_history_prices.emplace_back("2021-01-02", 4440000);

  shopping_service_->SetIsPriceInsightsEligible(true);
  shopping_service_->SetResponseForGetPriceInsightsInfoForUrl(info);

  handler_->GetPriceInsightsInfoForCurrentUrl(base::BindOnce(
      [](base::RunLoop* run_loop,
         shopping_service::mojom::PriceInsightsInfoPtr info) {
        ASSERT_EQ(123u, info->cluster_id);
        ASSERT_EQ("$1.23", info->typical_low_price);
        ASSERT_EQ("$2.34", info->typical_high_price);
        ASSERT_EQ("Unlocked, 4GB", info->catalog_attributes);
        ASSERT_EQ("http://example.com/jackpot", info->jackpot.spec());
        ASSERT_EQ(
            shopping_service::mojom::PriceInsightsInfo::PriceBucket::kHigh,
            info->bucket);
        ASSERT_EQ(true, info->has_multiple_catalogs);
        ASSERT_EQ(2, (int)info->history.size());
        ASSERT_EQ("2021-01-01", info->history[0]->date);
        ASSERT_EQ(3.33f, info->history[0]->price);
        ASSERT_EQ("$3.33", info->history[0]->formatted_price);
        ASSERT_EQ("2021-01-02", info->history[1]->date);
        ASSERT_EQ(4.44f, info->history[1]->price);
        ASSERT_EQ("$4.44", info->history[1]->formatted_price);
        ASSERT_EQ("en-us", info->locale);
        ASSERT_EQ("usd", info->currency_code);
        run_loop->Quit();
      },
      &run_loop));

  run_loop.Run();
}

TEST_F(ShoppingServiceHandlerTest, TestGetUrlInfosForOpenTabs) {
  base::RunLoop run_loop;

  commerce::UrlInfo url_info;
  url_info.title = u"example_title";
  url_info.url = GURL("https://example1.com/");
  std::vector<commerce::UrlInfo> url_info_list;
  url_info_list.push_back(url_info);

  ON_CALL(*shopping_service_, GetUrlInfosForActiveWebWrappers)
      .WillByDefault(testing::Return(url_info_list));

  handler_->GetUrlInfosForOpenTabs(base::BindOnce(
      [](base::RunLoop* run_loop,
         std::vector<shopping_service::mojom::UrlInfoPtr> url_infos) {
        ASSERT_EQ("example_title", url_infos[0]->title);
        ASSERT_EQ("https://example1.com/", url_infos[0]->url.spec());
        run_loop->Quit();
      },
      &run_loop));

  run_loop.Run();
}

TEST_F(ShoppingServiceHandlerTest, TestGetUrlInfosForRecentlyViewedTabs) {
  base::RunLoop run_loop;

  commerce::UrlInfo url_info;
  url_info.title = u"title";
  url_info.url = GURL("https://example.com/");
  std::vector<commerce::UrlInfo> url_info_list;
  url_info_list.push_back(url_info);

  ON_CALL(*shopping_service_, GetUrlInfosForRecentlyViewedWebWrappers)
      .WillByDefault(testing::Return(url_info_list));

  handler_->GetUrlInfosForRecentlyViewedTabs(base::BindOnce(
      [](base::RunLoop* run_loop,
         std::vector<shopping_service::mojom::UrlInfoPtr> url_infos) {
        ASSERT_EQ("title", url_infos[0]->title);
        ASSERT_EQ("https://example.com/", url_infos[0]->url.spec());
        run_loop->Quit();
      },
      &run_loop));

  run_loop.Run();
}

TEST_F(ShoppingServiceHandlerTest, TestShowInsightsSidePanelUI) {
  EXPECT_CALL(*delegate_, ShowInsightsSidePanelUI).Times(1);

  handler_->ShowInsightsSidePanelUI();
}

TEST_F(ShoppingServiceHandlerTest, TestOpenUrlInNewTab) {
  const GURL url = GURL("http://example.com/");
  EXPECT_CALL(*delegate_, OpenUrlInNewTab(url)).Times(1);

  handler_->OpenUrlInNewTab(url);
}

TEST_F(ShoppingServiceHandlerTest, TestShowFeedback) {
  EXPECT_CALL(*delegate_, ShowFeedback).Times(1);

  handler_->ShowFeedback();
}

TEST_F(ShoppingServiceHandlerTest, TestIsShoppingListEligible) {
  base::RunLoop run_loop;
  shopping_service_->SetIsShoppingListEligible(true);

  handler_->IsShoppingListEligible(base::BindOnce(
      [](base::RunLoop* run_loop, bool eligible) {
        ASSERT_TRUE(eligible);
        run_loop->Quit();
      },
      &run_loop));

  run_loop.Run();
}

TEST_F(ShoppingServiceHandlerTest,
       TestGetPriceTrackingStatusForCurrentUrl_WithBookmark) {
  base::RunLoop run_loop;
  const GURL current_url = GURL("http://example.com/1");
  delegate_->SetCurrentTabUrl(current_url);

  ProductInfo info;
  info.product_cluster_id = 123u;
  info.title = "product";
  AddProductBookmark(bookmark_model_.get(), u"product", current_url,
                     info.product_cluster_id.value(), true, 1230000, "usd");
  shopping_service_->SetIsSubscribedCallbackValue(true);
  shopping_service_->SetResponseForGetProductInfoForUrl(info);

  EXPECT_CALL(*shopping_service_, IsSubscribed(testing::_, testing::_))
      .Times(1);

  handler_->GetPriceTrackingStatusForCurrentUrl(base::BindOnce(
      [](base::RunLoop* run_loop, bool tracked) {
        ASSERT_TRUE(tracked);
        run_loop->Quit();
      },
      &run_loop));
  run_loop.Run();
}

TEST_F(ShoppingServiceHandlerTest,
       TestGetPriceTrackingStatusForCurrentUrl_WithoutBookmark) {
  base::RunLoop run_loop;

  shopping_service_->SetSubscribeCallbackValue(false);
  shopping_service_->SetResponseForGetProductInfoForUrl(std::nullopt);
  ;

  EXPECT_CALL(*shopping_service_, IsSubscribed(testing::_, testing::_))
      .Times(0);

  handler_->GetPriceTrackingStatusForCurrentUrl(base::BindOnce(
      [](base::RunLoop* run_loop, bool tracked) {
        ASSERT_FALSE(tracked);
        run_loop->Quit();
      },
      &run_loop));

  run_loop.Run();
}

TEST_F(ShoppingServiceHandlerTest, TestTrackPriceForCurrentUrl) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  const bookmarks::BookmarkNode* product = AddProductBookmark(
      bookmark_model_.get(), u"product 1", GURL("http://example.com/1"), 123L,
      false, 1230000, "usd");
  EXPECT_CALL(*delegate_, GetOrAddBookmarkForCurrentUrl)
      .Times(1)
      .WillOnce(testing::Return(product));
  EXPECT_CALL(*shopping_service_,
              Subscribe(VectorHasSubscriptionWithId("123"), testing::_))
      .Times(1);

  handler_->SetPriceTrackingStatusForCurrentUrl(true);

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::Shopping_ShoppingAction::kEntryName);
  EXPECT_EQ(1u, entries.size());
  ukm_recorder.ExpectEntryMetric(
      entries[0], ukm::builders::Shopping_ShoppingAction::kPriceTrackedName, 1);
}

TEST_F(ShoppingServiceHandlerTest, TestUntrackPriceForCurrentUrl) {
  ProductInfo info;
  info.product_cluster_id = 123u;
  info.title = "product";
  AddProductBookmark(bookmark_model_.get(), u"product",
                     GURL("http://example.com/1"),
                     info.product_cluster_id.value(), false, 1230000, "usd");
  shopping_service_->SetIsSubscribedCallbackValue(true);
  shopping_service_->SetResponseForGetProductInfoForUrl(info);

  EXPECT_CALL(*delegate_, GetOrAddBookmarkForCurrentUrl).Times(0);
  EXPECT_CALL(*shopping_service_,
              Unsubscribe(VectorHasSubscriptionWithId("123"), testing::_))
      .Times(1);

  handler_->SetPriceTrackingStatusForCurrentUrl(false);
  base::RunLoop().RunUntilIdle();
}

TEST_F(ShoppingServiceHandlerTest,
       TestGetParentBookmarkFolderNameForCurrentUrl_NoBookmark) {
  base::RunLoop run_loop;
  handler_->GetParentBookmarkFolderNameForCurrentUrl(base::BindOnce(
      [](base::RunLoop* run_loop, const std::u16string& name) {
        ASSERT_EQ(u"", name);
        run_loop->Quit();
      },
      &run_loop));

  run_loop.Run();
}

TEST_F(ShoppingServiceHandlerTest, TestShowBookmarkEditorForCurrentUrl) {
  EXPECT_CALL(*delegate_, ShowBookmarkEditorForCurrentUrl).Times(1);

  handler_->ShowBookmarkEditorForCurrentUrl();
}

TEST_F(ShoppingServiceHandlerTest, TestGetProductSpecifications) {
  ProductSpecifications specs;
  specs.product_dimension_map[1] = "color";
  ProductSpecifications::Product product;
  product.product_cluster_id = 12345L;
  product.title = "title";
  product.product_dimension_values[1] = {"red"};
  product.summary = "summary";
  specs.products.push_back(std::move(product));

  shopping_service_->SetResponseForGetProductSpecificationsForUrls(
      std::move(specs));

  base::RunLoop run_loop;
  handler_->GetProductSpecificationsForUrls(
      {GURL("http://example.com")},
      base::BindOnce(
          [](base::RunLoop* run_loop,
             shopping_service::mojom::ProductSpecificationsPtr specs_ptr) {
            ASSERT_EQ("color", specs_ptr->product_dimension_map[1]);

            ASSERT_EQ(12345u, specs_ptr->products[0]->product_cluster_id);
            ASSERT_EQ("red",
                      specs_ptr->products[0]->product_dimension_values[1][0]);
            ASSERT_EQ("title", specs_ptr->products[0]->title);
            ASSERT_EQ("summary", specs_ptr->products[0]->summary);

            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();

  handler_->ShowBookmarkEditorForCurrentUrl();
}

TEST_F(ShoppingServiceHandlerTest, TestBookmarkNodeMoved) {
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

TEST_F(ShoppingServiceHandlerTest, TestGetAllProductSpecificationsSets) {
  const std::string uuid = base::Uuid::GenerateRandomV4().AsLowercaseString();
  std::vector<ProductSpecificationsSet> sets;
  sets.push_back(ProductSpecificationsSet(
      uuid, 0, 0, {GURL("https://example.com/")}, "set1"));
  ON_CALL(*product_spec_service_, GetAllProductSpecifications)
      .WillByDefault(testing::Return(std::move(sets)));

  base::RunLoop run_loop;
  handler_->GetAllProductSpecificationsSets(base::BindOnce(
      [](base::RunLoop* run_loop, const std::string* uuid,
         const std::vector<shopping_service::mojom::ProductSpecificationsSetPtr>
             sets_ptr) {
        ASSERT_EQ(1u, sets_ptr.size());
        const auto& set1 = sets_ptr[0];
        ASSERT_EQ("set1", set1->name);
        ASSERT_EQ(1u, set1->urls.size());
        ASSERT_EQ("https://example.com/", set1->urls[0]);
        ASSERT_EQ(*uuid, set1->uuid.AsLowercaseString());
        run_loop->Quit();
      },
      &run_loop, &uuid));

  run_loop.Run();
}

TEST_F(ShoppingServiceHandlerTest, TestGetProductSpecificationsSetByUuid) {
  const base::Uuid& uuid = base::Uuid::GenerateRandomV4();
  ProductSpecificationsSet set = ProductSpecificationsSet(
      uuid.AsLowercaseString(), 0, 0, {GURL("https://example.com/")}, "set1");
  ON_CALL(*product_spec_service_, GetSetByUuid)
      .WillByDefault(testing::Return(std::move(set)));

  base::RunLoop run_loop;
  handler_->GetProductSpecificationsSetByUuid(
      uuid,
      base::BindOnce(
          [](base::RunLoop* run_loop, const base::Uuid* uuid,
             shopping_service::mojom::ProductSpecificationsSetPtr set_ptr) {
            ASSERT_EQ(*uuid, set_ptr->uuid);
            ASSERT_EQ("set1", set_ptr->name);
            ASSERT_EQ(1u, set_ptr->urls.size());
            ASSERT_EQ("https://example.com/", set_ptr->urls[0]);
            run_loop->Quit();
          },
          &run_loop, &uuid));

  run_loop.Run();
}

TEST_F(ShoppingServiceHandlerTest, TestAddProductSpecificationsSet) {
  ProductSpecificationsSet set(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), 0, 0,
      {GURL("https://example.com/")}, "name");
  ON_CALL(*product_spec_service_, AddProductSpecificationsSet)
      .WillByDefault(testing::Return(std::move(set)));

  EXPECT_CALL(*product_spec_service_, AddProductSpecificationsSet).Times(1);

  base::RunLoop run_loop;
  handler_->AddProductSpecificationsSet(
      "name", {GURL("https://example.com/")},
      base::BindOnce(
          [](base::RunLoop* run_loop,
             shopping_service::mojom::ProductSpecificationsSetPtr spec_ptr) {
            ASSERT_EQ("name", spec_ptr->name);
            ASSERT_EQ("https://example.com/", spec_ptr->urls[0].spec());
            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();
}

TEST_F(ShoppingServiceHandlerTest, TestDeleteProductSpecificationsSet) {
  EXPECT_CALL(*product_spec_service_, DeleteProductSpecificationsSet).Times(1);

  handler_->DeleteProductSpecificationsSet(base::Uuid::GenerateRandomV4());
}

class ShoppingServiceHandlerFeatureDisableTest : public testing::Test {
 public:
  ShoppingServiceHandlerFeatureDisableTest() {
    features_.InitAndDisableFeature(kShoppingList);
  }

 protected:
  void SetUp() override {
    bookmark_model_ = bookmarks::TestBookmarkClient::CreateModel();
    account_checker_ = std::make_unique<MockAccountChecker>();
    account_checker_->SetLocale("en-us");
    shopping_service_ = std::make_unique<MockShoppingService>();
    shopping_service_->SetAccountChecker(account_checker_.get());
    handler_ = std::make_unique<commerce::ShoppingServiceHandler>(
        page_.BindAndGetRemote(),
        mojo::PendingReceiver<
            shopping_service::mojom::ShoppingServiceHandler>(),
        bookmark_model_.get(), shopping_service_.get(), pref_service_.get(),
        &tracker_, nullptr);
  }

  MockPage page_;
  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model_;
  std::unique_ptr<MockAccountChecker> account_checker_;
  std::unique_ptr<MockShoppingService> shopping_service_;
  std::unique_ptr<commerce::ShoppingServiceHandler> handler_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  feature_engagement::test::MockTracker tracker_;
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList features_;
};

TEST_F(ShoppingServiceHandlerFeatureDisableTest,
       TestGetProductInfo_FeatureDisabled) {
  shopping_service_->SetIsShoppingListEligible(false);
  EXPECT_CALL(tracker_, NotifyEvent("price_tracking_side_panel_shown"))
      .Times(0);

  const bookmarks::BookmarkNode* product = AddProductBookmark(
      bookmark_model_.get(), u"product 1", GURL("http://example.com/1"), 123L,
      true, 1230000, "usd");

  std::vector<const bookmarks::BookmarkNode*> bookmark_list;
  bookmark_list.push_back(product);
  std::vector<shopping_service::mojom::BookmarkProductInfoPtr> empty_list;

  handler_->GetAllPriceTrackedBookmarkProductInfo(base::BindOnce(
      &GetEvaluationProductInfos, base::DoNothing(), std::move(empty_list)));
}

}  // namespace
}  // namespace commerce
