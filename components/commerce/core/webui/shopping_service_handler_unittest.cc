// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/webui/shopping_service_handler.h"

#include <memory>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/uuid.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/commerce/core/commerce_constants.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/commerce_utils.h"
#include "components/commerce/core/mock_account_checker.h"
#include "components/commerce/core/mock_shopping_service.h"
#include "components/commerce/core/mojom/shared.mojom.h"
#include "components/commerce/core/mojom/shopping_service.mojom.h"
#include "components/commerce/core/pref_names.h"
#include "components/commerce/core/price_tracking_utils.h"
#include "components/commerce/core/subscriptions/commerce_subscription.h"
#include "components/commerce/core/test_utils.h"
#include "components/feature_engagement/test/mock_tracker.h"
#include "components/optimization_guide/core/model_quality/test_model_quality_logs_uploader_service.h"
#include "components/optimization_guide/core/optimization_guide_prefs.h"
#include "components/power_bookmarks/core/power_bookmark_utils.h"
#include "components/power_bookmarks/core/proto/power_bookmark_meta.pb.h"
#include "components/power_bookmarks/core/proto/shopping_specifics.pb.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync/test/mock_data_type_local_change_processor.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace commerce {
namespace {

const std::string kTestUrl1 = "http://www.example.com/1";
const std::string kTestUrl2 = "http://www.example.com/2";

class MockDelegate : public ShoppingServiceHandler::Delegate {
 public:
  MockDelegate() {
    SetCurrentTabUrl(GURL("http://example.com"));
    SetCurrentTabUkmSourceId(123);
  }
  ~MockDelegate() override = default;

  MOCK_METHOD(std::optional<GURL>, GetCurrentTabUrl, (), (override));
  MOCK_METHOD(void, OpenUrlInNewTab, (const GURL& url), (override));
  MOCK_METHOD(void, SwitchToOrOpenTab, (const GURL& url), (override));
  MOCK_METHOD(const bookmarks::BookmarkNode*,
              GetOrAddBookmarkForCurrentUrl,
              (),
              (override));
  MOCK_METHOD(ukm::SourceId, GetCurrentTabUkmSourceId, (), (override));

  void SetCurrentTabUrl(const GURL& url) {
    ON_CALL(*this, GetCurrentTabUrl)
        .WillByDefault(testing::Return(std::make_optional<GURL>(url)));
  }

  void SetCurrentTabUkmSourceId(ukm::SourceId id) {
    ON_CALL(*this, GetCurrentTabUkmSourceId).WillByDefault(testing::Return(id));
  }
};

class ShoppingServiceHandlerTest : public testing::Test {
 public:
  ShoppingServiceHandlerTest() : logs_uploader_(&local_state_) {}

 protected:
  void SetUp() override {
    auto client = std::make_unique<bookmarks::TestBookmarkClient>();
    client->SetIsSyncFeatureEnabledIncludingBookmarks(true);
    bookmark_model_ =
        bookmarks::TestBookmarkClient::CreateModelWithClient(std::move(client));
    account_checker_ = std::make_unique<MockAccountChecker>();
    account_checker_->SetLocale("en-us");
    shopping_service_ = std::make_unique<MockShoppingService>();
    shopping_service_->SetAccountChecker(account_checker_.get());
    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    account_checker_->SetPrefs(pref_service_.get());
    MockAccountChecker::RegisterCommercePrefs(pref_service_->registry());
    SetTabCompareEnterprisePolicyPref(pref_service_.get(), 0);
    SetShoppingListEnterprisePolicyPref(pref_service_.get(), true);

    auto delegate = std::make_unique<MockDelegate>();
    delegate_ = delegate.get();
    handler_ = std::make_unique<commerce::ShoppingServiceHandler>(
        mojo::PendingReceiver<
            shopping_service::mojom::ShoppingServiceHandler>(),
        bookmark_model_.get(), shopping_service_.get(), pref_service_.get(),
        &tracker_, std::move(delegate), &logs_uploader_);
  }

