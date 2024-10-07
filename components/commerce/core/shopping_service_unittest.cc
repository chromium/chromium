// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/commerce/core/shopping_service.h"

#include <string>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/values.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/commerce_utils.h"
#include "components/commerce/core/feature_utils.h"
#include "components/commerce/core/mock_account_checker.h"
#include "components/commerce/core/mock_discounts_storage.h"
#include "components/commerce/core/mock_tab_restore_service.h"
#include "components/commerce/core/pref_names.h"
#include "components/commerce/core/proto/shopping_page_types.pb.h"
#include "components/commerce/core/shopping_service_test_base.h"
#include "components/commerce/core/test_utils.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/url_row.h"
#include "components/optimization_guide/core/optimization_guide_decider.h"
#include "components/optimization_guide/core/optimization_guide_decision.h"
#include "components/optimization_guide/core/optimization_metadata.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/power_bookmarks/core/power_bookmark_utils.h"
#include "components/power_bookmarks/core/proto/power_bookmark_meta.pb.h"
#include "components/power_bookmarks/core/proto/shopping_specifics.pb.h"
#include "components/prefs/testing_pref_service.h"
#include "components/search/ntp_features.h"
#include "components/sessions/core/serialized_navigation_entry.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/base/features.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/test/test_sync_service.h"
#include "components/unified_consent/pref_names.h"
#include "net/base/url_util.h"
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
const uint64_t kDiscountOfferId = 123456;

const std::vector<std::vector<std::string>> kProductCategories = {
    {"Dress", "Red Dress"}};

using NiceMockWebWrapper = testing::NiceMock<MockWebWrapper>;

}  // namespace

