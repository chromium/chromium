// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/shopping_service.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/uuid.h"
#include "base/values.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/mock_account_checker.h"
#include "components/commerce/core/mock_discounts_storage.h"
#include "components/commerce/core/pref_names.h"
#include "components/commerce/core/proto/shopping_page_types.pb.h"
#include "components/commerce/core/shopping_service_test_base.h"
#include "components/commerce/core/test_utils.h"
#include "components/optimization_guide/core/optimization_guide_decider.h"
#include "components/optimization_guide/core/optimization_guide_decision.h"
#include "components/optimization_guide/core/optimization_metadata.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/power_bookmarks/core/power_bookmark_utils.h"
#include "components/power_bookmarks/core/proto/power_bookmark_meta.pb.h"
#include "components/power_bookmarks/core/proto/shopping_specifics.pb.h"
#include "components/prefs/testing_pref_service.h"
#include "components/search/ntp_features.h"
#include "components/sync/base/features.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/gtest/include/gtest/gtest.h"

using optimization_guide::OptimizationGuideDecision;
using optimization_guide::OptimizationGuideDecisionCallback;
using optimization_guide::OptimizationMetadata;
using optimization_guide::proto::Any;
using optimization_guide::proto::OptimizationType;

using testing::_;

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

const char kDiscountsUrl1[] = "http://example.com/discounts_1";
const char kDiscountsUrl2[] = "http://example.com/discounts_2";
const char kDiscountLanguageCode[] = "en-US";
const char kDiscountDetail[] = "details";
const char kDiscountTerms[] = "terms";
const char kDiscountValueText[] = "10% off";
const double kDiscountExpiryTime = 1000000;
const char kDiscountCode[] = "discount code";
const uint64_t kDiscountId1 = 111;
const uint64_t kDiscountId2 = 222;
const uint64_t kDiscountOfferId = 123456;

}  // namespace

class ShoppingServiceTest : public ShoppingServiceTestBase,
                            public testing::WithParamInterface<bool> {
 public:
  ShoppingServiceTest() = default;
  ShoppingServiceTest(const ShoppingServiceTest&) = delete;
  ShoppingServiceTest operator=(const ShoppingServiceTest&) = delete;
  ~ShoppingServiceTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatureState(
        syncer::kReplaceSyncPromosWithSignInPromos,
        ShouldEnableReplaceSyncPromosWithSignInPromos());
    sync_service_->SetHasSyncConsent(
        !ShouldEnableReplaceSyncPromosWithSignInPromos());
    ShoppingServiceTestBase::SetUp();
  }

  // Expose the private feature check for testing.
  static bool IsShoppingListEligible(AccountChecker* account_checker,
                                     PrefService* prefs,
                                     const std::string& country,
                                     const std::string& locale) {
    return ShoppingService::IsShoppingListEligible(account_checker, prefs,
                                                   country, locale);
  }

  bool ShouldEnableReplaceSyncPromosWithSignInPromos() const {
    return GetParam();
  }

  void SetDiscountsStorageForTesting(
      std::unique_ptr<DiscountsStorage> storage) {
    shopping_service_->SetDiscountsStorageForTesting(std::move(storage));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Test that product info is processed correctly.
TEST_P(ShoppingServiceTest, TestProductInfoResponse) {
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
             const absl::optional<const ProductInfo>& info) {
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
TEST_P(ShoppingServiceTest, TestProductInfoResponse_ApiDisabled) {
  // Ensure a feature that uses product info is disabled.
  test_features_.InitWithFeatures({},
                                  {kShoppingList, kShoppingListRegionLaunched,
                                   ntp_features::kNtpChromeCartModule});

  base::RunLoop run_loop;
  shopping_service_->GetProductInfoForUrl(
      GURL(kProductUrl), base::BindOnce(
                             [](base::RunLoop* run_loop, const GURL& url,
                                const absl::optional<const ProductInfo>& info) {
                               ASSERT_EQ(kProductUrl, url.spec());
                               ASSERT_FALSE(info.has_value());
                               run_loop->Quit();
                             },
                             &run_loop));
  run_loop.Run();
}

TEST_P(ShoppingServiceTest, TestProductInfoResponse_CurrencyMismatch) {
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
             const absl::optional<const ProductInfo>& info) {
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
TEST_P(ShoppingServiceTest, TestProductInfoResponse_OptGuideFalse) {
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
                                const absl::optional<const ProductInfo>& info) {
                               ASSERT_EQ(kProductUrl, url.spec());
                               ASSERT_FALSE(info.has_value());
                               run_loop->Quit();
                             },
                             &run_loop));
  run_loop.Run();
}