  TestingPrefServiceSimple local_state_;
  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model_;
  std::unique_ptr<MockAccountChecker> account_checker_;
  std::unique_ptr<MockShoppingService> shopping_service_;
  std::unique_ptr<commerce::ShoppingServiceHandler> handler_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  optimization_guide::TestModelQualityLogsUploaderService logs_uploader_;
  raw_ptr<MockDelegate> delegate_;
  feature_engagement::test::MockTracker tracker_;
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList features_;
};

std::optional<ProductInfo> BuildProductInfoWithPriceSummary(
    uint64_t price,
    uint64_t price_lowest,
    uint64_t price_highest) {
  std::optional<commerce::ProductInfo> info;
  info.emplace();
  info->currency_code = "usd";
  info->amount_micros = price;

  PriceSummary summary;
  summary.set_is_preferred(true);
  summary.mutable_lowest_price()->set_currency_code("usd");
  summary.mutable_lowest_price()->set_amount_micros(price_lowest);
  summary.mutable_highest_price()->set_currency_code("usd");
  summary.mutable_highest_price()->set_amount_micros(price_highest);
  info->price_summary.push_back(std::move(summary));

  return info;
}

TEST_F(ShoppingServiceHandlerTest,
       TestGetProductInfoForCurrentUrl_FeatureEligible) {
  base::RunLoop run_loop;

  commerce::SetUpPriceInsightsEligibility(&features_, account_checker_.get(),
                                          true);

  std::optional<commerce::ProductInfo> info;
  info.emplace();
  info->title = "example_title";
  info->product_cluster_title = "example_cluster_title";
  info->product_cluster_id = std::optional<uint64_t>(123u);
  shopping_service_->SetResponseForGetProductInfoForUrl(info);

  handler_->GetProductInfoForCurrentUrl(base::BindOnce(
      [](base::RunLoop* run_loop, shared::mojom::ProductInfoPtr product_info) {
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

  commerce::SetUpPriceInsightsEligibility(&features_, account_checker_.get(),
                                          true);
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
             shared::mojom::ProductInfoPtr product_info) {
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
  commerce::SetUpPriceInsightsEligibility(&features_, account_checker_.get(),
                                          false);

  handler_->GetProductInfoForCurrentUrl(base::BindOnce(
      [](base::RunLoop* run_loop, shared::mojom::ProductInfoPtr product_info) {
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

  commerce::SetUpPriceInsightsEligibility(&features_, account_checker_.get(),
                                          true);
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
        ASSERT_EQ(2u, info->history.size());
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

TEST_F(ShoppingServiceHandlerTest, TestGetPriceInsightsInfoForUrl) {
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

  commerce::SetUpPriceInsightsEligibility(&features_, account_checker_.get(),
                                          true);
  shopping_service_->SetResponseForGetPriceInsightsInfoForUrl(info);

  handler_->GetPriceInsightsInfoForUrl(
      GURL("http://example.com/"),
      base::BindOnce(
          [](base::RunLoop* run_loop, const GURL& url,
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
            ASSERT_EQ(2u, info->history.size());
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

TEST_F(ShoppingServiceHandlerTest,
       TestGetPriceInsightsInfoForUrlWhenNotPriceInsightsEligible) {
  base::RunLoop run_loop;

  std::optional<commerce::PriceInsightsInfo> info;
  info.emplace();
  info->product_cluster_id = 123u;

  commerce::SetUpPriceInsightsEligibility(&features_, account_checker_.get(),
                                          false);
  shopping_service_->SetResponseForGetPriceInsightsInfoForUrl(info);

  handler_->GetPriceInsightsInfoForUrl(
      GURL("http://example.com/"),
      base::BindOnce(
          [](base::RunLoop* run_loop, const GURL& url,
             shopping_service::mojom::PriceInsightsInfoPtr info) {
            ASSERT_NE(123u, info->cluster_id);
            run_loop->Quit();
          },
          &run_loop));

  run_loop.Run();
}

TEST_F(ShoppingServiceHandlerTest, TestGetUrlInfosForProductTabs) {
  base::RunLoop run_loop;

  commerce::UrlInfo url_info;
  url_info.title = u"example_title";
  url_info.url = GURL("https://example1.com/");
  url_info.previewText = "URL text";
  std::vector<commerce::UrlInfo> url_info_list;
  url_info_list.push_back(url_info);

  ON_CALL(*shopping_service_, GetUrlInfosForWebWrappersWithProducts)
      .WillByDefault(
          [url_info_list](
              base::OnceCallback<void(std::vector<commerce::UrlInfo>)>
                  callback) { std::move(callback).Run(url_info_list); });

  handler_->GetUrlInfosForProductTabs(
      base::BindOnce([](std::vector<shopping_service::mojom::UrlInfoPtr>
                            url_infos) {
        ASSERT_EQ("example_title", url_infos[0]->title);
        ASSERT_EQ("https://example1.com/", url_infos[0]->url.spec());
        ASSERT_EQ("URL text", url_infos[0]->previewText);
      }).Then(run_loop.QuitClosure()));

  run_loop.Run();
}

TEST_F(ShoppingServiceHandlerTest, TestGetUrlInfosForRecentlyViewedTabs) {
  base::RunLoop run_loop;

  commerce::UrlInfo url_info;
  url_info.title = u"title";
  url_info.url = GURL("https://example.com/");
  url_info.previewText = "URL text";
  std::vector<commerce::UrlInfo> url_info_list;
  url_info_list.push_back(url_info);

  ON_CALL(*shopping_service_, GetUrlInfosForRecentlyViewedWebWrappers)
      .WillByDefault(testing::Return(url_info_list));

  handler_->GetUrlInfosForRecentlyViewedTabs(base::BindOnce(
      [](base::RunLoop* run_loop,
         std::vector<shopping_service::mojom::UrlInfoPtr> url_infos) {
        ASSERT_EQ("title", url_infos[0]->title);
        ASSERT_EQ("https://example.com/", url_infos[0]->url.spec());
        ASSERT_EQ("URL text", url_infos[0]->previewText);
        run_loop->Quit();
      },
      &run_loop));

  run_loop.Run();
}

TEST_F(ShoppingServiceHandlerTest, TestOpenUrlInNewTab) {
  const GURL url = GURL("http://example.com/");
  EXPECT_CALL(*delegate_, OpenUrlInNewTab(url)).Times(1);

  handler_->OpenUrlInNewTab(url);
}

TEST_F(ShoppingServiceHandlerTest, TestSwitchToOrOpenTab) {
  const GURL url = GURL("http://example.com/");
  EXPECT_CALL(*delegate_, SwitchToOrOpenTab(url)).Times(1);

  handler_->SwitchToOrOpenTab(url);
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

TEST_F(ShoppingServiceHandlerTest, TestProductInfoPriceSummary_ShowRange) {
  std::optional<commerce::ProductInfo> info =
      BuildProductInfoWithPriceSummary(150000000, 100000000, 200000000);
  info->price_display_recommendation =
      BuyableProduct_PriceDisplayRecommendation::
          BuyableProduct_PriceDisplayRecommendation_RECOMMENDATION_SHOW_RANGE;
  shopping_service_->SetResponseForGetProductInfoForUrl(info);

  base::RunLoop run_loop;
  handler_->GetProductInfoForUrl(
      GURL(), base::BindOnce([](const GURL& url,
                                shared::mojom::ProductInfoPtr product_info) {
                ASSERT_EQ("$100.00 - $200.00", product_info->price_summary);
              }).Then(run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(ShoppingServiceHandlerTest,
       TestProductInfoPriceSummary_ShowRangeLowerBound) {
  std::optional<commerce::ProductInfo> info =
      BuildProductInfoWithPriceSummary(150000000, 100000000, 200000000);
  info->price_display_recommendation = BuyableProduct_PriceDisplayRecommendation::
      BuyableProduct_PriceDisplayRecommendation_RECOMMENDATION_SHOW_RANGE_LOWER_BOUND;
  shopping_service_->SetResponseForGetProductInfoForUrl(info);

  base::RunLoop run_loop;
  handler_->GetProductInfoForUrl(
      GURL(), base::BindOnce([](const GURL& url,
                                shared::mojom::ProductInfoPtr product_info) {
                ASSERT_EQ("$100.00+", product_info->price_summary);
              }).Then(run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(ShoppingServiceHandlerTest,
       TestProductInfoPriceSummary_ShowRangeUpperBound) {
  std::optional<commerce::ProductInfo> info =
      BuildProductInfoWithPriceSummary(150000000, 100000000, 200000000);
  info->price_display_recommendation = BuyableProduct_PriceDisplayRecommendation::
      BuyableProduct_PriceDisplayRecommendation_RECOMMENDATION_SHOW_RANGE_UPPER_BOUND;
  shopping_service_->SetResponseForGetProductInfoForUrl(info);

  base::RunLoop run_loop;
  handler_->GetProductInfoForUrl(
      GURL(), base::BindOnce([](const GURL& url,
                                shared::mojom::ProductInfoPtr product_info) {
                ASSERT_EQ("$200.00", product_info->price_summary);
              }).Then(run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(ShoppingServiceHandlerTest, TestProductInfoPriceSummary_Unspecified) {
  std::optional<commerce::ProductInfo> info =
      BuildProductInfoWithPriceSummary(150000000, 100000000, 200000000);
  info->price_display_recommendation = BuyableProduct_PriceDisplayRecommendation::
      BuyableProduct_PriceDisplayRecommendation_RECOMMENDATION_SHOW_PRICE_UNDETERMINED;
  shopping_service_->SetResponseForGetProductInfoForUrl(info);

  base::RunLoop run_loop;
  handler_->GetProductInfoForUrl(
      GURL(), base::BindOnce([](const GURL& url,
                                shared::mojom::ProductInfoPtr product_info) {
                ASSERT_EQ("-", product_info->price_summary);
              }).Then(run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(ShoppingServiceHandlerTest, TestProductInfoPriceSummary_SinglePrice) {
  std::optional<commerce::ProductInfo> info =
      BuildProductInfoWithPriceSummary(150000000, 100000000, 200000000);
  info->price_display_recommendation = BuyableProduct_PriceDisplayRecommendation::
      BuyableProduct_PriceDisplayRecommendation_RECOMMENDATION_SHOW_SINGLE_PRICE;
  shopping_service_->SetResponseForGetProductInfoForUrl(info);

  base::RunLoop run_loop;
  handler_->GetProductInfoForUrl(
      GURL(), base::BindOnce([](const GURL& url,
                                shared::mojom::ProductInfoPtr product_info) {
                ASSERT_EQ("$150.00", product_info->price_summary);
              }).Then(run_loop.QuitClosure()));
  run_loop.Run();
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
        mojo::PendingReceiver<
            shopping_service::mojom::ShoppingServiceHandler>(),
        bookmark_model_.get(), shopping_service_.get(), pref_service_.get(),
        &tracker_, nullptr, nullptr);
  }

  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model_;
  std::unique_ptr<MockAccountChecker> account_checker_;
  std::unique_ptr<MockShoppingService> shopping_service_;
  std::unique_ptr<commerce::ShoppingServiceHandler> handler_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  feature_engagement::test::MockTracker tracker_;
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList features_;
};

}  // namespace
}  // namespace commerce