class ShoppingServiceTest : public ShoppingServiceTestBase,
                            public testing::WithParamInterface<bool> {
 public:
  ShoppingServiceTest()
      : ShoppingServiceTest(syncer::SyncService::TransportState::ACTIVE) {}
  ShoppingServiceTest(const ShoppingServiceTest&) = delete;
  ShoppingServiceTest operator=(const ShoppingServiceTest&) = delete;
  ~ShoppingServiceTest() override = default;

  explicit ShoppingServiceTest(
      syncer::SyncService::TransportState initial_sync_transport_state) {
    if (ShouldEnableReplaceSyncPromosWithSignInPromos()) {
      // `syncer::kSyncEnableBookmarksInTransportMode` must be enabled too in
      // order to use account bookmarks.
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/{syncer::kReplaceSyncPromosWithSignInPromos,
                                syncer::kSyncEnableBookmarksInTransportMode},
          /*disabled_features=*/{});
      bookmark_model_->CreateAccountPermanentFolders();
      identity_test_env_->MakePrimaryAccountAvailable(
          "test@example.com", signin::ConsentLevel::kSignin);
    } else {
      // Whether `syncer::kSyncEnableBookmarksInTransportMode` is enabled or not
      // makes no difference in this case.
      scoped_feature_list_.InitAndDisableFeature(
          syncer::kReplaceSyncPromosWithSignInPromos);
      identity_test_env_->MakePrimaryAccountAvailable(
          "test@example.com", signin::ConsentLevel::kSync);
    }

    sync_service_->SetSignedIn(ShouldEnableReplaceSyncPromosWithSignInPromos()
                                   ? signin::ConsentLevel::kSignin
                                   : signin::ConsentLevel::kSync);
    sync_service_->SetMaxTransportState(initial_sync_transport_state);

    static_cast<bookmarks::TestBookmarkClient*>(bookmark_model_->client())
        ->SetIsSyncFeatureEnabledIncludingBookmarks(
            !ShouldEnableReplaceSyncPromosWithSignInPromos());
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
      kCurrencyCode, kGpcTitle, kProductCategories);
  opt_guide_->AddPriceUpdateToPriceTrackingResponse(&meta, kCurrencyCode,
                                                    kNewPrice, kPrice);

  opt_guide_->SetResponse(GURL(kProductUrl), OptimizationType::PRICE_TRACKING,
                          OptimizationGuideDecision::kTrue, meta);

  base::RunLoop run_loop;
  shopping_service_->GetProductInfoForUrl(
      GURL(kProductUrl),
      base::BindOnce(
          [](base::RunLoop* run_loop, const GURL& url,
             const std::optional<const ProductInfo>& info) {
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

            ASSERT_EQ(static_cast<int>(kProductCategories.size()),
                      info->category_data.product_categories().size());

            for (size_t i = 0; i < kProductCategories.size(); i++) {
              auto labels =
                  info->category_data.product_categories()[i].category_labels();
              ASSERT_EQ(static_cast<int>(kProductCategories[i].size()),
                        labels.size());
              for (int j = 0; j < labels.size(); j++) {
                ASSERT_EQ(kProductCategories[i][j],
                          labels[j].category_default_label());
              }
            }
            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();
}

// Test that product info is fetched on demand if there is no web page open but
// the cache has an entry for the queried URL.
TEST_P(ShoppingServiceTest, TestProductInfoResponse_FallbackToOnDemand) {
  // Ensure a feature that uses product info is enabled. This doesn't
  // necessarily need to be the shopping list.
  test_features_.InitWithFeatures(
      {commerce::kShoppingList, commerce::kCommerceAllowServerImages}, {});

  OptimizationMetadata meta = opt_guide_->BuildPriceTrackingResponse(
      kTitle, kImageUrl, kOfferId, kClusterId, kCountryCode, kPrice,
      kCurrencyCode, kGpcTitle);
  opt_guide_->AddOnDemandShoppingResponse(
      GURL(kProductUrl), OptimizationGuideDecision::kTrue, meta);

  // If the URL is not in the cache, we should not expect a response.
  base::RunLoop run_loop;
  shopping_service_->GetProductInfoForUrl(
      GURL(kProductUrl), base::BindOnce(
                             [](base::RunLoop* run_loop, const GURL& url,
                                const std::optional<const ProductInfo>& info) {
                               ASSERT_EQ(kProductUrl, url.spec());
                               ASSERT_FALSE(info.has_value());
                               run_loop->Quit();
                             },
                             &run_loop));
  run_loop.Run();

  // When the URL is referenced by the cache, we should successfully use the
  // on-demand api.
  GetCache().AddRef(GURL(kProductUrl));

  base::RunLoop run_loop_after_cache;
  shopping_service_->GetProductInfoForUrl(
      GURL(kProductUrl), base::BindOnce(
                             [](base::RunLoop* run_loop, const GURL& url,
                                const std::optional<const ProductInfo>& info) {
                               ASSERT_EQ(kProductUrl, url.spec());
                               ASSERT_TRUE(info.has_value());

                               ASSERT_EQ(kTitle, info->title);
                               ASSERT_EQ(kGpcTitle,
                                         info->product_cluster_title);
                               ASSERT_EQ(kImageUrl, info->image_url);
                               ASSERT_EQ(kOfferId, info->offer_id);
                               ASSERT_EQ(kClusterId, info->product_cluster_id);
                               ASSERT_EQ(kCountryCode, info->country_code);

                               ASSERT_EQ(kCurrencyCode, info->currency_code);
                               ASSERT_EQ(kPrice, info->amount_micros);
                               run_loop->Quit();
                             },
                             &run_loop_after_cache));
  run_loop_after_cache.Run();

  GetCache().RemoveRef(GURL(kProductUrl));
}

// Test multiple on demand calls to get product info.
TEST_P(ShoppingServiceTest, TestProductInfoResponse_MultipleOnDemandRequests) {
  test_features_.InitWithFeatures(
      {commerce::kShoppingList, commerce::kCommerceAllowServerImages}, {});
  OptimizationMetadata meta = opt_guide_->BuildPriceTrackingResponse(
      kTitle, kImageUrl, kOfferId, kClusterId, kCountryCode, kPrice,
      kCurrencyCode, kGpcTitle);
  opt_guide_->AddOnDemandShoppingResponse(
      GURL(kProductUrl), OptimizationGuideDecision::kTrue, meta);
  GetCache().AddRef(GURL(kProductUrl));

  base::RunLoop run_loop_after_cache;
  ProductInfo info[2];
  shopping_service_->GetProductInfoForUrl(
      GURL(kProductUrl), base::BindOnce(
                             [](ProductInfo* result, const GURL& url,
                                const std::optional<const ProductInfo>& info) {
                               ASSERT_EQ(kProductUrl, url.spec());
                               *result = info.value();
                             },
                             &info[0]));

  shopping_service_->GetProductInfoForUrl(
      GURL(kProductUrl), base::BindOnce(
                             [](ProductInfo* result, const GURL& url,
                                const std::optional<const ProductInfo>& info) {
                               ASSERT_EQ(kProductUrl, url.spec());
                               *result = info.value();
                             },
                             &info[1])
                             .Then(run_loop_after_cache.QuitClosure()));
  run_loop_after_cache.Run();

  for (int i = 0; i < 2; ++i) {
    ASSERT_EQ(kTitle, info[i].title);
    ASSERT_EQ(kGpcTitle, info[i].product_cluster_title);
    ASSERT_EQ(kImageUrl, info[i].image_url);
    ASSERT_EQ(kOfferId, info[i].offer_id);
    ASSERT_EQ(kClusterId, info[i].product_cluster_id);
    ASSERT_EQ(kCountryCode, info[i].country_code);

    ASSERT_EQ(kCurrencyCode, info[i].currency_code);
    ASSERT_EQ(kPrice, info[i].amount_micros);
  }

  GetCache().RemoveRef(GURL(kProductUrl));
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
                                const std::optional<const ProductInfo>& info) {
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
             const std::optional<const ProductInfo>& info) {
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
                                const std::optional<const ProductInfo>& info) {
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
  NiceMockWebWrapper web1(GURL(url), false);
  NiceMockWebWrapper web2(GURL(url), false);

  std::string url2 = "http://example.com/bar";
  NiceMockWebWrapper web3(GURL(url2), false);

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

// Ensure we keep track of live web wrappers.
TEST_P(ShoppingServiceTest, TestWebWrapperSet) {
  test_features_.InitWithFeatures({kShoppingList}, {});

  std::string url1 = "http://example.com/foo";
  std::u16string title1 = u"example1";
  NiceMockWebWrapper web1(GURL(url1), false, nullptr, title1);

  UrlInfo url_info1;
  url_info1.url = GURL(url1);
  url_info1.title = title1;

  std::string url2 = "http://example.com/bar";
  std::u16string title2 = u"example2";
  NiceMockWebWrapper web2(GURL(url2), false, nullptr, title2);

  UrlInfo url_info2;
  url_info2.url = GURL(url2);
  url_info2.title = title2;

  std::string url3 = "http://example.com/baz";
  std::u16string title3 = u"example3";
  NiceMockWebWrapper web3(GURL(url3), false, nullptr, title3);

  UrlInfo url_info3;
  url_info3.url = GURL(url3);
  url_info3.title = title3;

  ASSERT_TRUE(shopping_service_->GetUrlInfosForActiveWebWrappers().empty());

  WebWrapperCreated(&web1);
  WebWrapperCreated(&web2);
  WebWrapperCreated(&web3);

  std::vector<UrlInfo> open_urls =
      shopping_service_->GetUrlInfosForActiveWebWrappers();

  ASSERT_EQ(3u, open_urls.size());
  ASSERT_TRUE(base::Contains(open_urls, url_info1));
  ASSERT_TRUE(base::Contains(open_urls, url_info2));
  ASSERT_TRUE(base::Contains(open_urls, url_info3));

  // Close one of the tabs
  WebWrapperDestroyed(&web1);

  open_urls = shopping_service_->GetUrlInfosForActiveWebWrappers();
  ASSERT_EQ(2u, open_urls.size());
  ASSERT_FALSE(base::Contains(open_urls, url_info1));
  ASSERT_TRUE(base::Contains(open_urls, url_info2));
  ASSERT_TRUE(base::Contains(open_urls, url_info3));

  WebWrapperDestroyed(&web2);
  WebWrapperDestroyed(&web3);

  ASSERT_TRUE(shopping_service_->GetUrlInfosForActiveWebWrappers().empty());
}

// Ensure we correctly identify open tabs with products.
TEST_P(ShoppingServiceTest, TestWebWrapperSetWithProducts) {
  test_features_.InitWithFeatures({kShoppingList}, {});

  std::string url1 = "http://example.com/product";
  std::u16string title1 = u"Product";
  NiceMockWebWrapper web1(GURL(url1), false, nullptr, title1);

  UrlInfo url_info1;
  url_info1.url = GURL(url1);
  url_info1.title = title1;

  OptimizationMetadata meta = opt_guide_->BuildPriceTrackingResponse(
      kTitle, kImageUrl, kOfferId, kClusterId, kCountryCode, kPrice,
      kCurrencyCode);
  opt_guide_->SetResponse(GURL(url1), OptimizationType::PRICE_TRACKING,
                          OptimizationGuideDecision::kTrue, meta);
  opt_guide_->AddOnDemandShoppingResponse(
      GURL(url1), OptimizationGuideDecision::kTrue, meta);

  std::string url2 = "http://example.com/non_product";
  std::u16string title2 = u"Non-product";
  NiceMockWebWrapper web2(GURL(url2), false, nullptr, title2);

  UrlInfo url_info2;
  url_info2.url = GURL(url2);
  url_info2.title = title2;

  WebWrapperCreated(&web1);
  WebWrapperCreated(&web2);

  base::RunLoop run_loop;
  shopping_service_->GetUrlInfosForWebWrappersWithProducts(
      base::BindOnce([](const std::vector<UrlInfo> product_tabs) {
        ASSERT_EQ(1u, product_tabs.size());
        ASSERT_EQ("http://example.com/product", product_tabs[0].url.spec());
      }).Then(run_loop.QuitClosure()));
  run_loop.Run();

  WebWrapperDestroyed(&web1);
  WebWrapperDestroyed(&web2);

  base::RunLoop final_run_loop;
  shopping_service_->GetUrlInfosForWebWrappersWithProducts(
      base::BindOnce([](const std::vector<UrlInfo> product_tabs) {
        ASSERT_TRUE(product_tabs.empty());
      }).Then(final_run_loop.QuitClosure()));
  final_run_loop.Run();
}

// Make sure recent URLs doesn't contain duplicates.
TEST_P(ShoppingServiceTest, TestRecentUrls_NoDuplicates) {
  std::string url1 = "http://example.com/foo";
  NiceMockWebWrapper web1(GURL(url1), false);
  std::string url2 = "http://example.com/bar";
  NiceMockWebWrapper web2(GURL(url2), false);

  ASSERT_EQ(
      0u, shopping_service_->GetUrlInfosForRecentlyViewedWebWrappers().size());

  OnWebWrapperSwitched(&web1);

  std::vector<UrlInfo> urls =
      shopping_service_->GetUrlInfosForRecentlyViewedWebWrappers();
  ASSERT_EQ(1u, urls.size());
  ASSERT_EQ(urls[0].url, GURL(url1));

  OnWebWrapperSwitched(&web2);

  urls = shopping_service_->GetUrlInfosForRecentlyViewedWebWrappers();
  ASSERT_EQ(2u, urls.size());
  ASSERT_EQ(urls[0].url, GURL(url2));
  ASSERT_EQ(urls[1].url, GURL(url1));

  // Adding the first URL again should move that url to the head of the list.
  // There should still only be one instance.
  OnWebWrapperSwitched(&web1);

  urls = shopping_service_->GetUrlInfosForRecentlyViewedWebWrappers();
  ASSERT_EQ(2u, urls.size());
  ASSERT_EQ(urls[0].url, GURL(url1));
  ASSERT_EQ(urls[1].url, GURL(url2));
}

// Make sure recent URLs doesn't go over the max size.
TEST_P(ShoppingServiceTest, TestRecentUrls_MaxCount) {
  for (int i = 0; i < 20; i++) {
    NiceMockWebWrapper wrapper = NiceMockWebWrapper(
        GURL("http://example.com/" + base::NumberToString(i)), false);
    OnWebWrapperSwitched(&wrapper);
  }

  std::string url1 = "http://example.com/foo";
  NiceMockWebWrapper web1(GURL(url1), false);
  std::string url2 = "http://example.com/bar";
  NiceMockWebWrapper web2(GURL(url2), false);

  ASSERT_EQ(
      10u, shopping_service_->GetUrlInfosForRecentlyViewedWebWrappers().size());
}

TEST_P(ShoppingServiceTest, TestRecentUrls_ClearedOnHistoryDeletion) {
  std::string url1 = "http://example.com/foo";
  MockWebWrapper web1(GURL(url1), false);
  std::string url2 = "http://example.com/bar";
  MockWebWrapper web2(GURL(url2), false);

  ASSERT_EQ(
      0u, shopping_service_->GetUrlInfosForRecentlyViewedWebWrappers().size());

  OnWebWrapperSwitched(&web1);
  OnWebWrapperSwitched(&web2);

  std::vector<UrlInfo> urls =
      shopping_service_->GetUrlInfosForRecentlyViewedWebWrappers();
  ASSERT_EQ(2u, urls.size());
  ASSERT_EQ(urls[0].url, GURL(url2));
  ASSERT_EQ(urls[1].url, GURL(url1));

  // Fake an event from the history service for deleting all history entries.
  history::DeletionInfo deletion_info(history::DeletionTimeRange::AllTime(),
                                      false, {}, {}, {});
  shopping_service_->OnHistoryDeletions(nullptr, deletion_info);

  // After history deletion the recently viewed tab list should be empty.
  ASSERT_EQ(
      0u, shopping_service_->GetUrlInfosForRecentlyViewedWebWrappers().size());
}

TEST_P(ShoppingServiceTest, TestRecentUrls_SingleHistoryItemDeletion) {
  std::string url1 = "http://example.com/foo";
  MockWebWrapper web1(GURL(url1), false);
  std::string url2 = "http://example.com/bar";
  MockWebWrapper web2(GURL(url2), false);

  ASSERT_EQ(
      0u, shopping_service_->GetUrlInfosForRecentlyViewedWebWrappers().size());

  OnWebWrapperSwitched(&web1);
  OnWebWrapperSwitched(&web2);

  std::vector<UrlInfo> urls =
      shopping_service_->GetUrlInfosForRecentlyViewedWebWrappers();
  ASSERT_EQ(2u, urls.size());
  ASSERT_EQ(urls[0].url, GURL(url2));
  ASSERT_EQ(urls[1].url, GURL(url1));

  // Fake an event from the history service for deleting a single entry.
  history::DeletionInfo deletion_info(history::DeletionTimeRange::Invalid(),
                                      false, {history::URLRow(GURL(url1))}, {},
                                      {});
  shopping_service_->OnHistoryDeletions(nullptr, deletion_info);

  // After history deletion the recently viewed tab list should be empty.
  // Deletion of any item should result in clearing all.
  ASSERT_EQ(
      0u, shopping_service_->GetUrlInfosForRecentlyViewedWebWrappers().size());
}

TEST_P(ShoppingServiceTest, TestRecentUrls_CacheEntriesRetained) {
  const size_t max_recents = 10;
  std::vector<std::unique_ptr<NiceMockWebWrapper>> web_wrappers;
  for (size_t i = 0; i < max_recents; i++) {
    web_wrappers.push_back(std::make_unique<NiceMockWebWrapper>(
        GURL("http://example.com/" + base::NumberToString(i)), false));
    OnWebWrapperSwitched(web_wrappers[i].get());
  }

  for (const auto& web_wrapper : web_wrappers) {
    ASSERT_TRUE(GetCache().IsUrlReferenced(web_wrapper->GetLastCommittedURL()));
  }

  // Add more URLs to push the originals out.
  for (size_t i = max_recents; i < max_recents * 2; i++) {
    web_wrappers.push_back(std::make_unique<NiceMockWebWrapper>(
        GURL("http://example.com/" + base::NumberToString(i)), false));
    OnWebWrapperSwitched(web_wrappers[i].get());
  }

  // The first set of web wrapper URLs should no longer be in the cache, but
  // the second set should.
  for (size_t i = 0; i < web_wrappers.size(); i++) {
    if (i < max_recents) {
      ASSERT_FALSE(
          GetCache().IsUrlReferenced(web_wrappers[i]->GetLastCommittedURL()));
    } else {
      ASSERT_TRUE(
          GetCache().IsUrlReferenced(web_wrappers[i]->GetLastCommittedURL()));
    }
  }
}

// Ensure the cache maintained by the service is observing changes in the URLs
// stored in each ProductSpecificationsSet.
TEST_P(ShoppingServiceTest, TestProductSpecificationsSetUrlsRetained) {
  ProductSpecificationsSet::Observer* observer =
      GetProductSpecServiceUrlRefObserver();
  ASSERT_FALSE(observer == nullptr);

  const GURL url1("http://example.com/1");
  const GURL url2("http://example.com/2");
  base::Uuid id = base::Uuid::GenerateRandomV4();

  ProductSpecificationsSet prod_spec_set(id.AsLowercaseString(), 0, 0, {url1},
                                         "specs");

  observer->OnProductSpecificationsSetAdded(prod_spec_set);

  // The URLs in the set should be referenced in the cache.
  ASSERT_EQ(1u, GetCache().GetUrlRefCount(url1));
  ASSERT_EQ(0u, GetCache().GetUrlRefCount(url2));

  ProductSpecificationsSet updated_prod_spec_set(id.AsLowercaseString(), 0, 0,
                                                 {url1, url2}, "specs");

  // The updated set is considered the same since the UUID matches the previous.
  observer->OnProductSpecificationsSetUpdate(prod_spec_set,
                                             updated_prod_spec_set);

  // The existing URL count should not have changed and the new one should be
  // added.
  ASSERT_EQ(1u, GetCache().GetUrlRefCount(url1));
  ASSERT_EQ(1u, GetCache().GetUrlRefCount(url2));

  ProductSpecificationsSet updated_prod_spec_set2(id.AsLowercaseString(), 0, 0,
                                                  {url2}, "specs");

  observer->OnProductSpecificationsSetUpdate(updated_prod_spec_set,
                                             updated_prod_spec_set2);

  ASSERT_EQ(0u, GetCache().GetUrlRefCount(url1));
  ASSERT_EQ(1u, GetCache().GetUrlRefCount(url2));

  observer->OnProductSpecificationsSetRemoved(updated_prod_spec_set2);

  // There should no longer be any references in the cache.
  ASSERT_EQ(0u, GetCache().GetUrlRefCount(url1));
  ASSERT_EQ(0u, GetCache().GetUrlRefCount(url2));
}

TEST_P(ShoppingServiceTest, TestRecentTabsCleanedUpOnDeletion) {
  const GURL url("http://example.com/");
  base::Uuid id = base::Uuid::GenerateRandomV4();

  ProductSpecificationsSet spec_set(id.AsLowercaseString(), 0, 0, {url},
                                    "specs");

  EXPECT_CALL(*GetMockTabRestoreService(), DeleteNavigationEntries).Times(1);
  ON_CALL(*GetMockTabRestoreService(), DeleteNavigationEntries)
      .WillByDefault(
          [spec_set = spec_set](
              const sessions::TabRestoreService::DeletionPredicate& predicate) {
            // A totally unrelated URL should be ignored.
            sessions::SerializedNavigationEntry entry1;
            entry1.set_virtual_url(GURL("http://example.com"));
            ASSERT_FALSE(predicate.Run(entry1));

            // An exact match should be removed.
            sessions::SerializedNavigationEntry entry2;
            entry2.set_virtual_url(GetProductSpecsTabUrlForID(spec_set.uuid()));
            ASSERT_TRUE(predicate.Run(entry2));

            // A matching URL with extra params should be removed as well.
            sessions::SerializedNavigationEntry entry3;
            entry3.set_virtual_url(
                GURL(GetProductSpecsTabUrlForID(spec_set.uuid()).spec() +
                     "?param=1"));
            ASSERT_TRUE(predicate.Run(entry3));
          });

  shopping_service_->OnProductSpecificationsSetRemoved(spec_set);
}

TEST_P(ShoppingServiceTest, TestProductSpecificationsUrlCountMetrics) {
  test_features_.InitWithFeatures({commerce::kProductSpecifications}, {});
  sync_service_->GetUserSettings()->SetSelectedTypes(
      true, {syncer::UserSelectableType::kProductComparison});

  pref_service_->SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);

  SetTabCompareEnterprisePolicyPref(pref_service_.get(), 0);

  const GURL url1("http://example.com/1");
  const GURL url2("http://example.com/2");

  OptimizationMetadata meta = opt_guide_->BuildPriceTrackingResponse(
      kTitle, kImageUrl, kOfferId, kClusterId, kCountryCode, kPrice,
      kCurrencyCode, kGpcTitle, kProductCategories);
  opt_guide_->SetResponse(url1, OptimizationType::PRICE_TRACKING,
                          OptimizationGuideDecision::kTrue, meta);

  NiceMockWebWrapper web1(GURL(url1), false);
  DidNavigatePrimaryMainFrame(&web1);

  NiceMockWebWrapper web2(GURL(url2), false);
  DidNavigatePrimaryMainFrame(&web2);

  base::HistogramTester histogram_tester;
  base::RunLoop looper;
  shopping_service_->GetProductSpecificationsForUrls(
      {url1, url2},
      base::BindOnce([](std::vector<uint64_t> urls,
                        std::optional<ProductSpecifications> specs) {
      }).Then(looper.QuitClosure()));
  looper.Run();

  histogram_tester.ExpectTotalCount("Commerce.Compare.Table.ColumnCount", 1);
  histogram_tester.ExpectUniqueSample("Commerce.Compare.Table.ColumnCount", 2,
                                      1);
  histogram_tester.ExpectTotalCount(
      "Commerce.Compare.Table.PercentageValidProducts", 1);
  histogram_tester.ExpectUniqueSample(
      "Commerce.Compare.Table.PercentageValidProducts", 0.5f, 1);

  DidNavigatePrimaryMainFrame(&web1);
  DidNavigatePrimaryMainFrame(&web2);
}

// Test that product info is inserted into the cache without a client
// necessarily querying for it.
TEST_P(ShoppingServiceTest, TestProductInfoCacheFullLifecycle) {
  test_features_.InitWithFeatures(
      {kShoppingList, kCommerceAllowLocalImages, kCommerceAllowServerImages},
      {});

  NiceMockWebWrapper web(GURL(kProductUrl), false);

  OptimizationMetadata meta = opt_guide_->BuildPriceTrackingResponse(
      kTitle, kImageUrl, kOfferId, kClusterId, kCountryCode);

  opt_guide_->SetResponse(GURL(kProductUrl), OptimizationType::PRICE_TRACKING,
                          OptimizationGuideDecision::kTrue, meta);

  DidNavigatePrimaryMainFrame(&web);

  // By this point there should be something in the cache.
  ASSERT_EQ(1, GetProductInfoCacheOpenURLCount(GURL(kProductUrl)));

  // We should be able to access the cached data.
  std::optional<ProductInfo> cached_info =
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
                                const std::optional<const ProductInfo>& info) {
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

  auto result = base::Value::Dict();
  result.Set("image", std::string(kImageUrl));
  base::Value js_result(std::move(result));
  NiceMockWebWrapper web(GURL(kProductUrl), false, &js_result);

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
  std::optional<ProductInfo> cached_info =
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
                                const std::optional<const ProductInfo>& info) {
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
  // The local extraction should only be able to run now after all loading has
  // completed (for at least the timeout duration).
  SimulateProductInfoLocalExtractionTaskFinished();

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

  auto result = base::Value::Dict();
  result.Set("image", std::string(kImageUrl));
  base::Value js_result(std::move(result));
  NiceMockWebWrapper web(GURL(kProductUrl), false, &js_result);

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
  SimulateProductInfoLocalExtractionTaskFinished();

  // By this point there should be something in the cache.
  ASSERT_EQ(1, GetProductInfoCacheOpenURLCount(GURL(kProductUrl)));

  // We should be able to access the cached data.
  std::optional<ProductInfo> cached_info =
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
                                const std::optional<const ProductInfo>& info) {
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

// The on-demand api should not be triggered in the case where we have an
// explicit negative signal from the page.
TEST_P(ShoppingServiceTest, TestProductInfoWithFallback_NoOnDemandCalls) {
  NiceMockWebWrapper web(GURL(kProductUrl), false);

  // Assume the page has already loaded for the navigation. This is usually the
  // case for single-page webapps.
  web.SetIsFirstLoadForNavigationFinished(true);

  opt_guide_->SetResponse(GURL(kProductUrl), OptimizationType::PRICE_TRACKING,
                          OptimizationGuideDecision::kFalse,
                          OptimizationMetadata());

  // The on-demand api should only ever be called once in this test.
  EXPECT_CALL(*GetMockOptGuideDecider(), CanApplyOptimizationOnDemand).Times(0);

  DidNavigatePrimaryMainFrame(&web);
  // If the page was already loaded, assume the js has time to run now.
  SimulateProductInfoLocalExtractionTaskFinished();

  // By this point there should be something in the cache.
  ASSERT_EQ(1, GetProductInfoCacheOpenURLCount(GURL(kProductUrl)));

  // We should be able to access the cached data.
  CommerceInfoCache::CacheEntry* entry =
      GetCache().GetEntryForUrl(GURL(kProductUrl));
  ASSERT_FALSE(entry == nullptr);
  ASSERT_FALSE(entry->run_product_info_on_demand);

  // Querying for the info multiple times should not trigger the on-demand api.
  base::RunLoop run_loop;
  shopping_service_->GetProductInfoForUrl(
      GURL(kProductUrl), base::BindOnce(
                             [](base::RunLoop* run_loop, const GURL& url,
                                const std::optional<const ProductInfo>& info) {
                               ASSERT_EQ(kProductUrl, url.spec());
                               ASSERT_FALSE(info.has_value());
                               run_loop->Quit();
                             },
                             &run_loop));
  run_loop.Run();

  // Close the "tab" and make sure the cache is empty.
  WebWrapperDestroyed(&web);
  ASSERT_EQ(0, GetProductInfoCacheOpenURLCount(GURL(kProductUrl)));
}

// If there's a reference to the url in the cache and opt guide doesn't know
// about the url, we should be allowed to call the on-demand api.
TEST_P(ShoppingServiceTest,
       TestProductInfoWithFallback_CallsOnDemandOnce_RefInCache) {
  // The on-demand api should not be called if there isn't a page open and
  // there isn't a reference in the cache.
  EXPECT_CALL(*GetMockOptGuideDecider(), CanApplyOptimizationOnDemand).Times(0);

  opt_guide_->SetResponse(GURL(kProductUrl), OptimizationType::PRICE_TRACKING,
                          OptimizationGuideDecision::kUnknown,
                          OptimizationMetadata());

  base::RunLoop run_loop;
  shopping_service_->GetProductInfoForUrl(
      GURL(kProductUrl), base::BindOnce(
                             [](base::RunLoop* run_loop, const GURL& url,
                                const std::optional<const ProductInfo>& info) {
                               ASSERT_EQ(kProductUrl, url.spec());
                               ASSERT_FALSE(info.has_value());
                               run_loop->Quit();
                             },
                             &run_loop));
  run_loop.Run();

  // We shouldn't have a cache entry.
  ASSERT_TRUE(GetCache().GetEntryForUrl(GURL(kProductUrl)) == nullptr);

  // Add a ref to the url - this could be a page loading or another feature.
  GetCache().AddRef(GURL(kProductUrl));

  CommerceInfoCache::CacheEntry* entry =
      GetCache().GetEntryForUrl(GURL(kProductUrl));
  ASSERT_FALSE(entry == nullptr);
  ASSERT_TRUE(entry->run_product_info_on_demand);

  // We should now be allowed to call the on-demand api when product info is
  // requested.
  EXPECT_CALL(*GetMockOptGuideDecider(), CanApplyOptimizationOnDemand).Times(1);

  OptimizationMetadata meta = opt_guide_->BuildPriceTrackingResponse(
      kTitle, "", kOfferId, kClusterId, kCountryCode);
  opt_guide_->AddOnDemandShoppingResponse(
      GURL(kProductUrl), OptimizationGuideDecision::kTrue, meta);

  // By this point there should be something in the cache.
  ASSERT_EQ(1, GetProductInfoCacheOpenURLCount(GURL(kProductUrl)));

  // Querying for the info multiple times should not trigger the on-demand api.
  base::RunLoop run_loop2;
  shopping_service_->GetProductInfoForUrl(
      GURL(kProductUrl), base::BindOnce(
                             [](base::RunLoop* run_loop, const GURL& url,
                                const std::optional<const ProductInfo>& info) {
                               ASSERT_EQ(kProductUrl, url.spec());
                               ASSERT_TRUE(info.has_value());
                               run_loop->Quit();
                             },
                             &run_loop2));
  run_loop2.Run();
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
             std::optional<MerchantInfo> info) {
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
                                 std::optional<MerchantInfo> info) {
                                ASSERT_EQ(kMerchantUrl, url.spec());
                                ASSERT_FALSE(info.has_value());
                                run_loop->Quit();
                              },
                              &run_loop));
  run_loop.Run();
}

