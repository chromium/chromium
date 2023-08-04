// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/shopping_service.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/mock_account_checker.h"
#include "components/commerce/core/pref_names.h"
#include "components/commerce/core/shopping_service_test_base.h"
#include "components/commerce/core/test_utils.h"
#include "components/optimization_guide/core/new_optimization_guide_decider.h"
#include "components/optimization_guide/core/optimization_guide_decision.h"
#include "components/optimization_guide/core/optimization_metadata.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/power_bookmarks/core/power_bookmark_utils.h"
#include "components/power_bookmarks/core/proto/power_bookmark_meta.pb.h"
#include "components/power_bookmarks/core/proto/shopping_specifics.pb.h"
#include "components/prefs/testing_pref_service.h"
#include "components/search/ntp_features.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/gtest/include/gtest/gtest.h"

using optimization_guide::OptimizationGuideDecision;
using optimization_guide::OptimizationGuideDecisionCallback;
using optimization_guide::OptimizationMetadata;
using optimization_guide::proto::Any;
using optimization_guide::proto::OptimizationType;

namespace commerce {

namespace {
const char kProductUrl[] = "http://example.com/";
const char kTitle[] = "product title";
const char kGpcTitle[] = "product gpc title";
const char kImageUrl[] = "http://example.com/image.png";
const uint64_t kOfferId = 123;
const uint64_t kClusterId = 456;
const char kCountryCode[] = "US";
const char kCurrencyCode[] = "USD";
const int64_t kPrice = 1000;
const int64_t kNewPrice = 500;

const char kMerchantUrl[] = "http://example.com/merchant";
const float kStarRating = 4.5;
const uint32_t kCountRating = 1000;
const char kDetailsPageUrl[] = "http://example.com/merchant_details_page";
const bool kHasReturnPolicy = true;
const bool kContainsSensitiveContent = false;

const char kEligibleCountry[] = "US";
const char kEligibleLocale[] = "en-us";

const char kPriceInsightsUrl[] = "http://example.com/price_insight";
const int64_t kLowTypicalPrice = 2000;
const int64_t kHighTypicalPrice = 3000;
const char kAnotherCurrencyCode[] = "EUR";
const char kAttributes[] = "Unlocked, 128GB";
const char kJackpotUrl[] = "http://example.com/jackpot";

}  // namespace

class ShoppingServiceTest : public ShoppingServiceTestBase {
 public:
  ShoppingServiceTest() = default;
  ShoppingServiceTest(const ShoppingServiceTest&) = delete;
  ShoppingServiceTest operator=(const ShoppingServiceTest&) = delete;
  ~ShoppingServiceTest() override = default;