// Test that the product info cache only keeps track of live tabs.
TEST_P(ShoppingServiceTest, TestProductInfoCacheURLCount) {
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
TEST_P(ShoppingServiceTest, TestProductInfoCacheFullLifecycle) {
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
                                const absl::optional<const ProductInfo>& info) {
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
TEST_P(ShoppingServiceTest,
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
                                const absl::optional<const ProductInfo>& info) {
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
TEST_P(ShoppingServiceTest,
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
                                const absl::optional<const ProductInfo>& info) {
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
TEST_P(ShoppingServiceTest, TestMerchantInfoResponse) {
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
TEST_P(ShoppingServiceTest, TestMerchantInfoResponse_ApiDisabled) {
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

TEST_P(ShoppingServiceTest, TestGetUpdatedProductInfoForBookmarks) {
  bookmarks::BookmarkModel* model =
      ShouldEnableReplaceSyncPromosWithSignInPromos()
          ? account_bookmark_model_.get()
          : local_or_syncable_bookmark_model_.get();

  const bookmarks::BookmarkNode* product1 =
      AddProductBookmark(model, u"title", GURL(kProductUrl), kClusterId, false);

  OptimizationMetadata updated_meta = opt_guide_->BuildPriceTrackingResponse(
      kTitle, "", kOfferId, kClusterId, kCountryCode);
  opt_guide_->AddOnDemandShoppingResponse(
      GURL(kProductUrl), OptimizationGuideDecision::kTrue, updated_meta);

  std::vector<base::Uuid> bookmark_uuids;
  bookmark_uuids.push_back(product1->uuid());
  int expected_calls = bookmark_uuids.size();

  base::RunLoop run_loop;

  auto callback = base::BindRepeating(
      [](bookmarks::BookmarkModel* model, int* call_count,
         base::RunLoop* run_loop, const base::Uuid& uuid, const GURL& url,
         absl::optional<ProductInfo> info) {
        const bookmarks::BookmarkNode* node = model->GetNodeByUuid(uuid);
        EXPECT_EQ(url.spec(), node->url().spec());

        (*call_count)--;
        if (*call_count <= 0) {
          run_loop->Quit();
        }
      },
      model, &expected_calls, &run_loop);

  shopping_service_->GetUpdatedProductInfoForBookmarks(bookmark_uuids,
                                                       callback);
  run_loop.Run();

  EXPECT_EQ(0, expected_calls);
}

TEST_P(ShoppingServiceTest, TestDataMergeWithLeadImage) {
  ProductInfo info;
  info.image_url = GURL(kImageUrl);

  base::Value::Dict data_map;
  data_map.Set("image", "https://example.com/fallback_image.png");

  MergeProductInfoData(&info, data_map);

  EXPECT_EQ(kImageUrl, info.image_url);
}

TEST_P(ShoppingServiceTest, TestDataMergeWithNoLeadImage) {
  test_features_.InitWithFeatures(
      {kCommerceAllowLocalImages, kCommerceAllowServerImages}, {});
  ProductInfo info;

  base::Value::Dict data_map;
  data_map.Set("image", kImageUrl);

  MergeProductInfoData(&info, data_map);

  EXPECT_EQ(kImageUrl, info.image_url.spec());
}

TEST_P(ShoppingServiceTest, TestDataMergeWithTitle) {
  ProductInfo info;
  info.title = kTitle;

  base::Value::Dict data_map;
  data_map.Set("title", "Some other fallback title");

  MergeProductInfoData(&info, data_map);

  EXPECT_EQ(kTitle, info.title);
}

TEST_P(ShoppingServiceTest, TestDataMergeWithNoTitle) {
  ProductInfo info;

  base::Value::Dict data_map;
  data_map.Set("title", kTitle);

  MergeProductInfoData(&info, data_map);

  EXPECT_EQ(kTitle, info.title);
}

TEST_P(ShoppingServiceTest, TestShoppingListEligible_Policy) {
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

TEST_P(ShoppingServiceTest, TestShoppingListEligible_FeatureFlagOff) {
  test_features_.InitWithFeatures({},
                                  {kShoppingList, kShoppingListRegionLaunched});

  TestingPrefServiceSimple prefs;
  RegisterPrefs(prefs.registry());
  SetShoppingListEnterprisePolicyPref(&prefs, true);

  MockAccountChecker checker;

  ASSERT_FALSE(IsShoppingListEligible(&checker, &prefs, kEligibleCountry,
                                      kEligibleLocale));
}

TEST_P(ShoppingServiceTest, TestShoppingListEligible_MSBB) {
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

TEST_P(ShoppingServiceTest, TestShoppingListEligible_SignIn) {
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

TEST_P(ShoppingServiceTest, TestShoppingListEligible_WAA) {
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

TEST_P(ShoppingServiceTest, TestShoppingListEligible_ChildAccount) {
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

TEST_P(ShoppingServiceTest, TestShoppingListEligible_SyncState) {
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

TEST_P(ShoppingServiceTest, TestShoppingListEligible_CountryAndLocale) {
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

TEST_P(ShoppingServiceTest,
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

TEST_P(ShoppingServiceTest, TestShoppingListEligible_CountryAndLocale_NoFlags) {
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

TEST_P(ShoppingServiceTest,
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

TEST_P(ShoppingServiceReadyTest, TestServiceReadyDelaysForSync) {
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

TEST_P(ShoppingServiceReadyTest, TestServiceReadyDelaysForSync_SyncActive) {
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

INSTANTIATE_TEST_SUITE_P(All, ShoppingServiceReadyTest, ::testing::Bool());

TEST_P(ShoppingServiceTest, TestPriceInsightsInfoResponse) {
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

TEST_P(ShoppingServiceTest,
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

TEST_P(ShoppingServiceTest, TestPriceInsightsInfoResponse_EmptyClusterId) {
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

TEST_P(ShoppingServiceTest, TestPriceInsightsInfoResponse_EmptyRange) {
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

TEST_P(ShoppingServiceTest, TestPriceInsightsInfoResponse_WithoutCache) {
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

  base::RunLoop run_loop1;
  shopping_service_->GetPriceInsightsInfoForUrl(
      GURL(kPriceInsightsUrl),
      base::BindOnce(
          [](base::RunLoop* run_loop, const GURL& url,
             const absl::optional<PriceInsightsInfo>& info) {
            ASSERT_EQ(kPriceInsightsUrl, url.spec());
            ASSERT_TRUE(info.has_value());
            ASSERT_EQ(kClusterId, info->product_cluster_id);
            run_loop->Quit();
          },
          &run_loop1));
  run_loop1.Run();

  // Simulate that the OptGuide result is not saved in cache and is cleared
  // after some time.
  opt_guide_->SetResponse(
      GURL(kPriceInsightsUrl), OptimizationType::PRICE_INSIGHTS,
      OptimizationGuideDecision::kTrue, OptimizationMetadata());
  base::RunLoop run_loop2;
  shopping_service_->GetPriceInsightsInfoForUrl(
      GURL(kPriceInsightsUrl),
      base::BindOnce(
          [](base::RunLoop* run_loop, const GURL& url,
             const absl::optional<PriceInsightsInfo>& info) {
            ASSERT_EQ(kPriceInsightsUrl, url.spec());
            ASSERT_FALSE(info.has_value());
            run_loop->Quit();
          },
          &run_loop2));
  run_loop2.Run();
}

TEST_P(ShoppingServiceTest, TestPriceInsightsInfoResponse_WithCache) {
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

  base::RunLoop run_loop1;
  shopping_service_->GetPriceInsightsInfoForUrl(
      GURL(kPriceInsightsUrl),
      base::BindOnce(
          [](base::RunLoop* run_loop, const GURL& url,
             const absl::optional<PriceInsightsInfo>& info) {
            ASSERT_EQ(kPriceInsightsUrl, url.spec());
            ASSERT_TRUE(info.has_value());
            ASSERT_EQ(kClusterId, info->product_cluster_id);
            run_loop->Quit();
          },
          &run_loop1));
  run_loop1.Run();

  // Simulate that the OptGuide result is cleared after some time but saved in
  // cache already.
  MockWebWrapper web(GURL(kPriceInsightsUrl), false);
  DidNavigatePrimaryMainFrame(&web);

  opt_guide_->SetResponse(
      GURL(kPriceInsightsUrl), OptimizationType::PRICE_INSIGHTS,
      OptimizationGuideDecision::kTrue, OptimizationMetadata());
  base::RunLoop run_loop2;
  shopping_service_->GetPriceInsightsInfoForUrl(
      GURL(kPriceInsightsUrl),
      base::BindOnce(
          [](base::RunLoop* run_loop, const GURL& url,
             const absl::optional<PriceInsightsInfo>& info) {
            ASSERT_EQ(kPriceInsightsUrl, url.spec());
            ASSERT_TRUE(info.has_value());
            ASSERT_EQ(kClusterId, info->product_cluster_id);
            run_loop->Quit();
          },
          &run_loop2));
  run_loop2.Run();

  // On navigating away, we should clear the cache.
  DidNavigateAway(&web, GURL(kPriceInsightsUrl));
  base::RunLoop run_loop3;
  shopping_service_->GetPriceInsightsInfoForUrl(
      GURL(kPriceInsightsUrl),
      base::BindOnce(
          [](base::RunLoop* run_loop, const GURL& url,
             const absl::optional<PriceInsightsInfo>& info) {
            ASSERT_EQ(kPriceInsightsUrl, url.spec());
            ASSERT_FALSE(info.has_value());
            run_loop->Quit();
          },
          &run_loop3));
  run_loop3.Run();
}

TEST_P(ShoppingServiceTest,
       TestPriceInsightsInfoResponse_WithCacheMultipleTabs) {
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

  MockWebWrapper web1(GURL(kPriceInsightsUrl), false);
  DidNavigatePrimaryMainFrame(&web1);

  base::RunLoop run_loop1;
  shopping_service_->GetPriceInsightsInfoForUrl(
      GURL(kPriceInsightsUrl),
      base::BindOnce(
          [](base::RunLoop* run_loop, const GURL& url,
             const absl::optional<PriceInsightsInfo>& info) {
            ASSERT_EQ(kPriceInsightsUrl, url.spec());
            ASSERT_TRUE(info.has_value());
            ASSERT_EQ(kClusterId, info->product_cluster_id);
            run_loop->Quit();
          },
          &run_loop1));
  run_loop1.Run();

  opt_guide_->SetResponse(
      GURL(kPriceInsightsUrl), OptimizationType::PRICE_INSIGHTS,
      OptimizationGuideDecision::kTrue, OptimizationMetadata());

  // Simulate navigating to another tab with the same url.
  MockWebWrapper web2(GURL(kPriceInsightsUrl), false);
  DidNavigatePrimaryMainFrame(&web2);

  // Navigating away from one tab should not clear the cache.
  DidNavigateAway(&web1, GURL(kPriceInsightsUrl));

  base::RunLoop run_loop2;
  shopping_service_->GetPriceInsightsInfoForUrl(
      GURL(kPriceInsightsUrl),
      base::BindOnce(
          [](base::RunLoop* run_loop, const GURL& url,
             const absl::optional<PriceInsightsInfo>& info) {
            ASSERT_EQ(kPriceInsightsUrl, url.spec());
            ASSERT_TRUE(info.has_value());
            ASSERT_EQ(kClusterId, info->product_cluster_id);
            run_loop->Quit();
          },
          &run_loop2));
  run_loop2.Run();

  // Navigating away from or destroying all tabs should clear the cache.
  WebWrapperDestroyed(&web2);
  base::RunLoop run_loop3;
  shopping_service_->GetPriceInsightsInfoForUrl(
      GURL(kPriceInsightsUrl),
      base::BindOnce(
          [](base::RunLoop* run_loop, const GURL& url,
             const absl::optional<PriceInsightsInfo>& info) {
            ASSERT_EQ(kPriceInsightsUrl, url.spec());
            ASSERT_FALSE(info.has_value());
            run_loop->Quit();
          },
          &run_loop3));
  run_loop3.Run();
}

TEST_P(ShoppingServiceTest, TestIsShoppingPage) {
  test_features_.InitAndEnableFeature(kShoppingPageTypes);
  base::RunLoop run_loop[3];
  OptimizationMetadata meta;
  ShoppingPageTypes data;

  data.add_shopping_page_types(commerce::ShoppingPageTypes::SHOPPING_PAGE);
  data.add_shopping_page_types(
      commerce::ShoppingPageTypes::MERCHANT_DOMAIN_PAGE);
  Any any;
  any.set_type_url(data.GetTypeName());
  data.SerializeToString(any.mutable_value());
  meta.set_any_metadata(any);
  opt_guide_->SetResponse(GURL(kProductUrl),
                          OptimizationType::SHOPPING_PAGE_TYPES,
                          OptimizationGuideDecision::kTrue, meta);

  shopping_service_->IsShoppingPage(
      GURL(kProductUrl), base::BindOnce(
                             [](base::RunLoop* run_loop, const GURL& url,
                                absl::optional<bool> info) {
                               ASSERT_TRUE(info.has_value());
                               ASSERT_TRUE(info.value());
                               run_loop->Quit();
                             },
                             &run_loop[0]));
  run_loop[0].Run();

  opt_guide_->SetResponse(GURL(kProductUrl),
                          OptimizationType::SHOPPING_PAGE_TYPES,
                          OptimizationGuideDecision::kFalse, meta);

  shopping_service_->IsShoppingPage(
      GURL(kProductUrl), base::BindOnce(
                             [](base::RunLoop* run_loop, const GURL& url,
                                absl::optional<bool> info) {
                               ASSERT_FALSE(info.has_value());
                               run_loop->Quit();
                             },
                             &run_loop[1]));
  run_loop[1].Run();

  data.clear_shopping_page_types();
  data.add_shopping_page_types(
      commerce::ShoppingPageTypes::MERCHANT_DOMAIN_PAGE);
  data.SerializeToString(any.mutable_value());
  meta.set_any_metadata(any);
  opt_guide_->SetResponse(GURL(kProductUrl),
                          OptimizationType::SHOPPING_PAGE_TYPES,
                          OptimizationGuideDecision::kTrue, meta);

  shopping_service_->IsShoppingPage(
      GURL(kProductUrl), base::BindOnce(
                             [](base::RunLoop* run_loop, const GURL& url,
                                absl::optional<bool> info) {
                               ASSERT_TRUE(info.has_value());
                               ASSERT_FALSE(info.value());
                               run_loop->Quit();
                             },
                             &run_loop[2]));
  run_loop[2].Run();
}

TEST_P(ShoppingServiceTest, TestDiscountInfoResponse) {
  test_features_.InitAndEnableFeature(kShowDiscountOnNavigation);

  std::vector<DiscountInfo> infos;

  // Valid info.
  DiscountInfo valid_info;
  valid_info.cluster_type = DiscountClusterType::kOfferLevel;
  valid_info.type = DiscountType::kFreeListingWithCode;
  valid_info.language_code = kDiscountLanguageCode;
  valid_info.description_detail = kDiscountDetail;
  valid_info.terms_and_conditions = kDiscountTerms;
  valid_info.value_in_text = kDiscountValueText;
  valid_info.discount_code = kDiscountCode;
  valid_info.id = kDiscountId1;
  valid_info.is_merchant_wide = true;
  valid_info.expiry_time_sec = kDiscountExpiryTime;
  valid_info.offer_id = kDiscountOfferId;
  infos.push_back(valid_info);
  // Another valid info with different cluster type.
  DiscountInfo valid_info_2 = valid_info;
  valid_info_2.id = kDiscountId2;
  valid_info_2.cluster_type = DiscountClusterType::kUnspecified;
  infos.push_back(valid_info_2);

  opt_guide_->SetResponse(GURL(kDiscountsUrl2),
                          OptimizationType::SHOPPING_DISCOUNTS,
                          OptimizationGuideDecision::kTrue,
                          opt_guide_->BuildDiscountsResponse(infos));

  std::unique_ptr<MockDiscountsStorage> storage =
      std::make_unique<MockDiscountsStorage>();
  EXPECT_CALL(*storage, HandleServerDiscounts(
                            std::vector<std::string>{kDiscountsUrl1}, _, _));
  SetDiscountsStorageForTesting(std::move(storage));

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(kDiscountsFetchResultHistogramName, 0);

  base::RunLoop run_loop;
  shopping_service_->GetDiscountInfoForUrls(
      std::vector<GURL>{GURL(kDiscountsUrl1), GURL(kDiscountsUrl2)},
      base::BindOnce(
          [](base::RunLoop* run_loop, const DiscountsMap& map) {
            ASSERT_EQ(1, (int)map.size());

            auto discounts = map.find(GURL(kDiscountsUrl2))->second;
            ASSERT_EQ(2, (int)discounts.size());

            ASSERT_EQ(DiscountClusterType::kOfferLevel,
                      discounts[0].cluster_type);
            ASSERT_EQ(DiscountType::kFreeListingWithCode, discounts[0].type);
            ASSERT_EQ(kDiscountLanguageCode, discounts[0].language_code);
            ASSERT_EQ(kDiscountDetail, discounts[0].description_detail);
            ASSERT_EQ(kDiscountTerms, discounts[0].terms_and_conditions);
            ASSERT_EQ(kDiscountValueText, discounts[0].value_in_text);
            ASSERT_EQ(kDiscountCode, discounts[0].discount_code);
            ASSERT_EQ(kDiscountId1, discounts[0].id);
            ASSERT_EQ(true, discounts[0].is_merchant_wide);
            ASSERT_EQ(kDiscountExpiryTime, discounts[0].expiry_time_sec);
            ASSERT_EQ(kDiscountOfferId, discounts[0].offer_id);

            ASSERT_EQ(kDiscountId2, discounts[1].id);
            ASSERT_EQ(DiscountClusterType::kUnspecified,
                      discounts[1].cluster_type);

            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();

  histogram_tester.ExpectTotalCount(kDiscountsFetchResultHistogramName, 1);
  histogram_tester.ExpectBucketCount(kDiscountsFetchResultHistogramName, 0, 1);
}

TEST_P(ShoppingServiceTest, TestDiscountInfoResponse_InfoWithoutId) {
  test_features_.InitAndEnableFeature(kShowDiscountOnNavigation);

  std::vector<DiscountInfo> infos;

  // Valid info.
  DiscountInfo valid_info;
  valid_info.cluster_type = DiscountClusterType::kOfferLevel;
  valid_info.type = DiscountType::kFreeListingWithCode;
  valid_info.language_code = kDiscountLanguageCode;
  valid_info.description_detail = kDiscountDetail;
  valid_info.terms_and_conditions = kDiscountTerms;
  valid_info.value_in_text = kDiscountValueText;
  valid_info.discount_code = kDiscountCode;
  valid_info.id = kDiscountId1;
  valid_info.is_merchant_wide = true;
  valid_info.expiry_time_sec = kDiscountExpiryTime;
  valid_info.offer_id = kDiscountOfferId;
  infos.push_back(valid_info);
  // Invalid info without id.
  DiscountInfo invalid_info = valid_info;
  invalid_info.id = kInvalidDiscountId;
  infos.push_back(invalid_info);

  opt_guide_->SetResponse(GURL(kDiscountsUrl2),
                          OptimizationType::SHOPPING_DISCOUNTS,
                          OptimizationGuideDecision::kTrue,
                          opt_guide_->BuildDiscountsResponse(infos));

  base::RunLoop run_loop;
  shopping_service_->GetDiscountInfoForUrls(
      std::vector<GURL>{GURL(kDiscountsUrl1), GURL(kDiscountsUrl2)},
      base::BindOnce(
          [](base::RunLoop* run_loop, const DiscountsMap& map) {
            ASSERT_EQ(1, (int)map.size());

            auto discounts = map.find(GURL(kDiscountsUrl2))->second;
            ASSERT_EQ(1, (int)discounts.size());
            ASSERT_EQ(kDiscountId1, discounts[0].id);
            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();
}

TEST_P(ShoppingServiceTest, TestDiscountInfoResponse_InfoWithoutTerms) {
  test_features_.InitAndEnableFeature(kShowDiscountOnNavigation);

  std::vector<DiscountInfo> infos;

  // Valid info.
  DiscountInfo valid_info;
  valid_info.cluster_type = DiscountClusterType::kOfferLevel;
  valid_info.type = DiscountType::kFreeListingWithCode;
  valid_info.language_code = kDiscountLanguageCode;
  valid_info.description_detail = kDiscountDetail;
  valid_info.terms_and_conditions = absl::nullopt;
  valid_info.value_in_text = kDiscountValueText;
  valid_info.discount_code = kDiscountCode;
  valid_info.id = kDiscountId1;
  valid_info.is_merchant_wide = true;
  valid_info.expiry_time_sec = kDiscountExpiryTime;
  valid_info.offer_id = kDiscountOfferId;
  infos.push_back(valid_info);

  opt_guide_->SetResponse(GURL(kDiscountsUrl2),
                          OptimizationType::SHOPPING_DISCOUNTS,
                          OptimizationGuideDecision::kTrue,
                          opt_guide_->BuildDiscountsResponse(infos));

  base::RunLoop run_loop;
  shopping_service_->GetDiscountInfoForUrls(
      std::vector<GURL>{GURL(kDiscountsUrl1), GURL(kDiscountsUrl2)},
      base::BindOnce(
          [](base::RunLoop* run_loop, const DiscountsMap& map) {
            ASSERT_EQ(1, (int)map.size());

            auto discounts = map.find(GURL(kDiscountsUrl2))->second;
            ASSERT_EQ(1, (int)discounts.size());
            ASSERT_EQ(kDiscountId1, discounts[0].id);
            ASSERT_FALSE(discounts[0].terms_and_conditions.has_value());
            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();
}

TEST_P(ShoppingServiceTest, TestDiscountInfoResponse_InfoWithoutDiscountCode) {
  test_features_.InitAndEnableFeature(kShowDiscountOnNavigation);

  std::vector<DiscountInfo> infos;

  // Invalid info without discount code.
  DiscountInfo invalid_info;
  invalid_info.cluster_type = DiscountClusterType::kOfferLevel;
  invalid_info.type = DiscountType::kFreeListingWithCode;
  invalid_info.language_code = kDiscountLanguageCode;
  invalid_info.description_detail = kDiscountDetail;
  invalid_info.terms_and_conditions = kDiscountTerms;
  invalid_info.value_in_text = kDiscountValueText;
  invalid_info.discount_code = absl::nullopt;
  invalid_info.id = kDiscountId1;
  invalid_info.is_merchant_wide = true;
  invalid_info.expiry_time_sec = kDiscountExpiryTime;
  invalid_info.offer_id = kDiscountOfferId;
  infos.push_back(invalid_info);

  opt_guide_->SetResponse(GURL(kDiscountsUrl2),
                          OptimizationType::SHOPPING_DISCOUNTS,
                          OptimizationGuideDecision::kTrue,
                          opt_guide_->BuildDiscountsResponse(infos));

  base::RunLoop run_loop;
  shopping_service_->GetDiscountInfoForUrls(
      std::vector<GURL>{GURL(kDiscountsUrl1), GURL(kDiscountsUrl2)},
      base::BindOnce(
          [](base::RunLoop* run_loop, const DiscountsMap& map) {
            ASSERT_EQ(0, (int)map.size());
            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();
}

INSTANTIATE_TEST_SUITE_P(All, ShoppingServiceTest, ::testing::Bool());

}  // namespace commerce