TEST_P(ShoppingServiceTest, TestGetUpdatedProductInfoForBookmarks) {
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
         std::optional<ProductInfo> info) {
        const bookmarks::BookmarkNode* node =
            bookmarks::GetBookmarkNodeByID(model, id);
        EXPECT_EQ(url.spec(), node->url().spec());

        (*call_count)--;
        if (*call_count <= 0) {
          run_loop->Quit();
        }
      },
      bookmark_model_.get(), &expected_calls, &run_loop);

  shopping_service_->GetUpdatedProductInfoForBookmarks(bookmark_ids, callback);
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
  checker.SetCountry(kEligibleCountry);
  checker.SetLocale(kEligibleLocale);
  checker.SetPrefs(&prefs);

  ASSERT_TRUE(IsShoppingListEligible(&checker));

  SetShoppingListEnterprisePolicyPref(&prefs, false);
  ASSERT_FALSE(IsShoppingListEligible(&checker));
}

TEST_P(ShoppingServiceTest, TestShoppingListEligible_FeatureFlagOff) {
  test_features_.InitWithFeatures({},
                                  {kShoppingList, kShoppingListRegionLaunched});

  TestingPrefServiceSimple prefs;
  RegisterPrefs(prefs.registry());
  SetShoppingListEnterprisePolicyPref(&prefs, true);

  MockAccountChecker checker;
  checker.SetCountry(kEligibleCountry);
  checker.SetLocale(kEligibleLocale);
  checker.SetPrefs(&prefs);

  ASSERT_FALSE(IsShoppingListEligible(&checker));
}