  // Expose the private feature check for testing.
  static bool IsShoppingListEligible(AccountChecker* account_checker,
                                     PrefService* prefs,
                                     const std::string& country,
                                     const std::string& locale) {
    return ShoppingService::IsShoppingListEligible(account_checker, prefs,
                                                   country, locale);
  }
};

// Test that product info is processed correctly.
TEST_F(ShoppingServiceTest, TestProductInfoResponse) {
  // Ensure a feature that uses product info is enabled. This doesn't
  // necessarily need to be the shopping list.
  test_features_.InitWithFeatures(
      {commerce::kShoppingList, commerce::kCommerceAllowServerImages}, {});

  OptimizationMetadata meta = opt_guide_->BuildPriceTrackingResponse(
      kTitle, kImageUrl, kOfferId, kClusterId, kCountryCode, kPrice,
      kCurrencyCode, kGpcTitle);
  opt_guide_->AddPriceUpdateToPriceTrackingResponse(&meta, kCurrencyCode,
                                                    kNewPrice, kPrice);

  opt_guide_->SetResponse(GURL(kProductUrl), OptimizationType::PRICE_TRACKING,
                          OptimizationGuideDecision::kTrue, meta);

  base::RunLoop run_loop;
  shopping_service_->GetProductInfoForUrl(
      GURL(kProductUrl),
      base::BindOnce(
          [](base::RunLoop* run_loop, const GURL& url,
             const absl::optional<ProductInfo>& info) {
            ASSERT_EQ(kProductUrl, url.spec());
            ASSERT_TRUE(info.has_value());

            ASSERT_EQ(kTitle, info->title);
            ASSERT_EQ(kGpcTitle, info->product_cluster_title);
            ASSERT_EQ(kImageUrl, info->image_url);
            ASSERT_EQ(kOfferId, info->offer_id);
            ASSERT_EQ(kClusterId, info->product_cluster_id);
            ASSERT_EQ(kCountryCode, info->country_code);

            ASSERT_EQ(kCurrencyCode, info->currency_code);
            ASSERT_EQ(kNewPrice, info->amount_micros);
            ASSERT_TRUE(info->previous_amount_micros.has_value());
            ASSERT_EQ(kPrice, info->previous_amount_micros.value());

            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();
}

// Test that the product info api fails gracefully (callback run with nullopt)
// if it is disabled.
TEST_F(ShoppingServiceTest, TestProductInfoResponse_ApiDisabled) {
  // Ensure a feature that uses product info is disabled.
  test_features_.InitWithFeatures({},
                                  {kShoppingList, kShoppingListRegionLaunched,
                                   ntp_features::kNtpChromeCartModule});

  base::RunLoop run_loop;
  shopping_service_->GetProductInfoForUrl(
      GURL(kProductUrl), base::BindOnce(
                             [](base::RunLoop* run_loop, const GURL& url,
                                const absl::optional<ProductInfo>& info) {
                               ASSERT_EQ(kProductUrl, url.spec());
                               ASSERT_FALSE(info.has_value());
                               run_loop->Quit();
                             },
                             &run_loop));
  run_loop.Run();
}

TEST_F(ShoppingServiceTest, TestProductInfoResponse_CurrencyMismatch) {
  // Ensure a feature that uses product info is enabled. This doesn't
  // necessarily need to be the shopping list.
  test_features_.InitWithFeatures(
      {commerce::kShoppingList, commerce::kCommerceAllowServerImages}, {});

  OptimizationMetadata meta = opt_guide_->BuildPriceTrackingResponse(
      kTitle, kImageUrl, kOfferId, kClusterId, kCountryCode, kPrice,
      kCurrencyCode);

  // Add a fake currency code to that doesn't match the original to ensure that
  // data is not used.
  opt_guide_->AddPriceUpdateToPriceTrackingResponse(&meta, "ZZ", kNewPrice,
                                                    kPrice);

  opt_guide_->SetResponse(GURL(kProductUrl), OptimizationType::PRICE_TRACKING,
                          OptimizationGuideDecision::kTrue, meta);

  base::RunLoop run_loop;
  shopping_service_->GetProductInfoForUrl(
      GURL(kProductUrl),
      base::BindOnce(
          [](base::RunLoop* run_loop, const GURL& url,
             const absl::optional<ProductInfo>& info) {
            ASSERT_EQ(kProductUrl, url.spec());
            ASSERT_TRUE(info.has_value());

            ASSERT_EQ(kTitle, info->title);
            ASSERT_EQ(kImageUrl, info->image_url);
            ASSERT_EQ(kOfferId, info->offer_id);
            ASSERT_EQ(kClusterId, info->product_cluster_id);
            ASSERT_EQ(kCountryCode, info->country_code);

            ASSERT_EQ(kCurrencyCode, info->currency_code);
            ASSERT_EQ(kPrice, info->amount_micros);
            ASSERT_FALSE(info->previous_amount_micros.has_value());

            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();
}

// Test that no object is provided for a negative optimization guide response.
TEST_F(ShoppingServiceTest, TestProductInfoResponse_OptGuideFalse) {
  test_features_.InitWithFeatures(
      {kShoppingList, kCommerceAllowLocalImages, kCommerceAllowServerImages},
      {});

  opt_guide_->SetResponse(GURL(kProductUrl), OptimizationType::PRICE_TRACKING,
                          OptimizationGuideDecision::kFalse,
                          OptimizationMetadata());

  base::RunLoop run_loop;
  shopping_service_->GetProductInfoForUrl(
      GURL(kProductUrl), base::BindOnce(
                             [](base::RunLoop* run_loop, const GURL& url,
                                const absl::optional<ProductInfo>& info) {
                               ASSERT_EQ(kProductUrl, url.spec());
                               ASSERT_FALSE(info.has_value());
                               run_loop->Quit();
                             },
                             &run_loop));
  run_loop.Run();
}

// Test that the product info cache only keeps track of live tabs.
TEST_F(ShoppingServiceTest, TestProductInfoCacheURLCount) {
  test_features_.InitWithFeatures(
      {kShoppingList, kCommerceAllowLocalImages, kCommerceAllowServerImages},
      {});

  std::string url = "http://example.com/foo";
  MockWebWrapper web1(GURL(url), false);
  MockWebWrapper web2(GURL(url), false);

  std::string url2 = "http://example.com/bar";
  MockWebWrapper web3(GURL(url2), false);

  // Ensure navigating to then navigating away clears the cache item.
  ASSERT_EQ(0, GetProductInfoCacheOpenURLCount(GURL(url)));
  DidNavigatePrimaryMainFrame(&web1);
  ASSERT_EQ(1, GetProductInfoCacheOpenURLCount(GURL(url)));
  DidNavigateAway(&web1, GURL(url));
  ASSERT_EQ(0, GetProductInfoCacheOpenURLCount(GURL(url)));

  // Ensure navigating to a URL in multiple "tabs" retains the cached item until
  // both are navigated away.
  ASSERT_EQ(0, GetProductInfoCacheOpenURLCount(GURL(url)));
  DidNavigatePrimaryMainFrame(&web1);
  DidNavigatePrimaryMainFrame(&web2);
  ASSERT_EQ(2, GetProductInfoCacheOpenURLCount(GURL(url)));
  DidNavigateAway(&web1, GURL(url));
  ASSERT_EQ(1, GetProductInfoCacheOpenURLCount(GURL(url)));
  DidNavigateAway(&web2, GURL(url));
  ASSERT_EQ(0, GetProductInfoCacheOpenURLCount(GURL(url)));

  // Make sure more than one entry can be in the cache.
  ASSERT_EQ(0, GetProductInfoCacheOpenURLCount(GURL(url)));
  ASSERT_EQ(0, GetProductInfoCacheOpenURLCount(GURL(url2)));
  DidNavigatePrimaryMainFrame(&web1);
  DidNavigatePrimaryMainFrame(&web3);
  ASSERT_EQ(1, GetProductInfoCacheOpenURLCount(GURL(url)));
  ASSERT_EQ(1, GetProductInfoCacheOpenURLCount(GURL(url2)));

  // Simulate closing the browser to ensure the cache is emptied.
  WebWrapperDestroyed(&web1);
  WebWrapperDestroyed(&web2);
  WebWrapperDestroyed(&web3);

  ASSERT_EQ(0, GetProductInfoCacheOpenURLCount(GURL(url)));
  ASSERT_EQ(0, GetProductInfoCacheOpenURLCount(GURL(url2)));
}

// Test that product info is inserted into the cache without a client
// necessarily querying for it.
TEST_F(ShoppingServiceTest, TestProductInfoCacheFullLifecycle) {
  test_features_.InitWithFeatures(
      {kShoppingList, kCommerceAllowLocalImages, kCommerceAllowServerImages},
      {});

  MockWebWrapper web(GURL(kProductUrl), false);

  OptimizationMetadata meta = opt_guide_->BuildPriceTrackingResponse(
      kTitle, kImageUrl, kOfferId, kClusterId, kCountryCode);

  opt_guide_->SetResponse(GURL(kProductUrl), OptimizationType::PRICE_TRACKING,
                          OptimizationGuideDecision::kTrue, meta);

  DidNavigatePrimaryMainFrame(&web);

  // By this point there should be something in the cache.
  ASSERT_EQ(1, GetProductInfoCacheOpenURLCount(GURL(kProductUrl)));

  // We should be able to access the cached data.
  absl::optional<ProductInfo> cached_info =
      shopping_service_->GetAvailableProductInfoForUrl(GURL(kProductUrl));
  ASSERT_EQ(kTitle, cached_info->title);
  ASSERT_EQ(kImageUrl, cached_info->image_url);
  ASSERT_EQ(kOfferId, cached_info->offer_id);
  ASSERT_EQ(kClusterId, cached_info->product_cluster_id);
  ASSERT_EQ(kCountryCode, cached_info->country_code);

  // The main API should still work.
  base::RunLoop run_loop;
  shopping_service_->GetProductInfoForUrl(
      GURL(kProductUrl), base::BindOnce(
                             [](base::RunLoop* run_loop, const GURL& url,
                                const absl::optional<ProductInfo>& info) {
                               ASSERT_EQ(kProductUrl, url.spec());
                               ASSERT_TRUE(info.has_value());

                               ASSERT_EQ(kTitle, info->title);
                               ASSERT_EQ(kImageUrl, info->image_url);
                               ASSERT_EQ(kOfferId, info->offer_id);
                               ASSERT_EQ(kClusterId, info->product_cluster_id);
                               ASSERT_EQ(kCountryCode, info->country_code);
                               run_loop->Quit();
                             },
                             &run_loop));
  run_loop.Run();

  // Close the "tab" and make sure the cache is empty.
  WebWrapperDestroyed(&web);
  ASSERT_EQ(0, GetProductInfoCacheOpenURLCount(GURL(kProductUrl)));
}

// Test the full lifecycle of product info assuming the page loads after
// optimization guide has provided a response.
TEST_F(ShoppingServiceTest,
       TestProductInfoCacheFullLifecycleWithFallback_PageNotLoaded) {
  test_features_.InitWithFeatures(
      {kShoppingList, kCommerceAllowLocalImages, kCommerceAllowServerImages},
      {});

  std::string json("{\"image\": \"" + std::string(kImageUrl) + "\"}");
  base::Value js_result(json);
  MockWebWrapper web(GURL(kProductUrl), false, &js_result);

  // Assume the page hasn't finished loading.
  web.SetIsFirstLoadForNavigationFinished(false);

  // Intentionally exclude the image URL to ensure the javascript fallback
  // works.
  OptimizationMetadata meta = opt_guide_->BuildPriceTrackingResponse(
      kTitle, "", kOfferId, kClusterId, kCountryCode);

  opt_guide_->SetResponse(GURL(kProductUrl), OptimizationType::PRICE_TRACKING,
                          OptimizationGuideDecision::kTrue, meta);

  DidNavigatePrimaryMainFrame(&web);

  // By this point there should be something in the cache.
  ASSERT_EQ(1, GetProductInfoCacheOpenURLCount(GURL(kProductUrl)));

  // We should be able to access the cached data.
  absl::optional<ProductInfo> cached_info =
      shopping_service_->GetAvailableProductInfoForUrl(GURL(kProductUrl));
  ASSERT_EQ(kTitle, cached_info->title);
  ASSERT_EQ("", cached_info->image_url);
  ASSERT_EQ(kOfferId, cached_info->offer_id);
  ASSERT_EQ(kClusterId, cached_info->product_cluster_id);
  ASSERT_EQ(kCountryCode, cached_info->country_code);

  // The main API should still work.
  base::RunLoop run_loop;
  shopping_service_->GetProductInfoForUrl(
      GURL(kProductUrl), base::BindOnce(
                             [](base::RunLoop* run_loop, const GURL& url,
                                const absl::optional<ProductInfo>& info) {
                               ASSERT_EQ(kProductUrl, url.spec());
                               ASSERT_TRUE(info.has_value());

                               ASSERT_EQ(kTitle, info->title);
                               ASSERT_EQ("", info->image_url);
                               ASSERT_EQ(kOfferId, info->offer_id);
                               ASSERT_EQ(kClusterId, info->product_cluster_id);
                               ASSERT_EQ(kCountryCode, info->country_code);
                               run_loop->Quit();
                             },
                             &run_loop));
  run_loop.Run();

  // The page will have finished its initial load prior to DidFinishLoad.
  web.SetIsFirstLoadForNavigationFinished(true);
  DidFinishLoad(&web);
  // The js should only be able to run now after all loading has completed (for
  // at least the timeout duration).
  SimulateProductInfoJsTaskFinished();

  // At this point we should have the image in the cache.
  cached_info =
      shopping_service_->GetAvailableProductInfoForUrl(GURL(kProductUrl));
  ASSERT_EQ(kImageUrl, cached_info->image_url.spec());

  // Close the "tab" and make sure the cache is empty.
  WebWrapperDestroyed(&web);
  ASSERT_EQ(0, GetProductInfoCacheOpenURLCount(GURL(kProductUrl)));
}

// Test the full lifecycle of product info assuming the page has loaded prior
// to optimization guide providing a response. This will happen for single-page
// webapps.
TEST_F(ShoppingServiceTest,
       TestProductInfoCacheFullLifecycleWithFallback_PageLoaded) {
  test_features_.InitWithFeatures(
      {kCommerceAllowLocalImages, kCommerceAllowServerImages}, {});

  std::string json("{\"image\": \"" + std::string(kImageUrl) + "\"}");
  base::Value js_result(json);
  MockWebWrapper web(GURL(kProductUrl), false, &js_result);

  // Assume the page has already loaded for the navigation. This is usually the
  // case for single-page webapps.
  web.SetIsFirstLoadForNavigationFinished(true);

  // Intentionally exclude the image URL to ensure the javascript fallback
  // works.
  OptimizationMetadata meta = opt_guide_->BuildPriceTrackingResponse(
      kTitle, "", kOfferId, kClusterId, kCountryCode);

  opt_guide_->SetResponse(GURL(kProductUrl), OptimizationType::PRICE_TRACKING,
                          OptimizationGuideDecision::kTrue, meta);

  DidNavigatePrimaryMainFrame(&web);
  // If the page was already loaded, assume the js has time to run now.
  SimulateProductInfoJsTaskFinished();

  // By this point there should be something in the cache.
  ASSERT_EQ(1, GetProductInfoCacheOpenURLCount(GURL(kProductUrl)));

  // We should be able to access the cached data.
  absl::optional<ProductInfo> cached_info =
      shopping_service_->GetAvailableProductInfoForUrl(GURL(kProductUrl));
  ASSERT_EQ(kTitle, cached_info->title);
  // Since the fallback will run immediately, we should have a populated image
  // URL.
  ASSERT_EQ(kImageUrl, cached_info->image_url);
  ASSERT_EQ(kOfferId, cached_info->offer_id);
  ASSERT_EQ(kClusterId, cached_info->product_cluster_id);
  ASSERT_EQ(kCountryCode, cached_info->country_code);

  // The main API should still work.
  base::RunLoop run_loop;
  shopping_service_->GetProductInfoForUrl(
      GURL(kProductUrl), base::BindOnce(
                             [](base::RunLoop* run_loop, const GURL& url,
                                const absl::optional<ProductInfo>& info) {
                               ASSERT_EQ(kProductUrl, url.spec());
                               ASSERT_TRUE(info.has_value());

                               ASSERT_EQ(kTitle, info->title);
                               ASSERT_EQ(kImageUrl, info->image_url);
                               ASSERT_EQ(kOfferId, info->offer_id);
                               ASSERT_EQ(kClusterId, info->product_cluster_id);
                               ASSERT_EQ(kCountryCode, info->country_code);
                               run_loop->Quit();
                             },
                             &run_loop));
  run_loop.Run();

  // Close the "tab" and make sure the cache is empty.
  WebWrapperDestroyed(&web);
  ASSERT_EQ(0, GetProductInfoCacheOpenURLCount(GURL(kProductUrl)));
}

// Test that merchant info is processed correctly.
TEST_F(ShoppingServiceTest, TestMerchantInfoResponse) {
  // Ensure a feature that uses merchant info is enabled.
  test_features_.InitAndEnableFeature(kCommerceMerchantViewer);

  OptimizationMetadata meta = opt_guide_->BuildMerchantTrustResponse(
      kStarRating, kCountRating, kDetailsPageUrl, kHasReturnPolicy,
      kContainsSensitiveContent);

  opt_guide_->SetResponse(GURL(kMerchantUrl),
                          OptimizationType::MERCHANT_TRUST_SIGNALS_V2,
                          OptimizationGuideDecision::kTrue, meta);

  base::RunLoop run_loop;
  shopping_service_->GetMerchantInfoForUrl(
      GURL(kMerchantUrl),
      base::BindOnce(
          [](base::RunLoop* run_loop, const GURL& url,
             absl::optional<MerchantInfo> info) {
            ASSERT_EQ(kMerchantUrl, url.spec());
            ASSERT_TRUE(info.has_value());

            ASSERT_EQ(kStarRating, info->star_rating);
            ASSERT_EQ(kCountRating, info->count_rating);
            ASSERT_EQ(kDetailsPageUrl, info->details_page_url.spec());
            ASSERT_EQ(kHasReturnPolicy, info->has_return_policy);
            ASSERT_EQ(kContainsSensitiveContent,
                      info->contains_sensitive_content);
            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();
}

// Test that the merchant info fails gracefully when the api is disabled.
TEST_F(ShoppingServiceTest, TestMerchantInfoResponse_ApiDisabled) {
  // Ensure a feature that uses merchant info is disabled.
  test_features_.InitAndDisableFeature(kCommerceMerchantViewer);

  base::RunLoop run_loop;
  shopping_service_->GetMerchantInfoForUrl(
      GURL(kMerchantUrl), base::BindOnce(
                              [](base::RunLoop* run_loop, const GURL& url,
                                 absl::optional<MerchantInfo> info) {
                                ASSERT_EQ(kMerchantUrl, url.spec());
                                ASSERT_FALSE(info.has_value());
                                run_loop->Quit();
                              },
                              &run_loop));
  run_loop.Run();
}

TEST_F(ShoppingServiceTest, TestGetUpdatedProductInfoForBookmarks) {
  const bookmarks::BookmarkNode* product1 = AddProductBookmark(
      bookmark_model_.get(), u"title", GURL(kProductUrl), kClusterId, false);

  OptimizationMetadata updated_meta = opt_guide_->BuildPriceTrackingResponse(
      kTitle, "", kOfferId, kClusterId, kCountryCode);
  opt_guide_->AddOnDemandShoppingResponse(
      GURL(kProductUrl), OptimizationGuideDecision::kTrue, updated_meta);

  std::vector<int64_t> bookmark_ids;
  bookmark_ids.push_back(product1->id());
  int expected_calls = bookmark_ids.size();

  base::RunLoop run_loop;

  auto callback = base::BindRepeating(
      [](bookmarks::BookmarkModel* model, int* call_count,
         base::RunLoop* run_loop, const int64_t id, const GURL& url,
         absl::optional<ProductInfo> info) {
        const bookmarks::BookmarkNode* node =
            bookmarks::GetBookmarkNodeByID(model, id);
        EXPECT_EQ(url.spec(), node->url().spec());

        (*call_count)--;
        if (*call_count <= 0)
          run_loop->Quit();
      },
      bookmark_model_.get(), &expected_calls, &run_loop);

  shopping_service_->GetUpdatedProductInfoForBookmarks(bookmark_ids, callback);
  run_loop.Run();

  EXPECT_EQ(0, expected_calls);
}

TEST_F(ShoppingServiceTest, TestDataMergeWithLeadImage) {
  ProductInfo info;
  info.image_url = GURL(kImageUrl);

  base::Value::Dict data_map;
  data_map.Set("image", "https://example.com/fallback_image.png");

  MergeProductInfoData(&info, data_map);

  EXPECT_EQ(kImageUrl, info.image_url);
}

TEST_F(ShoppingServiceTest, TestDataMergeWithNoLeadImage) {
  test_features_.InitWithFeatures(
      {kCommerceAllowLocalImages, kCommerceAllowServerImages}, {});
  ProductInfo info;

  base::Value::Dict data_map;
  data_map.Set("image", kImageUrl);

  MergeProductInfoData(&info, data_map);

  EXPECT_EQ(kImageUrl, info.image_url.spec());
}

TEST_F(ShoppingServiceTest, TestDataMergeWithTitle) {
  ProductInfo info;
  info.title = kTitle;

  base::Value::Dict data_map;
  data_map.Set("title", "Some other fallback title");

  MergeProductInfoData(&info, data_map);

  EXPECT_EQ(kTitle, info.title);
}

TEST_F(ShoppingServiceTest, TestDataMergeWithNoTitle) {
  ProductInfo info;

  base::Value::Dict data_map;
  data_map.Set("title", kTitle);

  MergeProductInfoData(&info, data_map);

  EXPECT_EQ(kTitle, info.title);
}

TEST_F(ShoppingServiceTest, TestShoppingListEligible_Policy) {
  test_features_.InitWithFeatures({kShoppingList},
                                  {kShoppingListRegionLaunched});

  TestingPrefServiceSimple prefs;
  RegisterPrefs(prefs.registry());
  SetShoppingListEnterprisePolicyPref(&prefs, true);

  MockAccountChecker checker;

  ASSERT_TRUE(IsShoppingListEligible(&checker, &prefs, kEligibleCountry,
                                     kEligibleLocale));

  SetShoppingListEnterprisePolicyPref(&prefs, false);
  ASSERT_FALSE(IsShoppingListEligible(&checker, &prefs, kEligibleCountry,
                                      kEligibleLocale));
}

TEST_F(ShoppingServiceTest, TestShoppingListEligible_FeatureFlagOff) {
  test_features_.InitWithFeatures({},
                                  {kShoppingList, kShoppingListRegionLaunched});

  TestingPrefServiceSimple prefs;
  RegisterPrefs(prefs.registry());
  SetShoppingListEnterprisePolicyPref(&prefs, true);

  MockAccountChecker checker;

  ASSERT_FALSE(IsShoppingListEligible(&checker, &prefs, kEligibleCountry,
                                      kEligibleLocale));
}

TEST_F(ShoppingServiceTest, TestShoppingListEligible_MSBB) {
  test_features_.InitWithFeatures({kShoppingList},
                                  {kShoppingListRegionLaunched});

  TestingPrefServiceSimple prefs;
  RegisterPrefs(prefs.registry());
  SetShoppingListEnterprisePolicyPref(&prefs, true);

  MockAccountChecker checker;

  ASSERT_TRUE(IsShoppingListEligible(&checker, &prefs, kEligibleCountry,
                                     kEligibleLocale));

  checker.SetAnonymizedUrlDataCollectionEnabled(false);

  ASSERT_FALSE(IsShoppingListEligible(&checker, &prefs, kEligibleCountry,
                                      kEligibleLocale));
}

TEST_F(ShoppingServiceTest, TestShoppingListEligible_SignIn) {
  test_features_.InitWithFeatures({kShoppingList},
                                  {kShoppingListRegionLaunched});

  TestingPrefServiceSimple prefs;
  RegisterPrefs(prefs.registry());
  SetShoppingListEnterprisePolicyPref(&prefs, true);

  MockAccountChecker checker;

  ASSERT_TRUE(IsShoppingListEligible(&checker, &prefs, kEligibleCountry,
                                     kEligibleLocale));

  checker.SetSignedIn(false);

  ASSERT_FALSE(IsShoppingListEligible(&checker, &prefs, kEligibleCountry,
                                      kEligibleLocale));
}

TEST_F(ShoppingServiceTest, TestShoppingListEligible_WAA) {
  test_features_.InitWithFeatures({kShoppingList},
                                  {kShoppingListRegionLaunched});

  TestingPrefServiceSimple prefs;
  RegisterPrefs(prefs.registry());
  SetShoppingListEnterprisePolicyPref(&prefs, true);

  MockAccountChecker checker;

  ASSERT_TRUE(IsShoppingListEligible(&checker, &prefs, kEligibleCountry,
                                     kEligibleLocale));

  checker.SetWebAndAppActivityEnabled(false);

  ASSERT_FALSE(IsShoppingListEligible(&checker, &prefs, kEligibleCountry,
                                      kEligibleLocale));
}

TEST_F(ShoppingServiceTest, TestShoppingListEligible_ChildAccount) {
  test_features_.InitWithFeatures({kShoppingList},
                                  {kShoppingListRegionLaunched});

  TestingPrefServiceSimple prefs;
  RegisterPrefs(prefs.registry());
  SetShoppingListEnterprisePolicyPref(&prefs, true);

  MockAccountChecker checker;

  ASSERT_TRUE(IsShoppingListEligible(&checker, &prefs, kEligibleCountry,
                                     kEligibleLocale));

  checker.SetIsSubjectToParentalControls(true);

  ASSERT_FALSE(IsShoppingListEligible(&checker, &prefs, kEligibleCountry,
                                      kEligibleLocale));
}

TEST_F(ShoppingServiceTest, TestShoppingListEligible_SyncState) {
  test_features_.InitWithFeatures({kShoppingList},
                                  {kShoppingListRegionLaunched});

  TestingPrefServiceSimple prefs;
  RegisterPrefs(prefs.registry());
  SetShoppingListEnterprisePolicyPref(&prefs, true);

  MockAccountChecker checker;

  ASSERT_TRUE(IsShoppingListEligible(&checker, &prefs, kEligibleCountry,
                                     kEligibleLocale));

  checker.SetSyncingBookmarks(false);

  ASSERT_FALSE(IsShoppingListEligible(&checker, &prefs, kEligibleCountry,
                                      kEligibleLocale));
}

TEST_F(ShoppingServiceTest, TestShoppingListEligible_CountryAndLocale) {
  test_features_.InitWithFeatures({kShoppingList},
                                  {kShoppingListRegionLaunched});

  TestingPrefServiceSimple prefs;
  RegisterPrefs(prefs.registry());
  SetShoppingListEnterprisePolicyPref(&prefs, true);

  MockAccountChecker checker;

  ASSERT_TRUE(IsShoppingListEligible(&checker, &prefs, kEligibleCountry,
                                     kEligibleLocale));

  // This should continue to work since we can assume, for the sake of the test,
  // that the experiment config includes the ZZ country and zz-zz locale.
  ASSERT_TRUE(IsShoppingListEligible(&checker, &prefs, "ZZ", "zz-zz"));
}

TEST_F(ShoppingServiceTest,
       TestShoppingListEligible_CountryAndLocale_BothFlags) {
  test_features_.InitWithFeatures({kShoppingList, kShoppingListRegionLaunched},
                                  {});

  TestingPrefServiceSimple prefs;
  RegisterPrefs(prefs.registry());
  SetShoppingListEnterprisePolicyPref(&prefs, true);

  MockAccountChecker checker;

  ASSERT_TRUE(IsShoppingListEligible(&checker, &prefs, kEligibleCountry,
                                     kEligibleLocale));

  // Same as the previous test, this should still work since, presumably, the
  // experiment config for "ShoppingList" includes these.
  ASSERT_TRUE(IsShoppingListEligible(&checker, &prefs, "ZZ", "zz-zz"));
}

TEST_F(ShoppingServiceTest, TestShoppingListEligible_CountryAndLocale_NoFlags) {
  test_features_.InitWithFeatures({},
                                  {kShoppingList, kShoppingListRegionLaunched});

  TestingPrefServiceSimple prefs;
  RegisterPrefs(prefs.registry());
  SetShoppingListEnterprisePolicyPref(&prefs, true);

  MockAccountChecker checker;

  ASSERT_FALSE(IsShoppingListEligible(&checker, &prefs, kEligibleCountry,
                                      kEligibleLocale));

  ASSERT_FALSE(IsShoppingListEligible(&checker, &prefs, "ZZ", "zz-zz"));
}

TEST_F(ShoppingServiceTest,
       TestShoppingListEligible_CountryAndLocale_RegionLaunched) {
  test_features_.InitWithFeatures({kShoppingListRegionLaunched},
                                  {kShoppingList});

  TestingPrefServiceSimple prefs;
  RegisterPrefs(prefs.registry());
  SetShoppingListEnterprisePolicyPref(&prefs, true);

  MockAccountChecker checker;

  ASSERT_TRUE(IsShoppingListEligible(&checker, &prefs, kEligibleCountry,
                                     kEligibleLocale));

  // If we only have the region flag enabled, we should be restricted to
  // specific countries and locales. The fake country and locale below should
  // be blocked.
  ASSERT_FALSE(IsShoppingListEligible(&checker, &prefs, "ZZ", "zz-zz"));
}

class ShoppingServiceReadyTest : public ShoppingServiceTest {
 public:
  ShoppingServiceReadyTest() = default;
  ShoppingServiceReadyTest(const ShoppingServiceReadyTest&) = delete;
  ShoppingServiceReadyTest operator=(const ShoppingServiceReadyTest&) = delete;
  ~ShoppingServiceReadyTest() override = default;

  void SetUp() override {
    sync_service_->SetTransportState(
        syncer::SyncService::TransportState::INITIALIZING);
    ShoppingServiceTest::SetUp();
  }
};

TEST_F(ShoppingServiceReadyTest, TestServiceReadyDelaysForSync) {
  test_features_.InitWithFeatures({kShoppingList}, {});

  bool service_ready = false;
  shopping_service_->WaitForReady(
      base::BindOnce([](bool* service_ready,
                        ShoppingService* service) { *service_ready = true; },
                     &service_ready));

  base::RunLoop().RunUntilIdle();

  // The ready check should not have run since sync is not ready.
  ASSERT_FALSE(service_ready);

  sync_service_->SetHasSyncConsent(true);
  sync_service_->SetInitialSyncFeatureSetupComplete(true);
  sync_service_->SetTransportState(syncer::SyncService::TransportState::ACTIVE);
  sync_service_->FireStateChanged();

  base::RunLoop().RunUntilIdle();

  // The run loop should be finished now.
  ASSERT_TRUE(service_ready);
}

TEST_F(ShoppingServiceReadyTest, TestServiceReadyDelaysForSync_SyncActive) {
  test_features_.InitWithFeatures({kShoppingList}, {});

  sync_service_->SetHasSyncConsent(true);
  sync_service_->SetInitialSyncFeatureSetupComplete(true);
  sync_service_->SetTransportState(syncer::SyncService::TransportState::ACTIVE);
  sync_service_->FireStateChanged();

  bool service_ready = false;
  shopping_service_->WaitForReady(
      base::BindOnce([](bool* service_ready,
                        ShoppingService* service) { *service_ready = true; },
                     &service_ready));

  base::RunLoop().RunUntilIdle();

  // The ready check should complete since sync was already active.
  ASSERT_TRUE(service_ready);
}

TEST_F(ShoppingServiceTest, TestPriceInsightsInfoResponse) {
  test_features_.InitAndEnableFeature(kPriceInsights);

  std::vector<std::tuple<std::string, int64_t>> history_prices;
  history_prices.emplace_back("2021-01-01", 100);
  history_prices.emplace_back("2021-01-02", 200);

  OptimizationMetadata meta = opt_guide_->BuildPriceInsightsResponse(
      kClusterId, kCurrencyCode, kLowTypicalPrice, kHighTypicalPrice,
      kCurrencyCode, kAttributes, history_prices, kJackpotUrl,
      PriceBucket::kHighPrice, true);

  opt_guide_->SetResponse(GURL(kPriceInsightsUrl),
                          OptimizationType::PRICE_INSIGHTS,
                          OptimizationGuideDecision::kTrue, meta);

  base::RunLoop run_loop;
  shopping_service_->GetPriceInsightsInfoForUrl(
      GURL(kPriceInsightsUrl),
      base::BindOnce(
          [](base::RunLoop* run_loop, const GURL& url,
             const absl::optional<PriceInsightsInfo>& info) {
            ASSERT_EQ(kPriceInsightsUrl, url.spec());
            ASSERT_TRUE(info.has_value());

            ASSERT_EQ(kClusterId, info->product_cluster_id);
            ASSERT_EQ(kCurrencyCode, info->currency_code);
            ASSERT_EQ(kLowTypicalPrice, info->typical_low_price_micros);
            ASSERT_EQ(kHighTypicalPrice, info->typical_high_price_micros);
            ASSERT_EQ(kAttributes, info->catalog_attributes);
            ASSERT_EQ(2, (int)(info->catalog_history_prices.size()));
            ASSERT_EQ("2021-01-01",
                      std::get<0>(info->catalog_history_prices[0]));
            ASSERT_EQ("2021-01-02",
                      std::get<0>(info->catalog_history_prices[1]));
            ASSERT_EQ(100, std::get<1>(info->catalog_history_prices[0]));
            ASSERT_EQ(200, std::get<1>(info->catalog_history_prices[1]));
            ASSERT_EQ(kJackpotUrl, info->jackpot_url);
            ASSERT_EQ(PriceBucket::kHighPrice, info->price_bucket);
            ASSERT_EQ(true, info->has_multiple_catalogs);

            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();
}

TEST_F(ShoppingServiceTest,
       TestPriceInsightsInfoResponse_DifferentCurrencyCode) {
  test_features_.InitAndEnableFeature(kPriceInsights);

  std::vector<std::tuple<std::string, int64_t>> history_prices;
  history_prices.emplace_back("2021-01-01", 100);
  history_prices.emplace_back("2021-01-02", 200);

  OptimizationMetadata meta = opt_guide_->BuildPriceInsightsResponse(
      kClusterId, kCurrencyCode, kLowTypicalPrice, kHighTypicalPrice,
      kAnotherCurrencyCode, kAttributes, history_prices, kJackpotUrl,
      PriceBucket::kHighPrice, true);

  opt_guide_->SetResponse(GURL(kPriceInsightsUrl),
                          OptimizationType::PRICE_INSIGHTS,
                          OptimizationGuideDecision::kTrue, meta);

  base::RunLoop run_loop;
  shopping_service_->GetPriceInsightsInfoForUrl(
      GURL(kPriceInsightsUrl),
      base::BindOnce(
          [](base::RunLoop* run_loop, const GURL& url,
             const absl::optional<PriceInsightsInfo>& info) {
            ASSERT_EQ(kPriceInsightsUrl, url.spec());
            ASSERT_TRUE(info.has_value());

            ASSERT_EQ(kClusterId, info->product_cluster_id);
            ASSERT_EQ(kCurrencyCode, info->currency_code);
            ASSERT_EQ(kLowTypicalPrice, info->typical_low_price_micros);
            ASSERT_EQ(kHighTypicalPrice, info->typical_high_price_micros);
            ASSERT_EQ(absl::nullopt, info->catalog_attributes);
            ASSERT_EQ(0, (int)(info->catalog_history_prices.size()));
            ASSERT_EQ(absl::nullopt, info->jackpot_url);
            ASSERT_EQ(PriceBucket::kHighPrice, info->price_bucket);
            ASSERT_EQ(true, info->has_multiple_catalogs);

            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();
}

TEST_F(ShoppingServiceTest, TestPriceInsightsInfoResponse_EmptyClusterId) {
  test_features_.InitAndEnableFeature(kPriceInsights);

  std::vector<std::tuple<std::string, int64_t>> history_prices;
  history_prices.emplace_back("2021-01-01", 100);
  history_prices.emplace_back("2021-01-02", 200);

  OptimizationMetadata meta = opt_guide_->BuildPriceInsightsResponse(
      0, kCurrencyCode, kLowTypicalPrice, kHighTypicalPrice,
      kAnotherCurrencyCode, kAttributes, history_prices, kJackpotUrl,
      PriceBucket::kHighPrice, true);

  opt_guide_->SetResponse(GURL(kPriceInsightsUrl),
                          OptimizationType::PRICE_INSIGHTS,
                          OptimizationGuideDecision::kTrue, meta);

  base::RunLoop run_loop;
  shopping_service_->GetPriceInsightsInfoForUrl(
      GURL(kPriceInsightsUrl),
      base::BindOnce(
          [](base::RunLoop* run_loop, const GURL& url,
             const absl::optional<PriceInsightsInfo>& info) {
            ASSERT_EQ(kPriceInsightsUrl, url.spec());
            ASSERT_FALSE(info.has_value());

            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();
}

TEST_F(ShoppingServiceTest, TestPriceInsightsInfoResponse_EmptyRange) {
  test_features_.InitAndEnableFeature(kPriceInsights);

  std::vector<std::tuple<std::string, int64_t>> history_prices;
  history_prices.emplace_back("2021-01-01", 100);
  history_prices.emplace_back("2021-01-02", 200);

  OptimizationMetadata meta = opt_guide_->BuildPriceInsightsResponse(
      kClusterId, "", 0, 0, kCurrencyCode, kAttributes, history_prices, "",
      PriceBucket::kHighPrice, true);

  opt_guide_->SetResponse(GURL(kPriceInsightsUrl),
                          OptimizationType::PRICE_INSIGHTS,
                          OptimizationGuideDecision::kTrue, meta);

  base::RunLoop run_loop;
  shopping_service_->GetPriceInsightsInfoForUrl(
      GURL(kPriceInsightsUrl),
      base::BindOnce(
          [](base::RunLoop* run_loop, const GURL& url,
             const absl::optional<PriceInsightsInfo>& info) {
            ASSERT_EQ(kPriceInsightsUrl, url.spec());
            ASSERT_TRUE(info.has_value());

            ASSERT_EQ(kClusterId, info->product_cluster_id);
            ASSERT_EQ(kCurrencyCode, info->currency_code);
            ASSERT_EQ(absl::nullopt, info->typical_low_price_micros);
            ASSERT_EQ(absl::nullopt, info->typical_high_price_micros);
            ASSERT_EQ(kAttributes, info->catalog_attributes);
            ASSERT_EQ(2, (int)(info->catalog_history_prices.size()));
            ASSERT_EQ("2021-01-01",
                      std::get<0>(info->catalog_history_prices[0]));
            ASSERT_EQ("2021-01-02",
                      std::get<0>(info->catalog_history_prices[1]));
            ASSERT_EQ(100, std::get<1>(info->catalog_history_prices[0]));
            ASSERT_EQ(200, std::get<1>(info->catalog_history_prices[1]));
            ASSERT_EQ(absl::nullopt, info->jackpot_url);
            ASSERT_EQ(PriceBucket::kHighPrice, info->price_bucket);
            ASSERT_EQ(true, info->has_multiple_catalogs);

            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();
}

}  // namespace commerce