TEST_P(ShoppingServiceTest, TestShoppingListEligible_MSBB) {
  test_features_.InitWithFeatures({kShoppingList},
                                  {kShoppingListRegionLaunched});

  TestingPrefServiceSimple prefs;
  RegisterPrefs(prefs.registry());
  SetShoppingListEnterprisePolicyPref(&prefs, true);

  MockAccountChecker checker;
  checker.SetCountry(kEligibleCountry);
  checker.SetLocale(kEligibleLocale);
  checker.SetPrefs(&prefs);

  ASSERT_TRUE(IsShoppingListEligible(&checker));

  checker.SetAnonymizedUrlDataCollectionEnabled(false);

  ASSERT_FALSE(IsShoppingListEligible(&checker));
}

TEST_P(ShoppingServiceTest, TestShoppingListEligible_SignIn) {
  test_features_.InitWithFeatures({kShoppingList},
                                  {kShoppingListRegionLaunched});

  TestingPrefServiceSimple prefs;
  RegisterPrefs(prefs.registry());
  SetShoppingListEnterprisePolicyPref(&prefs, true);

  MockAccountChecker checker;
  checker.SetCountry(kEligibleCountry);
  checker.SetLocale(kEligibleLocale);
  checker.SetPrefs(&prefs);

  ASSERT_TRUE(IsShoppingListEligible(&checker));

  checker.SetSignedIn(false);

  ASSERT_FALSE(IsShoppingListEligible(&checker));
}

TEST_P(ShoppingServiceTest, TestShoppingListEligible_ChildAccount) {
  test_features_.InitWithFeatures({kShoppingList},
                                  {kShoppingListRegionLaunched});

  TestingPrefServiceSimple prefs;
  RegisterPrefs(prefs.registry());
  SetShoppingListEnterprisePolicyPref(&prefs, true);

  MockAccountChecker checker;
  checker.SetCountry(kEligibleCountry);
  checker.SetLocale(kEligibleLocale);
  checker.SetPrefs(&prefs);

  ASSERT_TRUE(IsShoppingListEligible(&checker));

  checker.SetIsSubjectToParentalControls(true);

  ASSERT_FALSE(IsShoppingListEligible(&checker));
}

TEST_P(ShoppingServiceTest, TestShoppingListEligible_SyncState) {
  test_features_.InitWithFeatures({kShoppingList},
                                  {kShoppingListRegionLaunched});

  TestingPrefServiceSimple prefs;
  RegisterPrefs(prefs.registry());
  SetShoppingListEnterprisePolicyPref(&prefs, true);

  MockAccountChecker checker;
  checker.SetCountry(kEligibleCountry);
  checker.SetLocale(kEligibleLocale);
  checker.SetPrefs(&prefs);

  ASSERT_TRUE(IsShoppingListEligible(&checker));

  checker.SetSyncingBookmarks(false);

  ASSERT_FALSE(IsShoppingListEligible(&checker));
}

TEST_P(ShoppingServiceTest, TestShoppingListEligible_CountryAndLocale) {
  test_features_.InitWithFeatures({kShoppingList},
                                  {kShoppingListRegionLaunched});

  TestingPrefServiceSimple prefs;
  RegisterPrefs(prefs.registry());
  SetShoppingListEnterprisePolicyPref(&prefs, true);

  MockAccountChecker checker;
  checker.SetCountry(kEligibleCountry);
  checker.SetLocale(kEligibleLocale);
  checker.SetPrefs(&prefs);

  ASSERT_TRUE(IsShoppingListEligible(&checker));

  checker.SetCountry("ZZ");
  checker.SetLocale("zz-zz");

  // This should continue to work since we can assume, for the sake of the test,
  // that the experiment config includes the ZZ country and zz-zz locale.
  ASSERT_TRUE(IsShoppingListEligible(&checker));
}

TEST_P(ShoppingServiceTest,
       TestShoppingListEligible_CountryAndLocale_BothFlags) {
  test_features_.InitWithFeatures({kShoppingList, kShoppingListRegionLaunched},
                                  {});

  TestingPrefServiceSimple prefs;
  RegisterPrefs(prefs.registry());
  SetShoppingListEnterprisePolicyPref(&prefs, true);

  MockAccountChecker checker;
  checker.SetCountry(kEligibleCountry);
  checker.SetLocale(kEligibleLocale);
  checker.SetPrefs(&prefs);

  ASSERT_TRUE(IsShoppingListEligible(&checker));

  checker.SetCountry("ZZ");
  checker.SetLocale("zz-zz");

  // Same as the previous test, this should still work since, presumably, the
  // experiment config for "ShoppingList" includes these.
  ASSERT_TRUE(IsShoppingListEligible(&checker));
}

TEST_P(ShoppingServiceTest, TestShoppingListEligible_CountryAndLocale_NoFlags) {
  test_features_.InitWithFeatures({},
                                  {kShoppingList, kShoppingListRegionLaunched});

  TestingPrefServiceSimple prefs;
  RegisterPrefs(prefs.registry());
  SetShoppingListEnterprisePolicyPref(&prefs, true);

  MockAccountChecker checker;

  ASSERT_FALSE(IsShoppingListEligible(&checker));

  checker.SetCountry("ZZ");
  checker.SetLocale("zz-zz");

  ASSERT_FALSE(IsShoppingListEligible(&checker));
}

TEST_P(ShoppingServiceTest,
       TestShoppingListEligible_CountryAndLocale_RegionLaunched) {
  test_features_.InitWithFeatures({kShoppingListRegionLaunched},
                                  {kShoppingList});

  TestingPrefServiceSimple prefs;
  RegisterPrefs(prefs.registry());
  SetShoppingListEnterprisePolicyPref(&prefs, true);

  MockAccountChecker checker;
  checker.SetCountry(kEligibleCountry);
  checker.SetLocale(kEligibleLocale);
  checker.SetPrefs(&prefs);

  ASSERT_TRUE(IsShoppingListEligible(&checker));

  checker.SetCountry("ZZ");
  checker.SetLocale("zz-zz");

  // If we only have the region flag enabled, we should be restricted to
  // specific countries and locales. The fake country and locale below should
  // be blocked.
  ASSERT_FALSE(IsShoppingListEligible(&checker));
}

class ShoppingServiceReadyTest : public ShoppingServiceTest {
 public:
  ShoppingServiceReadyTest()
      : ShoppingServiceTest(syncer::SyncService::TransportState::INITIALIZING) {
  }
  ShoppingServiceReadyTest(const ShoppingServiceReadyTest&) = delete;
  ShoppingServiceReadyTest operator=(const ShoppingServiceReadyTest&) = delete;
  ~ShoppingServiceReadyTest() override = default;
};

TEST_P(ShoppingServiceReadyTest,
       TestServiceReadyDelaysForSync_TransportStartsInactive) {
  test_features_.InitWithFeatures({kShoppingList}, {});

  bool service_ready = false;
  shopping_service_->WaitForReady(
      base::BindOnce([](bool* service_ready,
                        ShoppingService* service) { *service_ready = true; },
                     &service_ready));

  base::RunLoop().RunUntilIdle();

  // The ready check shouldn't have run since transport state is INITIALIZING.
  ASSERT_FALSE(service_ready);

  sync_service_->SetMaxTransportState(
      syncer::SyncService::TransportState::ACTIVE);
  sync_service_->FireStateChanged();

  base::RunLoop().RunUntilIdle();

  // The run loop should be finished now.
  ASSERT_TRUE(service_ready);
}

TEST_P(ShoppingServiceReadyTest,
       TestServiceReadyDelaysForSync_TransportStartsActive) {
  test_features_.InitWithFeatures({kShoppingList}, {});

  sync_service_->SetMaxTransportState(
      syncer::SyncService::TransportState::ACTIVE);
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
             const std::optional<PriceInsightsInfo>& info) {
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
             const std::optional<PriceInsightsInfo>& info) {
            ASSERT_EQ(kPriceInsightsUrl, url.spec());
            ASSERT_TRUE(info.has_value());

            ASSERT_EQ(kClusterId, info->product_cluster_id);
            ASSERT_EQ(kCurrencyCode, info->currency_code);
            ASSERT_EQ(kLowTypicalPrice, info->typical_low_price_micros);
            ASSERT_EQ(kHighTypicalPrice, info->typical_high_price_micros);
            ASSERT_EQ(std::nullopt, info->catalog_attributes);
            ASSERT_EQ(0, (int)(info->catalog_history_prices.size()));
            ASSERT_EQ(std::nullopt, info->jackpot_url);
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
             const std::optional<PriceInsightsInfo>& info) {
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
             const std::optional<PriceInsightsInfo>& info) {
            ASSERT_EQ(kPriceInsightsUrl, url.spec());
            ASSERT_TRUE(info.has_value());

            ASSERT_EQ(kClusterId, info->product_cluster_id);
            ASSERT_EQ(kCurrencyCode, info->currency_code);
            ASSERT_EQ(std::nullopt, info->typical_low_price_micros);
            ASSERT_EQ(std::nullopt, info->typical_high_price_micros);
            ASSERT_EQ(kAttributes, info->catalog_attributes);
            ASSERT_EQ(2, (int)(info->catalog_history_prices.size()));
            ASSERT_EQ("2021-01-01",
                      std::get<0>(info->catalog_history_prices[0]));
            ASSERT_EQ("2021-01-02",
                      std::get<0>(info->catalog_history_prices[1]));
            ASSERT_EQ(100, std::get<1>(info->catalog_history_prices[0]));
            ASSERT_EQ(200, std::get<1>(info->catalog_history_prices[1]));
            ASSERT_EQ(std::nullopt, info->jackpot_url);
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
             const std::optional<PriceInsightsInfo>& info) {
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
             const std::optional<PriceInsightsInfo>& info) {
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
             const std::optional<PriceInsightsInfo>& info) {
            ASSERT_EQ(kPriceInsightsUrl, url.spec());
            ASSERT_TRUE(info.has_value());
            ASSERT_EQ(kClusterId, info->product_cluster_id);
            run_loop->Quit();
          },
          &run_loop1));
  run_loop1.Run();

  // Simulate that the OptGuide result is cleared after some time but saved in
  // cache already.
  NiceMockWebWrapper web(GURL(kPriceInsightsUrl), false);
  DidNavigatePrimaryMainFrame(&web);

  opt_guide_->SetResponse(
      GURL(kPriceInsightsUrl), OptimizationType::PRICE_INSIGHTS,
      OptimizationGuideDecision::kTrue, OptimizationMetadata());
  base::RunLoop run_loop2;
  shopping_service_->GetPriceInsightsInfoForUrl(
      GURL(kPriceInsightsUrl),
      base::BindOnce(
          [](base::RunLoop* run_loop, const GURL& url,
             const std::optional<PriceInsightsInfo>& info) {
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
             const std::optional<PriceInsightsInfo>& info) {
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

  NiceMockWebWrapper web1(GURL(kPriceInsightsUrl), false);
  DidNavigatePrimaryMainFrame(&web1);

  base::RunLoop run_loop1;
  shopping_service_->GetPriceInsightsInfoForUrl(
      GURL(kPriceInsightsUrl),
      base::BindOnce(
          [](base::RunLoop* run_loop, const GURL& url,
             const std::optional<PriceInsightsInfo>& info) {
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
  NiceMockWebWrapper web2(GURL(kPriceInsightsUrl), false);
  DidNavigatePrimaryMainFrame(&web2);

  // Navigating away from one tab should not clear the cache.
  DidNavigateAway(&web1, GURL(kPriceInsightsUrl));

  base::RunLoop run_loop2;
  shopping_service_->GetPriceInsightsInfoForUrl(
      GURL(kPriceInsightsUrl),
      base::BindOnce(
          [](base::RunLoop* run_loop, const GURL& url,
             const std::optional<PriceInsightsInfo>& info) {
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
             const std::optional<PriceInsightsInfo>& info) {
            ASSERT_EQ(kPriceInsightsUrl, url.spec());
            ASSERT_FALSE(info.has_value());
            run_loop->Quit();
          },
          &run_loop3));
  run_loop3.Run();
}

TEST_P(ShoppingServiceTest, TestIsShoppingPage) {
  opt_guide_->SetDefaultShoppingPage(false);
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
                                std::optional<bool> info) {
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
                                std::optional<bool> info) {
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
                                std::optional<bool> info) {
                               ASSERT_TRUE(info.has_value());
                               ASSERT_FALSE(info.value());
                               run_loop->Quit();
                             },
                             &run_loop[2]));
  run_loop[2].Run();
}

TEST_P(ShoppingServiceTest, TestDiscountInfoResponse) {
  test_features_.InitWithFeatures({kEnableDiscountInfoApi}, {});

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

  opt_guide_->SetResponse(GURL(kDiscountsUrl1),
                          OptimizationType::SHOPPING_DISCOUNTS,
                          OptimizationGuideDecision::kTrue,
                          opt_guide_->BuildDiscountsResponse(infos));

  std::unique_ptr<MockDiscountsStorage> storage =
      std::make_unique<MockDiscountsStorage>();
  EXPECT_CALL(*storage, HandleServerDiscounts(GURL(kDiscountsUrl1), _, _));
  SetDiscountsStorageForTesting(std::move(storage));

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(kDiscountsFetchResultHistogramName, 0);

  base::RunLoop run_loop;
  shopping_service_->GetDiscountInfoForUrl(
      GURL(kDiscountsUrl1),
      base::BindOnce(
          [](base::RunLoop* run_loop, const GURL& url,
             const std::vector<DiscountInfo> discounts) {
            ASSERT_EQ(1, (int)discounts.size());

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

            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();

  histogram_tester.ExpectTotalCount(kDiscountsFetchResultHistogramName, 1);
  histogram_tester.ExpectBucketCount(kDiscountsFetchResultHistogramName, 0, 1);
}

TEST_P(ShoppingServiceTest, TestDiscountInfoResponse_InfoWithoutId) {
  test_features_.InitWithFeatures({kEnableDiscountInfoApi}, {});

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

  opt_guide_->SetResponse(GURL(kDiscountsUrl1),
                          OptimizationType::SHOPPING_DISCOUNTS,
                          OptimizationGuideDecision::kTrue,
                          opt_guide_->BuildDiscountsResponse(infos));

  base::RunLoop run_loop;
  shopping_service_->GetDiscountInfoForUrl(
      GURL(kDiscountsUrl1), base::BindOnce(
                                [](base::RunLoop* run_loop, const GURL& url,
                                   const std::vector<DiscountInfo> discounts) {
                                  ASSERT_EQ(1, (int)discounts.size());
                                  ASSERT_EQ(kDiscountId1, discounts[0].id);
                                  run_loop->Quit();
                                },
                                &run_loop));
  run_loop.Run();
}

TEST_P(ShoppingServiceTest, TestDiscountInfoResponse_InfoWithoutTerms) {
  test_features_.InitWithFeatures({kEnableDiscountInfoApi}, {});

  std::vector<DiscountInfo> infos;

  // Valid info.
  DiscountInfo valid_info;
  valid_info.cluster_type = DiscountClusterType::kOfferLevel;
  valid_info.type = DiscountType::kFreeListingWithCode;
  valid_info.language_code = kDiscountLanguageCode;
  valid_info.description_detail = kDiscountDetail;
  valid_info.terms_and_conditions = std::nullopt;
  valid_info.value_in_text = kDiscountValueText;
  valid_info.discount_code = kDiscountCode;
  valid_info.id = kDiscountId1;
  valid_info.is_merchant_wide = true;
  valid_info.expiry_time_sec = kDiscountExpiryTime;
  valid_info.offer_id = kDiscountOfferId;
  infos.push_back(valid_info);

  opt_guide_->SetResponse(GURL(kDiscountsUrl1),
                          OptimizationType::SHOPPING_DISCOUNTS,
                          OptimizationGuideDecision::kTrue,
                          opt_guide_->BuildDiscountsResponse(infos));

  base::RunLoop run_loop;
  shopping_service_->GetDiscountInfoForUrl(
      GURL(kDiscountsUrl1),
      base::BindOnce(
          [](base::RunLoop* run_loop, const GURL& key,
             const std::vector<DiscountInfo> discounts) {
            ASSERT_EQ(1, (int)discounts.size());
            ASSERT_EQ(kDiscountId1, discounts[0].id);
            ASSERT_FALSE(discounts[0].terms_and_conditions.has_value());
            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();
}

TEST_P(ShoppingServiceTest, TestDiscountInfoResponse_InfoWithoutDiscountCode) {
  test_features_.InitWithFeatures({kEnableDiscountInfoApi}, {});

  std::vector<DiscountInfo> infos;

  // Invalid info without discount code.
  DiscountInfo invalid_info;
  invalid_info.cluster_type = DiscountClusterType::kOfferLevel;
  invalid_info.type = DiscountType::kFreeListingWithCode;
  invalid_info.language_code = kDiscountLanguageCode;
  invalid_info.description_detail = kDiscountDetail;
  invalid_info.terms_and_conditions = kDiscountTerms;
  invalid_info.value_in_text = kDiscountValueText;
  invalid_info.discount_code = std::nullopt;
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
  shopping_service_->GetDiscountInfoForUrl(
      GURL(kDiscountsUrl1), base::BindOnce(
                                [](base::RunLoop* run_loop, const GURL& key,
                                   const std::vector<DiscountInfo> discounts) {
                                  ASSERT_EQ(0, (int)discounts.size());
                                  run_loop->Quit();
                                },
                                &run_loop));
  run_loop.Run();
}

TEST_P(ShoppingServiceTest, TestProductSpecificationsCache) {
  test_features_.InitWithFeatures({kProductSpecifications}, {});

  const GURL url("http://example.com");
  OptimizationMetadata meta = opt_guide_->BuildPriceTrackingResponse(
      kTitle, kImageUrl, kOfferId, kClusterId, kCountryCode, kPrice,
      kCurrencyCode, kGpcTitle, kProductCategories);
  opt_guide_->SetResponse(url, OptimizationType::PRICE_TRACKING,
                          OptimizationGuideDecision::kTrue, meta);

  ProductSpecifications specs;
  specs.product_dimension_map[1L] = kTitle;
  MockProductSpecificationsServerProxy* mock_server_proxy =
      new MockProductSpecificationsServerProxy();
  mock_server_proxy->SetGetProductSpecificationsForClusterIdsResponse(
      std::move(specs));
  SetProductSpecificationsServerProxy(base::WrapUnique(mock_server_proxy));

  base::RunLoop run_loop[2];

  shopping_service_->GetProductSpecificationsForUrls(
      {url}, base::BindOnce([](std::vector<uint64_t> cluster_ids,
                               std::optional<ProductSpecifications> specs) {
             }).Then(run_loop[0].QuitClosure()));
  run_loop[0].Run();

  // The first product specs should be cached, so this empty value should not be
  // returned.
  mock_server_proxy->SetGetProductSpecificationsForClusterIdsResponse(
      std::nullopt);

  shopping_service_->GetProductSpecificationsForUrls(
      {url}, base::BindOnce([](std::vector<uint64_t> cluster_ids,
                               std::optional<ProductSpecifications> specs) {
               ASSERT_TRUE(specs.has_value());
               ASSERT_TRUE(specs->product_dimension_map[1L] == kTitle);
             }).Then(run_loop[1].QuitClosure()));
  run_loop[1].Run();
}

INSTANTIATE_TEST_SUITE_P(All, ShoppingServiceTest, ::testing::Bool());

}  // namespace commerce
